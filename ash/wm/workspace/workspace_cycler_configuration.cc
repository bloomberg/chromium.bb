// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/workspace_cycler_configuration.h"

#include "ash/ash_switches.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/values.h"

namespace {

ListValue* g_shallower_than_selected_y_offsets = NULL;
ListValue* g_deeper_than_selected_y_offsets = NULL;
double g_selected_y_offset = 40;
double g_selected_scale = 0.95;
double g_min_scale = 0.9;
double g_max_scale = 1.0;
double g_min_brightness = -0.4;
double g_background_opacity = 0.8;
double g_desktop_workspace_brightness = -0.4;
double g_distance_to_initiate_cycling = 10;
double g_scroll_amount_to_cycle_to_next_workspace = 10;
double g_cycler_step_animation_duration_ratio = 10;
double g_start_cycler_animation_duration = 100;
double g_stop_cycler_animation_duration = 100;

}  // namespace

namespace ash {

// static
bool WorkspaceCyclerConfiguration::IsCyclerEnabled() {
  return CommandLine::ForCurrentProcess()->HasSwitch(
      ash::switches::kAshEnableWorkspaceScrubbing);
}

// static
bool WorkspaceCyclerConfiguration::IsListProperty(Property property) {
  return (property == SHALLOWER_THAN_SELECTED_Y_OFFSETS ||
          property == DEEPER_THAN_SELECTED_Y_OFFSETS);
}

// static
void WorkspaceCyclerConfiguration::SetListValue(Property property,
                                                const ListValue& list_value) {
  DCHECK(IsListProperty(property));
  if (property == SHALLOWER_THAN_SELECTED_Y_OFFSETS)
    g_shallower_than_selected_y_offsets = list_value.DeepCopy();
  else if (property == DEEPER_THAN_SELECTED_Y_OFFSETS)
    g_deeper_than_selected_y_offsets = list_value.DeepCopy();
}

// static
void WorkspaceCyclerConfiguration::SetDouble(Property property, double value) {
  switch (property) {
    case SHALLOWER_THAN_SELECTED_Y_OFFSETS:
    case DEEPER_THAN_SELECTED_Y_OFFSETS:
      NOTREACHED();
      break;
    case SELECTED_Y_OFFSET:
      g_selected_y_offset = value;
      break;
    case SELECTED_SCALE:
      g_selected_scale = value;
      break;
    case MIN_SCALE:
      g_min_scale = value;
      break;
    case MAX_SCALE:
      g_max_scale = value;
      break;
    case MIN_BRIGHTNESS:
      g_min_brightness = value;
      break;
    case BACKGROUND_OPACITY:
      g_background_opacity = value;
      break;
    case DESKTOP_WORKSPACE_BRIGHTNESS:
      g_desktop_workspace_brightness = value;
      break;
    case DISTANCE_TO_INITIATE_CYCLING:
      g_distance_to_initiate_cycling = value;
      break;
    case SCROLL_DISTANCE_TO_CYCLE_TO_NEXT_WORKSPACE:
      g_scroll_amount_to_cycle_to_next_workspace = value;
      break;
    case CYCLER_STEP_ANIMATION_DURATION_RATIO:
      g_cycler_step_animation_duration_ratio = value;
      break;
    case START_CYCLER_ANIMATION_DURATION:
      g_start_cycler_animation_duration = value;
      break;
    case STOP_CYCLER_ANIMATION_DURATION:
      g_stop_cycler_animation_duration = value;
      break;
  }
}

// static
const ListValue& WorkspaceCyclerConfiguration::GetListValue(Property property) {
  DCHECK(IsListProperty(property));
  if (property == SHALLOWER_THAN_SELECTED_Y_OFFSETS) {
    if (!g_shallower_than_selected_y_offsets) {
      // First time the property is accessed. Initialize it to the default
      // value.
      g_shallower_than_selected_y_offsets = new base::ListValue();
      g_shallower_than_selected_y_offsets->AppendDouble(-40);
      g_shallower_than_selected_y_offsets->AppendDouble(-32);
      g_shallower_than_selected_y_offsets->AppendDouble(-20);
    }
    return *g_shallower_than_selected_y_offsets;
  } else if (property == DEEPER_THAN_SELECTED_Y_OFFSETS) {
    if (!g_deeper_than_selected_y_offsets) {
      // First time the property is accessed. Initialize it to the default
      // value.
      g_deeper_than_selected_y_offsets = new base::ListValue();
      g_deeper_than_selected_y_offsets->AppendDouble(28);
      g_deeper_than_selected_y_offsets->AppendDouble(20);
    }
    return *g_deeper_than_selected_y_offsets;
  }

  NOTREACHED();
  CR_DEFINE_STATIC_LOCAL(base::ListValue, default_return_value, ());
  return default_return_value;
}

// static
double WorkspaceCyclerConfiguration::GetDouble(Property property) {
  switch (property) {
    case SHALLOWER_THAN_SELECTED_Y_OFFSETS:
    case DEEPER_THAN_SELECTED_Y_OFFSETS:
      NOTREACHED();
      return 0;
    case SELECTED_Y_OFFSET:
      return g_selected_y_offset;
    case SELECTED_SCALE:
      return g_selected_scale;
    case MIN_SCALE:
      return g_min_scale;
    case MAX_SCALE:
      return g_max_scale;
    case MIN_BRIGHTNESS:
      return g_min_brightness;
    case BACKGROUND_OPACITY:
      return g_background_opacity;
    case DESKTOP_WORKSPACE_BRIGHTNESS:
      return g_desktop_workspace_brightness;
    case DISTANCE_TO_INITIATE_CYCLING:
      return g_distance_to_initiate_cycling;
    case SCROLL_DISTANCE_TO_CYCLE_TO_NEXT_WORKSPACE:
      return g_scroll_amount_to_cycle_to_next_workspace;
    case CYCLER_STEP_ANIMATION_DURATION_RATIO:
      return g_cycler_step_animation_duration_ratio;
    case START_CYCLER_ANIMATION_DURATION:
      return g_start_cycler_animation_duration;
    case STOP_CYCLER_ANIMATION_DURATION:
      return g_stop_cycler_animation_duration;
  }
  NOTREACHED();
  return 0;
}

}  // namespace ash
