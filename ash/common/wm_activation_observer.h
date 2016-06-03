// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_WM_ACTIVATION_OBSERVER_H_
#define ASH_COMMON_WM_ACTIVATION_OBSERVER_H_

#include "ash/ash_export.h"

namespace ash {

class WmWindow;

// Used to track changes in activation.
class ASH_EXPORT WmActivationObserver {
 public:
  virtual void OnWindowActivated(WmWindow* gained_active,
                                 WmWindow* lost_active) {}

  // Called when during window activation the currently active window is
  // selected for activation. This can happen when a window requested for
  // activation cannot be activated because a system modal window is active.
  virtual void OnAttemptToReactivateWindow(WmWindow* request_active,
                                           WmWindow* actual_active) {}

 protected:
  virtual ~WmActivationObserver() {}
};

}  // namespace ash

#endif  // ASH_COMMON_WM_ACTIVATION_OBSERVER_H_
