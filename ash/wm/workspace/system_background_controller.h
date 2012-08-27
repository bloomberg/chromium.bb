// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WORKSPACE_SYSTEM_BACKGROUND_CONTROLLER_H_
#define ASH_WM_WORKSPACE_SYSTEM_BACKGROUND_CONTROLLER_H_

#include "ash/ash_export.h"
#include "base/basictypes.h"

namespace aura {
class RootWindow;
};

namespace ash {
namespace internal {

// SystemBackgroundController shows the system level background. See
// kShellWindowId_SystemBackgroundContainer for a description of the system
// level background.
class ASH_EXPORT SystemBackgroundController {
 public:
  explicit SystemBackgroundController(aura::RootWindow* root);
  ~SystemBackgroundController();

 private:
  class View;

  // View responsible for rendering the background. This is non-NULL if the
  // widget containing it is deleted. It is owned by the widget.
  View* view_;

  DISALLOW_COPY_AND_ASSIGN(SystemBackgroundController);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_WORKSPACE_SYSTEM_BACKGROUND_CONTROLLER_H_
