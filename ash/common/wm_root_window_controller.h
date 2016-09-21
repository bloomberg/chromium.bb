// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_WM_ROOT_WINDOW_CONTROLLER_H_
#define ASH_COMMON_WM_ROOT_WINDOW_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/common/wm/workspace/workspace_types.h"
#include "base/macros.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/widget/widget.h"

namespace gfx {
class Point;
}

namespace ui {
class MenuModel;
}

namespace views {
class MenuModelAdapter;
class MenuRunner;
}

namespace ash {

class AlwaysOnTopController;
class AnimatingWallpaperWidgetController;
class DockedWindowLayoutManager;
class PanelLayoutManager;
class SystemModalContainerLayoutManager;
class WallpaperWidgetController;
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

  DockedWindowLayoutManager* docked_window_layout_manager() {
    return docked_window_layout_manager_;
  }

  PanelLayoutManager* panel_layout_manager() { return panel_layout_manager_; }

  wm::RootWindowLayoutManager* root_window_layout_manager() {
    return root_window_layout_manager_;
  }

  WallpaperWidgetController* wallpaper_widget_controller() {
    return wallpaper_widget_controller_.get();
  }
  void SetWallpaperWidgetController(WallpaperWidgetController* controller);

  AnimatingWallpaperWidgetController* animating_wallpaper_widget_controller() {
    return animating_wallpaper_widget_controller_.get();
  }
  void SetAnimatingWallpaperWidgetController(
      AnimatingWallpaperWidgetController* controller);

  WorkspaceController* workspace_controller() {
    return workspace_controller_.get();
  }

  AlwaysOnTopController* always_on_top_controller() {
    return always_on_top_controller_.get();
  }

  wm::WorkspaceWindowState GetWorkspaceWindowState();

  // Returns the layout manager for the appropriate modal-container. If the
  // window is inside the lockscreen modal container, then the layout manager
  // for that is returned. Otherwise the layout manager for the default modal
  // container is returned.
  // If no window is specified (i.e. |window| is null), then the lockscreen
  // modal container is used if the screen is currently locked. Otherwise, the
  // default modal container is used.
  SystemModalContainerLayoutManager* GetSystemModalLayoutManager(
      WmWindow* window);

  virtual bool HasShelf() = 0;

  virtual WmShell* GetShell() = 0;

  virtual WmShelf* GetShelf() = 0;

  // Returns the window associated with this WmRootWindowController.
  virtual WmWindow* GetWindow() = 0;

  // Gets the WmWindow whose shell window id is |container_id|.
  WmWindow* GetContainer(int container_id);
  const WmWindow* GetContainer(int container_id) const;

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

  // Shows a context menu at the |location_in_screen|.
  void ShowContextMenu(const gfx::Point& location_in_screen,
                       ui::MenuSourceType source_type);

  // Called when the wallpaper animation has started or finished.
  // TODO: port remaining classic ash wallpaper functionality here.
  virtual void OnInitialWallpaperAnimationStarted();
  virtual void OnWallpaperAnimationFinished(views::Widget* widget);

 protected:
  // Moves child windows to |dest|.
  void MoveWindowsTo(WmWindow* dest);

  // Creates the containers (WmWindows) used by the shell.
  void CreateContainers();

  // Creates the LayoutManagers for the windows created by CreateContainers().
  void CreateLayoutManagers();

  // Resets WmShell::GetRootWindowForNewWindows() if appropriate. This is called
  // during shutdown to make sure GetRootWindowForNewWindows() isn't referencing
  // this.
  void ResetRootForNewWindowsIfNecessary();

  // Called during shutdown to destroy state such as windows and LayoutManagers.
  void CloseChildWindows();

  // Called from CloseChildWindows() to determine if the specified window should
  // be destroyed.
  virtual bool ShouldDestroyWindowInCloseChildWindows(WmWindow* window) = 0;

 private:
  // Callback for MenuModelAdapter.
  void OnMenuClosed();

  WmWindow* root_;

  // LayoutManagers are owned by the window they are installed on.
  DockedWindowLayoutManager* docked_window_layout_manager_ = nullptr;
  PanelLayoutManager* panel_layout_manager_ = nullptr;
  wm::RootWindowLayoutManager* root_window_layout_manager_ = nullptr;

  std::unique_ptr<WallpaperWidgetController> wallpaper_widget_controller_;
  std::unique_ptr<AnimatingWallpaperWidgetController>
      animating_wallpaper_widget_controller_;
  std::unique_ptr<WorkspaceController> workspace_controller_;

  std::unique_ptr<AlwaysOnTopController> always_on_top_controller_;

  // Manages the context menu.
  std::unique_ptr<ui::MenuModel> menu_model_;
  std::unique_ptr<views::MenuModelAdapter> menu_model_adapter_;
  std::unique_ptr<views::MenuRunner> menu_runner_;

  DISALLOW_COPY_AND_ASSIGN(WmRootWindowController);
};

}  // namespace ash

#endif  // ASH_COMMON_WM_ROOT_WINDOW_CONTROLLER_H_
