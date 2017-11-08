// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_state.h"
#include "base/macros.h"

namespace ash {
namespace wm {

// BaseState implements the common framework for WindowState::State.
class BaseState : public WindowState::State {
 public:
  explicit BaseState(mojom::WindowStateType initial_state_type);
  ~BaseState() override;

  // WindowState::State:
  void OnWMEvent(WindowState* window_state, const WMEvent* event) override;
  mojom::WindowStateType GetType() const override;

 protected:
  // Returns the WindowStateType corresponds to the WMEvent type.
  static mojom::WindowStateType GetStateForTransitionEvent(
      const WMEvent* event);

  // Handle workspace related events, such as DISPLAY_BOUNDS_CHANGED.
  virtual void HandleWorkspaceEvents(WindowState* window_state,
                                     const WMEvent* event) = 0;

  // Handle state dependent events, such as TOGGLE_MAXIMIZED,
  // TOGGLE_FULLSCREEN.
  virtual void HandleCompoundEvents(WindowState* window_state,
                                    const WMEvent* event) = 0;

  // Handle bounds change events: SET_BOUNDS and CENTER.
  virtual void HandleBoundsEvents(WindowState* window_state,
                                  const WMEvent* event) = 0;

  // Handle state transition events, such as MAXIMZIED, MINIMIZED.
  virtual void HandleTransitionEvents(WindowState* window_state,
                                      const WMEvent* event) = 0;

  // Show/Hide window when minimized state changes.
  void UpdateMinimizedState(WindowState* window_state,
                            mojom::WindowStateType previous_state_type);

  // The current type of the window.
  mojom::WindowStateType state_type_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BaseState);
};

}  // namespace wm
}  // namespace ash
