// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_WM_ROOT_WINDOW_CONTROLLER_H_
#define ASH_COMMON_WM_ROOT_WINDOW_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/common/wm/workspace/workspace_types.h"
#include "ui/views/widget/widget.h"

namespace gfx {
class Point;
}

namespace ash {

class AlwaysOnTopController;
class WmShelf;
class WmShell;
class WmRootWindowControllerObserver;
class WmWindow;
class WorkspaceLayoutManagerBackdropDelegate;

// Provides state associated with a root of a window hierarchy.
class ASH_EXPORT WmRootWindowController {
 public:
  virtual ~WmRootWindowController() {}

  virtual bool HasShelf() = 0;

  virtual WmShell* GetShell() = 0;

  virtual wm::WorkspaceWindowState GetWorkspaceWindowState() = 0;

  // TODO: remove when WorkspaceController moved to common:
  // http://crbug.com/624173.
  virtual void SetMaximizeBackdropDelegate(
      std::unique_ptr<WorkspaceLayoutManagerBackdropDelegate> delegate) = 0;

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

  // Returns the window events will be targeted at for the specified location
  // (in screen coordinates).
  //
  // NOTE: the returned window may not contain the location as resize handles
  // may extend outside the bounds of the window.
  virtual WmWindow* FindEventTarget(const gfx::Point& location_in_screen) = 0;

  virtual void AddObserver(WmRootWindowControllerObserver* observer) = 0;
  virtual void RemoveObserver(WmRootWindowControllerObserver* observer) = 0;
};

}  // namespace ash

#endif  // ASH_COMMON_WM_ROOT_WINDOW_CONTROLLER_H_
