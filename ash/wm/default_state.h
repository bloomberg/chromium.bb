// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DEFAULT_STATE_H_
#define ASH_WM_DEFAULT_STATE_H_

#include "ash/wm/window_state.h"

namespace ash {
namespace wm {
class SetBoundsEvent;

// DefaultState implements Ash behavior without state machine.
class DefaultState : public WindowState::State {
 public:
  explicit DefaultState(WindowStateType initial_state_type);
  virtual ~DefaultState();

  // WindowState::State overrides:
  virtual void OnWMEvent(WindowState* window_state,
                         const WMEvent* event) OVERRIDE;
  virtual WindowStateType GetType() const OVERRIDE;
  virtual void AttachState(WindowState* window_state,
                           WindowState::State* previous_state) OVERRIDE;
  virtual void DetachState(WindowState* window_state) OVERRIDE;

 private:
  // Process state dependent events, such as TOGGLE_MAXIMIZED,
  // TOGGLE_FULLSCREEN.
  static bool ProcessCompoundEvents(WindowState* window_state,
                                    const WMEvent* event);

  // Process workspace related events, such as DISPLAY_BOUNDS_CHANGED.
  static bool ProcessWorkspaceEvents(WindowState* window_state,
                                     const WMEvent* event);

  // Animates to new window bounds based on the current and previous state type.
  static void UpdateBounds(wm::WindowState* window_state,
                           wm::WindowStateType old_state_type);

  // Set the fullscreen/maximized bounds without animation.
  static bool SetMaximizedOrFullscreenBounds(wm::WindowState* window_state);

  static void SetBounds(WindowState* window_state,
                        const SetBoundsEvent* bounds_event);

  // The current type of the window.
  WindowStateType state_type_;

  // The saved window state for the case that the state gets de-/activated.
  gfx::Rect stored_bounds_;
  gfx::Rect stored_restore_bounds_;

  // The size of the workspace when the mode got started. If it differs from
  // the current values the bounds will get ignored.
  gfx::Size stored_workspace_size_;

  // The window state only gets remembered for DCHECK reasons.
  WindowState* stored_window_state_;

  DISALLOW_COPY_AND_ASSIGN(DefaultState);
};

}  // namespace wm
}  // namespace ash

#endif  // ASH_WM_DEFAULT_STATE_H_
