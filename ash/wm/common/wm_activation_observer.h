// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_COMMON_WM_ACTIVATION_OBSERVER_H_
#define ASH_WM_COMMON_WM_ACTIVATION_OBSERVER_H_

#include "ash/ash_export.h"

namespace ash {
namespace wm {

class WmWindow;

// Used to track changes in activation.
class ASH_EXPORT WmActivationObserver {
 public:
  virtual void OnWindowActivated(WmWindow* gained_active,
                                 WmWindow* lost_active) = 0;

 protected:
  virtual ~WmActivationObserver() {}
};

}  // namespace wm
}  // namespace ash

#endif  // ASH_WM_COMMON_WM_ACTIVATION_OBSERVER_H_
