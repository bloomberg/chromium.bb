// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ROOT_WINDOW_CONTROLLER_H_
#define ASH_ROOT_WINDOW_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/wm/shelf_types.h"
#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"

class SkBitmap;

namespace aura {
class EventFilter;
class RootWindow;
class Window;
namespace shared {
class InputMethodEventFilter;
class RootWindowEventFilter;
}  // namespace shared
}  // namespace aura

namespace ash {
class Launcher;
class ToplevelWindowEventHandler;

namespace internal {

class ColoredWindowController;
class PanelLayoutManager;
class RootWindowLayoutManager;
class ScreenDimmer;
class ShelfLayoutManager;
class StatusAreaWidget;
class SystemBackgroundController;
class SystemModalContainerLayoutManager;
class WorkspaceController;

// This class maintains the per root window state for ash. This class
// owns the root window and other dependent objects that should be
// deleted upon the deletion of the root window.  The RootWindowController
// for particular root window is stored as a property and can be obtained
// using |GetRootWindowController(aura::RootWindow*)| function.
class ASH_EXPORT RootWindowController {
 public:
  explicit RootWindowController(aura::RootWindow* root_window);
  ~RootWindowController();

  aura::RootWindow* root_window() { return root_window_.get(); }

  RootWindowLayoutManager* root_window_layout() { return root_window_layout_; }

  WorkspaceController* workspace_controller() {
    return workspace_controller_.get();
  }

  ScreenDimmer* screen_dimmer() { return screen_dimmer_.get(); }

  Launcher* launcher() { return launcher_.get(); }

  // TODO(sky): don't expose this!
  internal::ShelfLayoutManager* shelf() const { return shelf_; }

  internal::StatusAreaWidget* status_area_widget() const {
    return status_area_widget_;
  }

  // Returns the layout-manager for the appropriate modal-container. If the
  // window is inside the lockscreen modal container, then the layout manager
  // for that is returned. Otherwise the layout manager for the default modal
  // container is returned.
  // If no window is specified (i.e. |window| is NULL), then the lockscreen
  // modal container is used if the screen is currently locked. Otherwise, the
  // default modal container is used.
  SystemModalContainerLayoutManager* GetSystemModalLayoutManager(
      aura::Window* window);

  aura::Window* GetContainer(int container_id);

  void InitLayoutManagers();
  void CreateContainers();

  // Initializs the RootWindowController for primary display. This
  // creates
  void InitForPrimaryDisplay();

  // Initializes |background_|.  |is_first_run_after_boot| determines the
  // background's initial color.
  void CreateSystemBackground(bool is_first_run_after_boot);

  // Initializes |launcher_|.  Does nothing if it's already initialized.
  void CreateLauncher();

  // Show launcher view if it was created hidden (before session has started).
  void ShowLauncher();

  // Updates |background_| to be black after the desktop background is visible.
  void HandleDesktopBackgroundVisible();

  // Deletes associated objects and clears the state, but doesn't delete
  // the root window yet. This is used to delete a secondary displays'
  // root window safely when the display disconnect signal is received,
  // which may come while we're in the nested message loop.
  void Shutdown();

  // Deletes all child windows and performs necessary cleanup.
  void CloseChildWindows();

  // Moves child windows to |dest|.
  void MoveWindowsTo(aura::RootWindow* dest);

  // Force the shelf to query for it's current visibility state.
  void UpdateShelfVisibility();

  // Sets/gets the shelf auto-hide behavior.
  void SetShelfAutoHideBehavior(ShelfAutoHideBehavior behavior);
  ShelfAutoHideBehavior GetShelfAutoHideBehavior() const;

  // Sets/gets the shelf alignemnt.
  bool SetShelfAlignment(ShelfAlignment alignment);
  ShelfAlignment GetShelfAlignment();

private:
  // Creates each of the special window containers that holds windows of various
  // types in the shell UI.
  void CreateContainersInRootWindow(aura::RootWindow* root_window);

  scoped_ptr<aura::RootWindow> root_window_;
  RootWindowLayoutManager* root_window_layout_;

  // Widget containing system tray.
  internal::StatusAreaWidget* status_area_widget_;

  // The shelf for managing the launcher and the status widget.
  // RootWindowController does not own the shelf. Instead, it is owned
  // by container of the status area.
  internal::ShelfLayoutManager* shelf_;

  // Manages layout of panels. Owned by PanelContainer.
  internal::PanelLayoutManager* panel_layout_manager_;

  scoped_ptr<Launcher> launcher_;

  // A background layer that's displayed beneath all other layers.  Without
  // this, portions of the root window that aren't covered by layers will be
  // painted white; this can show up if e.g. it takes a long time to decode the
  // desktop background image when displaying the login screen.
  scoped_ptr<ColoredWindowController> background_;

  scoped_ptr<ScreenDimmer> screen_dimmer_;
  scoped_ptr<WorkspaceController> workspace_controller_;

  // We need to own event handlers for various containers.
  scoped_ptr<ToplevelWindowEventHandler> default_container_handler_;
  scoped_ptr<ToplevelWindowEventHandler> always_on_top_container_handler_;
  scoped_ptr<ToplevelWindowEventHandler> modal_container_handler_;
  scoped_ptr<ToplevelWindowEventHandler> lock_modal_container_handler_;

  DISALLOW_COPY_AND_ASSIGN(RootWindowController);
};

}  // namespace internal
}  // ash

#endif  //  ASH_ROOT_WINDOW_CONTROLLER_H_
