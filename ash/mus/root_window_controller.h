// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_ROOT_WINDOW_CONTROLLER_H_
#define ASH_MUS_ROOT_WINDOW_CONTROLLER_H_

#include <memory>

#include "ash/mus/disconnected_app_handler.h"
#include "services/ui/public/interfaces/window_manager_constants.mojom.h"
#include "ui/display/display.h"

namespace aura {
class WindowTreeHostMus;
namespace client {
class WindowParentingClient;
}
}

namespace gfx {
class Insets;
}

namespace service_manager {
class Connector;
}

namespace ash {

class WmShelf;

namespace mus {

class WindowManager;
class WmRootWindowControllerMus;
class WmTestBase;
class WmTestHelper;
class WmWindowMus;

// RootWindowController manages the windows and state for a single display.
// RootWindowController takes ownership of the WindowTreeHostMus that it passed
// to it.
class RootWindowController {
 public:
  RootWindowController(
      WindowManager* window_manager,
      std::unique_ptr<aura::WindowTreeHostMus> window_tree_host,
      const display::Display& display);
  ~RootWindowController();

  void Shutdown();

  service_manager::Connector* GetConnector();

  aura::Window* root();
  const aura::Window* root() const;
  WmRootWindowControllerMus* wm_root_window_controller() {
    return wm_root_window_controller_.get();
  }

  aura::Window* NewTopLevelWindow(
      ui::mojom::WindowType window_type,
      std::map<std::string, std::vector<uint8_t>>* properties);

  WmWindowMus* GetWindowByShellWindowId(int id);

  void SetWorkAreaInests(const gfx::Insets& insets);
  void SetDisplay(const display::Display& display);

  WindowManager* window_manager() { return window_manager_; }

  aura::WindowTreeHostMus* window_tree_host() {
    return window_tree_host_.get();
  }

  const display::Display& display() const { return display_; }

  WmShelf* wm_shelf() { return wm_shelf_.get(); }

 private:
  friend class WmTestBase;
  friend class WmTestHelper;

  gfx::Rect CalculateDefaultBounds(
      aura::Window* container_window,
      const std::map<std::string, std::vector<uint8_t>>* properties) const;
  gfx::Rect GetMaximizedWindowBounds() const;

  WindowManager* window_manager_;
  std::unique_ptr<aura::WindowTreeHostMus> window_tree_host_;
  int window_count_ = 0;

  display::Display display_;

  std::unique_ptr<WmRootWindowControllerMus> wm_root_window_controller_;
  std::unique_ptr<WmShelf> wm_shelf_;

  std::unique_ptr<aura::client::WindowParentingClient> parenting_client_;

  std::unique_ptr<DisconnectedAppHandler> disconnected_app_handler_;

  DISALLOW_COPY_AND_ASSIGN(RootWindowController);
};

}  // namespace mus
}  // namespace ash

#endif  // ASH_MUS_ROOT_WINDOW_CONTROLLER_H_
