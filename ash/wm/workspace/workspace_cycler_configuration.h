// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WORKSPACE_WORKSPACE_CYCLER_CONFIGURATION_H_
#define ASH_WM_WORKSPACE_WORKSPACE_CYCLER_CONFIGURATION_H_

#include "ash/ash_export.h"
#include "base/basictypes.h"

namespace base {
class ListValue;
}

namespace ash {

// Sets and retrieves properties related to cycling workspaces.
class ASH_EXPORT WorkspaceCyclerConfiguration {
 public:
  enum Property {
    // The y offsets for the workspaces which are shallower than the selected
    // workspace with respect to the top of the screen workarea. The offset at
    // index 0 is for the workspace closest to the selected workspace. The size
    // of the ListValue limits the amount of visible workspaces shallower than
    // the selected workspace.
    SHALLOWER_THAN_SELECTED_Y_OFFSETS,

    // The y offsets for the workspaces which are deeper than the selected
    // workspace with respect to the bottom of the screen workarea. The offset
    // at index 0 is for the workspace closest to the selected workspace. The
    // size of the ListValue limits the amount of visible workspaces deeper
    // than the selected workspace.
    DEEPER_THAN_SELECTED_Y_OFFSETS,

    // The y offset of the selected workspace with respect to the top of the
    // screen workarea.
    SELECTED_Y_OFFSET,

    SELECTED_SCALE,
    MIN_SCALE,
    MAX_SCALE,
    MIN_BRIGHTNESS,

    // The background opacity while the user is cycling through workspaces.
    BACKGROUND_OPACITY,

    // The brightness of the desktop workspace.
    DESKTOP_WORKSPACE_BRIGHTNESS,

    // The vertical scroll distance in pixels to initiate workspace cycling.
    DISTANCE_TO_INITIATE_CYCLING,

    // The vertical scroll distance in pixels to cycle to the next / previous
    // workspace.
    SCROLL_DISTANCE_TO_CYCLE_TO_NEXT_WORKSPACE,

    // The ratio of the duration of the animation in milliseconds to the amount
    // that the user has scrolled vertically in pixels.
    // The duration of the animation is computed by: distance_scrolled * ratio
    CYCLER_STEP_ANIMATION_DURATION_RATIO,

    // The duration in milliseconds of the animations when animating starting
    // the cycler.
    START_CYCLER_ANIMATION_DURATION,

    // The duration in milliseconds of the animations when animating stopping
    // the cycler.
    STOP_CYCLER_ANIMATION_DURATION,
  };

  // Returns true if the cycler is enabled.
  static bool IsCyclerEnabled();

  // Returns true if |property| is of type base::ListValue.
  static bool IsListProperty(Property property);

  // Sets |property| to |list_value|.
  static void SetListValue(Property property,
                           const base::ListValue& list_value);

  // Sets |property| to |value|.
  static void SetDouble(Property property, double value);

  // Gets the ListValue associated with |property|.
  static const base::ListValue& GetListValue(Property property);

  // Gets the double associated with |property|.
  static double GetDouble(Property property);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(WorkspaceCyclerConfiguration);
};

}  // namespace ash

#endif  // ASH_WM_WORKSPACE_WORKSPACE_CYCLER_ANIMATOR_CONFIGURATION_H_
