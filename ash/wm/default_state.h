// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DEFAULT_STATE_H_
#define ASH_WM_DEFAULT_STATE_H_

#include "ash/wm/window_state.h"

namespace ash {
namespace wm {

// DefaultState implements Ash behavior without state machine.
class DefaultState : public WindowState::State {
 public:
  DefaultState();
  virtual ~DefaultState();

  // WindowState::State overrides:
  virtual void OnWMEvent(WindowState* window_state, WMEvent event) OVERRIDE;

 private:
  // Process stete dependent events, such as TOGGLE_MAXIMIZED,
  // TOGGLE_FULLSCREEN.
  static bool ProcessCompoundEvents(WindowState* window_state, WMEvent event);

  // Animates to new window bounds based on the current and previous show type.
  static void UpdateBoundsFromShowType(wm::WindowState* window_state,
                                       wm::WindowShowType old_show_type);

  DISALLOW_COPY_AND_ASSIGN(DefaultState);
};

}  // namespace wm
}  // namespace ash

#endif  // ASH_WM_DEFAULT_STATE_H_
