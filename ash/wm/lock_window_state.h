// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_LOCK_WINDOW_STATE_H_
#define ASH_WM_LOCK_WINDOW_STATE_H_

#include "ash/wm/window_state.h"

namespace ash {

// The LockWindowState implementation which reduces all possible window
// states to maximized (or normal if can't be maximized)/minimized/full-screen
// and is applied only on lock (login) window container.
// LockWindowState implements Ash behavior without state machine.
class LockWindowState : public wm::WindowState::State {
 public:
  // The |window|'s state object will be modified to use this new window mode
  // state handler.
  explicit LockWindowState(aura::Window* window);
  virtual ~LockWindowState();

  // WindowState::State overrides:
  virtual void OnWMEvent(wm::WindowState* window_state,
                         const wm::WMEvent* event) OVERRIDE;
  virtual wm::WindowStateType GetType() const OVERRIDE;
  virtual void AttachState(wm::WindowState* window_state,
                           wm::WindowState::State* previous_state) OVERRIDE;
  virtual void DetachState(wm::WindowState* window_state) OVERRIDE;

  // Creates new LockWindowState instance and attaches it to |window|.
  static wm::WindowState* SetLockWindowState(aura::Window* window);

 private:
  // Updates the window to |new_state_type| and resulting bounds:
  // Either full screen, maximized centered or minimized. If the state does not
  // change, only the bounds will be changed.
  void UpdateWindow(wm::WindowState* window_state,
                    wm::WindowStateType new_state_type);

  // Depending on the capabilities of the window we either return
  // |WINDOW_STATE_TYPE_MAXIMIZED| or |WINDOW_STATE_TYPE_NORMAL|.
  wm::WindowStateType GetMaximizedOrCenteredWindowType(
      wm::WindowState* window_state);

  // Updates the bounds taking virtual keyboard bounds into consideration.
  void UpdateBounds(wm::WindowState* window_state);

  // The current state type. Due to the nature of this state, this can only be
  // WM_STATE_TYPE{NORMAL, MINIMIZED, MAXIMIZED}.
  wm::WindowStateType current_state_type_;

  DISALLOW_COPY_AND_ASSIGN(LockWindowState);
};

}  // namespace ash

#endif  // ASH_WM_LOCK_WINDOW_STATE_H_
