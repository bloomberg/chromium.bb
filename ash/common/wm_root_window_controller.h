// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_WM_ROOT_WINDOW_CONTROLLER_H_
#define ASH_COMMON_WM_ROOT_WINDOW_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/common/wm/workspace/workspace_types.h"
#include "base/macros.h"
#include "ui/views/widget/widget.h"

namespace gfx {
class Point;
}

namespace ash {

class AlwaysOnTopController;
class WmShelf;
class WmShell;
class WmWindow;
class WorkspaceController;

namespace wm {
class RootWindowLayoutManager;
}

// Provides state associated with a root of a window hierarchy.
class ASH_EXPORT WmRootWindowController {
 public:
  explicit WmRootWindowController(WmWindow* window);
  virtual ~WmRootWindowController();

  wm::RootWindowLayoutManager* root_window_layout_manager() {
    return root_window_layout_manager_;
  }

  WorkspaceController* workspace_controller() {
    return workspace_controller_.get();
  }

  wm::WorkspaceWindowState GetWorkspaceWindowState();

  virtual bool HasShelf() = 0;

  virtual WmShell* GetShell() = 0;

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

  // Gets the last location seen in a mouse event in this root window's
  // coordinates. This may return a point outside the root window's bounds.
  virtual gfx::Point GetLastMouseLocationInRoot() = 0;

 protected:
  // Creates the containers (WmWindows) used by the shell.
  void CreateContainers();

  // Creates the LayoutManagers for the windows created by CreateContainers().
  void CreateLayoutManagers();

  void DeleteWorkspaceController();

 private:
  WmWindow* root_;

  wm::RootWindowLayoutManager* root_window_layout_manager_;

  std::unique_ptr<WorkspaceController> workspace_controller_;

  DISALLOW_COPY_AND_ASSIGN(WmRootWindowController);
};

}  // namespace ash

#endif  // ASH_COMMON_WM_ROOT_WINDOW_CONTROLLER_H_
