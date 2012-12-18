// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_ACTIVATION_CONTROLLER_DELEGATE_H_
#define ASH_WM_ACTIVATION_CONTROLLER_DELEGATE_H_

#include "ash/ash_export.h"

namespace aura {
class Window;
}

namespace ash {
namespace internal {

class ASH_EXPORT ActivationControllerDelegate {
 public:
  virtual ~ActivationControllerDelegate() {}

  // Called when the ActivationController is about to activate |window|. The
  // delegate gets an opportunity to take action and modify activation.
  // Modification occurs via the return value:
  // Returning |window| will activate |window|.
  // Returning some other window will activate that window instead.
  // Returning NULL will not change activation.
  virtual aura::Window* WillActivateWindow(aura::Window* window) = 0;

  // Called when the ActivationController is about to focus |window|. Returns
  // the window that should be focused instead.
  virtual aura::Window* WillFocusWindow(aura::Window* window) = 0;
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_ACTIVATION_CONTROLLER_DELEGATE_H_
