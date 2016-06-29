// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_ROOT_WINDOW_CONTROLLER_H_
#define ASH_MUS_ROOT_WINDOW_CONTROLLER_H_

#include <memory>

#include "ash/mus/disconnected_app_handler.h"
#include "ash/mus/shelf_layout_manager_delegate.h"
#include "ash/public/interfaces/container.mojom.h"
#include "components/mus/public/cpp/window_observer.h"
#include "components/mus/public/interfaces/window_manager_constants.mojom.h"
#include "ui/display/display.h"

namespace shell {
class Connector;
}

namespace ash {

class AlwaysOnTopController;
class RootWindowControllerCommon;
class WorkspaceLayoutManager;

namespace mus {

class LayoutManager;
class ShelfLayoutManager;
class StatusLayoutManager;
class WindowManager;
class WmRootWindowControllerMus;
class WmShelfMus;
class WmTestBase;
class WmTestHelper;
class WmWindowMus;

// RootWindowController manages the windows and state for a single display.
// RootWindowController is tied to the lifetime of the ::mus::Window it is
// created with. It is assumed the RootWindowController is deleted once the
// associated ::mus::Window is destroyed.
class RootWindowController : public ShelfLayoutManagerDelegate {
 public:
  RootWindowController(WindowManager* window_manager,
                       ::mus::Window* root,
                       const display::Display& display);
  ~RootWindowController() override;

  shell::Connector* GetConnector();

  ::mus::Window* root() { return root_; }

  int window_count() { return window_count_; }

  ::mus::Window* NewTopLevelWindow(
      std::map<std::string, std::vector<uint8_t>>* properties);

  ::mus::Window* GetWindowForContainer(mojom::Container container);

  WmWindowMus* GetWindowByShellWindowId(int id);

  WindowManager* window_manager() { return window_manager_; }

  const display::Display& display() const { return display_; }

  ShelfLayoutManager* GetShelfLayoutManager();
  StatusLayoutManager* GetStatusLayoutManager();
  WorkspaceLayoutManager* workspace_layout_manager() {
    return workspace_layout_manager_;
  }

  AlwaysOnTopController* always_on_top_controller() {
    return always_on_top_controller_.get();
  }

  WmShelfMus* wm_shelf() { return wm_shelf_.get(); }

 private:
  friend class WmTestBase;
  friend class WmTestHelper;

  gfx::Rect CalculateDefaultBounds(::mus::Window* window) const;
  gfx::Rect GetMaximizedWindowBounds() const;

  // ShelfLayoutManagerDelegate:
  void OnShelfWindowAvailable() override;

  // Creates the necessary set of layout managers in the shell windows.
  void CreateLayoutManagers();

  WindowManager* window_manager_;
  ::mus::Window* root_;
  int window_count_ = 0;

  display::Display display_;

  std::unique_ptr<RootWindowControllerCommon> root_window_controller_common_;

  std::unique_ptr<WmRootWindowControllerMus> wm_root_window_controller_;
  std::unique_ptr<WmShelfMus> wm_shelf_;

  // Owned by the corresponding container.
  WorkspaceLayoutManager* workspace_layout_manager_ = nullptr;
  std::map<::mus::Window*, std::unique_ptr<LayoutManager>> layout_managers_;

  std::unique_ptr<AlwaysOnTopController> always_on_top_controller_;

  std::unique_ptr<DisconnectedAppHandler> disconnected_app_handler_;

  DISALLOW_COPY_AND_ASSIGN(RootWindowController);
};

}  // namespace mus
}  // namespace ash

#endif  // ASH_MUS_ROOT_WINDOW_CONTROLLER_H_
