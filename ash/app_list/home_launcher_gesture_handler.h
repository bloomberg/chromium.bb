// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_HOME_LAUNCHER_GESTURE_HANDLER_H_
#define ASH_APP_LIST_HOME_LAUNCHER_GESTURE_HANDLER_H_

#include <map>
#include <vector>

#include "ash/ash_export.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_observer.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/scoped_observer.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/transform.h"

namespace ash {

class AppListControllerImpl;

// HomeLauncherGestureHandler makes modifications to a window's transform and
// opacity when gesture drag events are received and forwarded to it.
// Additionally hides windows which may block the home launcher. All
// modifications are either transitioned to their final state, or back to their
// initial state on release event.
class ASH_EXPORT HomeLauncherGestureHandler : aura::WindowObserver,
                                              TabletModeObserver,
                                              ui::ImplicitAnimationObserver {
 public:
  // Enum which tracks which mode the current scroll process is in.
  enum class Mode {
    kNone,             // There is no current scroll process.
    kSwipeUpToShow,    // Swiping away the MRU window to display launcher.
    kSwipeDownToHide,  // Swiping down the MRU window to hide launcher.
  };

  explicit HomeLauncherGestureHandler(
      AppListControllerImpl* app_list_controller);
  ~HomeLauncherGestureHandler() override;

  // Called by owner of this object when a gesture event is received. |location|
  // should be in screen coordinates. Returns false if the the gesture event
  // was not processed.
  bool OnPressEvent(Mode mode);
  bool OnScrollEvent(const gfx::Point& location);
  bool OnReleaseEvent(const gfx::Point& location);

  // TODO(sammiequon): Investigate if it is needed to observe potential window
  // visibility changes, if they can happen.
  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

  // TabletModeObserver:
  void OnTabletModeEnded() override;

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override;

  aura::Window* window() { return window_; }

 private:
  // Stores the initial and target opacities and transforms of a window.
  struct WindowValues {
    float initial_opacity;
    float target_opacity;
    gfx::Transform initial_transform;
    gfx::Transform target_transform;
  };

  // Updates the opacity and transform |window_| and its transient children base
  // on the values in |window_values_| and |transient_descendants_values_|.
  // |progress| is between 0.0 and 1.0, where 0.0 means the window will have its
  // original opacity and transform, and 1.0 means the window will be faded out
  // and transformed offscreen.
  void UpdateWindows(double progress, bool animate);

  // Stop observing all windows and remove their local pointers.
  void RemoveObserversAndStopTracking();

  aura::Window* window_ = nullptr;

  Mode mode_ = Mode::kNone;

  // Original and target transform and opacity of |window_|.
  WindowValues window_values_;

  // Original and target transform and opacity of the backdrop window. Empty if
  // there is no backdrop on mouse pressed.
  base::Optional<WindowValues> backdrop_values_;

  // Tracks the transient descendants of |window_| and their initial and target
  // opacities and transforms.
  std::map<aura::Window*, WindowValues> transient_descendants_values_;

  // Stores windows which were shown behind the mru window. They need to be
  // hidden so the home launcher is visible when swiping up.
  std::vector<aura::Window*> hidden_windows_;

  // Tracks the location of the last received event in screen coordinates. Empty
  // if there is currently no window being processed.
  base::Optional<gfx::Point> last_event_location_;

  ScopedObserver<TabletModeController, TabletModeObserver>
      tablet_mode_observer_{this};

  // Unowned and guaranteed to be non null for the lifetime of this.
  AppListControllerImpl* app_list_controller_;

  DISALLOW_COPY_AND_ASSIGN(HomeLauncherGestureHandler);
};

}  // namespace ash

#endif  // ASH_APP_LIST_HOME_LAUNCHER_GESTURE_HANDLER_H_
