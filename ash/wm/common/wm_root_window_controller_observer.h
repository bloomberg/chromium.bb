// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_COMMON_WM_ROOT_WINDOW_CONTROLLER_OBSERVER_H_
#define ASH_WM_COMMON_WM_ROOT_WINDOW_CONTROLLER_OBSERVER_H_

#include "ash/ash_export.h"

namespace ash {
namespace wm {

class WmWindow;

class ASH_EXPORT WmRootWindowControllerObserver {
 public:
  virtual void OnWorkAreaChanged() {}
  virtual void OnFullscreenStateChanged(bool is_fullscreen) {}
  virtual void OnShelfAlignmentChanged() {}

 protected:
  virtual ~WmRootWindowControllerObserver() {}
};

}  // namespace wm
}  // namespace ash

#endif  // ASH_WM_COMMON_WM_ROOT_WINDOW_CONTROLLER_OBSERVER_H_
