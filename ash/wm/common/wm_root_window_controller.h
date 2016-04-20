// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_COMMON_WM_ROOT_CONTROLLER_H_
#define ASH_WM_COMMON_WM_ROOT_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/wm/common/workspace/workspace_types.h"

namespace ash {
namespace wm {

class WmGlobals;
class WmWindow;

class ASH_EXPORT WmRootWindowController {
 public:
  virtual ~WmRootWindowController() {}

  virtual bool HasShelf() = 0;

  virtual WmGlobals* GetGlobals() = 0;

  virtual WorkspaceWindowState GetWorkspaceWindowState() = 0;
};

}  // namespace wm
}  // namespace ash

#endif  // ASH_WM_COMMON_WM_ROOT_CONTROLLER_H_
