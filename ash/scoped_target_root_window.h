// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef ASH_SCOPED_TARGET_ROOT_WINDOW_H_
#define ASH_SCOPED_TARGET_ROOT_WINDOW_H_

#include "base/basictypes.h"

namespace aura {
class RootWindow;
}

namespace ash {
namespace internal {

// Constructing a ScopedTargetRootWindow allows temporarily
// switching a target root window so that a new window gets created
// in the same window where a user interaction happened.
// An example usage is to specify the target root window when creating
// a new window using launcher's icon.
class ScopedTargetRootWindow {
 public:
  explicit ScopedTargetRootWindow(aura::RootWindow* root_window);
  ~ScopedTargetRootWindow();

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedTargetRootWindow);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_SCOPED_TARGET_ROOT_WINDOW_H_
