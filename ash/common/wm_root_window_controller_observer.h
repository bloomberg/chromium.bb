// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_WM_ROOT_WINDOW_CONTROLLER_OBSERVER_H_
#define ASH_COMMON_WM_ROOT_WINDOW_CONTROLLER_OBSERVER_H_

#include "ash/ash_export.h"

namespace ash {

class WmWindow;

// TODO(sky): nuke and go back to ShellObserver.
class ASH_EXPORT WmRootWindowControllerObserver {
 public:
  virtual void OnWorkAreaChanged() {}
  virtual void OnFullscreenStateChanged(bool is_fullscreen) {}
  virtual void OnShelfAlignmentChanged() {}

 protected:
  virtual ~WmRootWindowControllerObserver() {}
};

}  // namespace ash

#endif  // ASH_COMMON_WM_ROOT_WINDOW_CONTROLLER_OBSERVER_H_
