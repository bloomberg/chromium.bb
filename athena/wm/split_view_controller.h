// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_WM_SPLIT_VIEW_CONTROLLER_H_
#define ATHENA_WM_SPLIT_VIEW_CONTROLLER_H_

#include "athena/wm/bezel_controller.h"
#include "athena/wm/public/window_manager_observer.h"
#include "athena/wm/window_overview_mode.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"

namespace gfx {
class Transform;
}

namespace athena {
class WindowListProvider;
class WindowManager;
class WindowOverviewModeDelegate;

// Responsible for entering split view mode, exiting from split view mode, and
// laying out the windows in split view mode.
class SplitViewController : public BezelController::ScrollDelegate,
                            public WindowManagerObserver {
 public:
  SplitViewController(aura::Window* container,
                      WindowListProvider* window_list_provider,
                      WindowManager* window_manager);

  virtual ~SplitViewController();

  bool IsSplitViewModeActive() const;

  // Activates split-view mode with |left| and |right| windows. If |left| and/or
  // |right| is NULL, then the first window in the window-list (which is neither
  // |left| nor |right|) is selected instead.
  void ActivateSplitMode(aura::Window* left, aura::Window* right);

 private:
  enum State {
    // Split View mode is not active. |left_window_| and |right_window| are
    // NULL.
    INACTIVE,
    // Two windows |left_window_| and |right_window| are shown side by side and
    // there is a horizontal scroll in progress which is dragging the separator
    // between the two windows.
    SCROLLING,
    // Split View mode is active with |left_window_| and |right_window| showing
    // side by side each occupying half the screen. No scroll in progress.
    ACTIVE
  };

  void UpdateLayout(bool animate);

  void SetWindowTransform(aura::Window* left_window,
                          const gfx::Transform& transform,
                          bool animate);

  void OnAnimationCompleted(aura::Window* window);

  void UpdateSeparatorPositionFromScrollDelta(float delta);

  // Returns the current activity shown to the user. Persists through the
  // SCROLLING and ACTIVE states and gets updated if the current activity goes
  // off screen when the user switches between activities.
  aura::Window* GetCurrentActivityWindow();

  // BezelController::ScrollDelegate overrides.
  virtual void ScrollBegin(BezelController::Bezel bezel, float delta) OVERRIDE;
  virtual void ScrollEnd() OVERRIDE;
  virtual void ScrollUpdate(float delta) OVERRIDE;
  virtual bool CanScroll() OVERRIDE;

  // WindowManagerObserver
  virtual void OnOverviewModeEnter() OVERRIDE;
  virtual void OnOverviewModeExit() OVERRIDE;

  State state_;

  aura::Window* container_;

  // Window Manager which owns this SplitViewController.
  // Must be non NULL for the duration of the lifetime.
  WindowManager* window_manager_;

  // Provider of the list of windows to cycle through. Not owned.
  WindowListProvider* window_list_provider_;

  // Keeps track of the current activity shown as the user switches between
  // activities. Persists through the SCROLLING and ACTIVE states. Gets reset
  // to NULL when overview mode is activated.
  aura::Window* current_activity_window_;

  // Windows for the left and right activities shown in SCROLLING and ACTIVE
  // states. In INACTIVE state these are NULL.
  aura::Window* left_window_;
  aura::Window* right_window_;

  // Position of the separator between left_window_ and right_window_ in
  // container_ coordinates (along the x axis).
  int separator_position_;

  base::WeakPtrFactory<SplitViewController> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SplitViewController);
};

}  // namespace athena

#endif  // ATHENA_WM_SPLIT_VIEW_CONTROLLER_H_
