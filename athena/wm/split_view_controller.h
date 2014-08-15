// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_WM_SPLIT_VIEW_CONTROLLER_H_
#define ATHENA_WM_SPLIT_VIEW_CONTROLLER_H_

#include "athena/athena_export.h"
#include "athena/wm/bezel_controller.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"

namespace gfx {
class Transform;
}

namespace athena {
class WindowListProvider;

// Responsible for entering split view mode, exiting from split view mode, and
// laying out the windows in split view mode.
class ATHENA_EXPORT SplitViewController
    : public BezelController::ScrollDelegate {
 public:
  SplitViewController(aura::Window* container,
                      WindowListProvider* window_list_provider);

  virtual ~SplitViewController();

  bool IsSplitViewModeActive() const;

  // Activates split-view mode with |left| and |right| windows. If |left| and/or
  // |right| is NULL, then the first window in the window-list (which is neither
  // |left| nor |right|) is selected instead.
  void ActivateSplitMode(aura::Window* left, aura::Window* right);

  // Resets the internal state to an inactive state. Calling this does not
  // change the window bounds/transforms etc. The caller must take care of
  // making any necessary changes.
  void DeactivateSplitMode();

  aura::Window* left_window() { return left_window_; }
  aura::Window* right_window() { return right_window_; }

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

  // BezelController::ScrollDelegate:
  virtual void ScrollBegin(BezelController::Bezel bezel, float delta) OVERRIDE;
  virtual void ScrollEnd() OVERRIDE;
  virtual void ScrollUpdate(float delta) OVERRIDE;
  virtual bool CanScroll() OVERRIDE;

  State state_;

  aura::Window* container_;

  // Provider of the list of windows to cycle through. Not owned.
  WindowListProvider* window_list_provider_;

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
