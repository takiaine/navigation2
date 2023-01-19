// Copyright (c) 2023, Samsung Research America
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <memory>
#include <string>

#include "nav2_route/plugins/edge_cost_functions/costmap_scorer.hpp"

namespace nav2_route
{

void CostmapScorer::configure(
  const rclcpp_lifecycle::LifecycleNode::SharedPtr & node,
  const std::string & name)
{
  name_ = name;

  // Find whether to use average or maximum cost values
  nav2_util::declare_parameter_if_not_declared(
    node, getName() + ".use_maximum", rclcpp::ParameterValue(true));
  use_max_ = static_cast<float>(node->get_parameter(getName() + ".use_maximum").as_bool());

  // Edge is invalid if its in collision
  nav2_util::declare_parameter_if_not_declared(
    node, getName() + ".invalid_on_collision", rclcpp::ParameterValue(true));
  invalid_on_collision_ =
    static_cast<float>(node->get_parameter(getName() + ".invalid_on_collision").as_bool());

  // Edge is invalid if edge is off the costmap
  nav2_util::declare_parameter_if_not_declared(
    node, getName() + ".invalid_off_map", rclcpp::ParameterValue(true));
  invalid_off_map_ =
    static_cast<float>(node->get_parameter(getName() + ".invalid_off_map").as_bool());

  // Create costmap subscriber
  nav2_util::declare_parameter_if_not_declared(
    node, getName() + ".costmap_topic",
    rclcpp::ParameterValue(std::string("global_costmap/costmap_raw")));
  std::string costmap_topic =
    node->get_parameter(getName() + ".costmap_topic").as_string();
  costmap_subscriber_ = std::make_unique<nav2_costmap_2d::CostmapSubscriber>(node, costmap_topic);

  // Find the proportional weight to apply, if multiple cost functions
  nav2_util::declare_parameter_if_not_declared(
    node, getName() + ".weight", rclcpp::ParameterValue(1.0));
  weight_ = static_cast<float>(node->get_parameter(getName() + ".weight").as_double());
}

void CostmapScorer::prepare()
{
  try {
    costmap_ = costmap_subscriber_->getCostmap();
  } catch (...) {
    costmap_.reset();
  }
}

// TODO(sm) does this critic make efficiency sense at a
// reasonable sized graph / node distance? Lower iterator density?
bool CostmapScorer::score(const EdgePtr edge, float & cost)
{
  // If we don't have costmap information, all edges are invalid for safety
  if (!costmap_) {
    return false;
  }

  float largest_cost = 0.0;
  float running_cost = 0.0;
  float point_cost = 0.0;
  unsigned int idx = 0;

  unsigned int x0, y0, x1, y1;
  if (!costmap_->worldToMap(edge->start->coords.x, edge->start->coords.y, x0, y0) ||
    !costmap_->worldToMap(edge->end->coords.x, edge->end->coords.y, x1, y1))
  {
    if (invalid_off_map_) {
      return false;
    }
    return true;
  }

  for (nav2_util::LineIterator iter(x0, y0, x1, y1); iter.isValid(); iter.advance()) {
    point_cost = static_cast<float>(costmap_->getCost(iter.getX(), iter.getY()));

    // if in collision, no need to continue
    if (point_cost == 254.0 && invalid_on_collision_) {
      return false;
    }

    idx++;
    running_cost += point_cost;
    if (largest_cost < point_cost) {
      largest_cost = point_cost;
    }
  }


  if (use_max_) {
    cost = weight_ * largest_cost / 254.0 /*normalization*/;
  } else {
    cost = weight_ * running_cost / (static_cast<float>(idx) * 254.0) /*normalization*/;
  }

  return true;
}

std::string CostmapScorer::getName()
{
  return name_;
}

}  // namespace nav2_route

#include "pluginlib/class_list_macros.hpp"
PLUGINLIB_EXPORT_CLASS(nav2_route::CostmapScorer, nav2_route::EdgeCostFunction)
