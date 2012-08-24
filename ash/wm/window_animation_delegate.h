// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_ANIMATION_DELEGATE_H_
#define ASH_WM_WINDOW_ANIMATION_DELEGATE_H_

#include "ash/ash_export.h"
#include "base/compiler_specific.h"

namespace aura {
class Window;
}

namespace ash {
namespace internal {

// WindowAnimationDelegate is used to determine if a window should be animated.
// It is only queried if kChildWindowVisibilityChangesAnimatedKey is true.
// The delegate of the parent window is queried. If no delegate is installed
// it is treated as though animations are enabled.
class ASH_EXPORT WindowAnimationDelegate {
 public:
  WindowAnimationDelegate() {}

  static void SetDelegate(aura::Window* window,
                          WindowAnimationDelegate* delegate);
  static WindowAnimationDelegate* GetDelegate(aura::Window* window);

  // Returns true if |window| should be animated.
  virtual bool ShouldAnimateWindow(aura::Window* window) = 0;

 protected:
  virtual ~WindowAnimationDelegate() {}
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_WINDOW_ANIMATION_DELEGATE_H_
