// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_COMMON_WM_ROOT_CONTROLLER_H_
#define ASH_WM_COMMON_WM_ROOT_CONTROLLER_H_

#include <stdint.h>

#include "ash/ash_export.h"
#include "ash/wm/common/workspace/workspace_types.h"
#include "ui/views/widget/widget.h"

namespace ash {

class AlwaysOnTopController;

namespace wm {

class WmGlobals;
class WmRootWindowControllerObserver;
class WmShelf;
class WmWindow;

// Provides state associated with a root of a window hierarchy.
class ASH_EXPORT WmRootWindowController {
 public:
  virtual ~WmRootWindowController() {}

  static WmRootWindowController* GetWithDisplayId(int64_t id);

  virtual bool HasShelf() = 0;

  virtual WmGlobals* GetGlobals() = 0;

  virtual WorkspaceWindowState GetWorkspaceWindowState() = 0;

  virtual AlwaysOnTopController* GetAlwaysOnTopController() = 0;

  virtual WmShelf* GetShelf() = 0;

  // Returns the window associated with this WmRootWindowController.
  virtual WmWindow* GetWindow() = 0;

  // Configures |init_params| prior to initializing |widget|.
  // |shell_container_id| is the id of the container to parent |widget| to.
  virtual void ConfigureWidgetInitParamsForContainer(
      views::Widget* widget,
      int shell_container_id,
      views::Widget::InitParams* init_params) = 0;

  virtual void AddObserver(WmRootWindowControllerObserver* observer) = 0;
  virtual void RemoveObserver(WmRootWindowControllerObserver* observer) = 0;
};

}  // namespace wm
}  // namespace ash

#endif  // ASH_WM_COMMON_WM_ROOT_CONTROLLER_H_
