// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_COMMON_WINDOW_STATE_OBSERVER_H_
#define ASH_WM_COMMON_WINDOW_STATE_OBSERVER_H_

#include "ash/wm/common/ash_wm_common_export.h"
#include "ash/wm/common/wm_types.h"

namespace ash {
namespace wm {
class WindowState;

class ASH_WM_COMMON_EXPORT WindowStateObserver {
 public:
  // Following observer methods are different from kWindowShowStatekey
  // property change as they will be invoked when the window
  // gets left/right maximized, and auto positioned. |old_type| is the value
  // before the change.

  // Called after the window's state type is set to new type, but before
  // the window's bounds has been updated for the new type.
  // This is used to update the shell state such as work area so
  // that the window can use the correct environment to update its bounds.
  // TODO(oshima): Remove this once docked windows has its own state.
  virtual void OnPreWindowStateTypeChange(WindowState* window_state,
                                          WindowStateType old_type) {}

  // Called after the window's state has been updated.
  // This is used to update the shell state that depends on the updated
  // window bounds, such as shelf visibility.
  virtual void OnPostWindowStateTypeChange(WindowState* window_state,
                                           WindowStateType old_type) {}
};

}  // namespace wm
}  // namespace ash

#endif  // ASH_WM_COMMON_WINDOW_STATE_OBSERVER_H_
