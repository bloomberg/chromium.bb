// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_ROOT_WINDOW_CONTROLLER_H_
#define ASH_MUS_ROOT_WINDOW_CONTROLLER_H_

#include <memory>

#include "ash/mus/disconnected_app_handler.h"
#include "ash/root_window_controller.h"
#include "services/ui/public/interfaces/window_manager_constants.mojom.h"
#include "ui/display/display.h"

namespace aura {
class WindowTreeHostMus;
}

namespace gfx {
class Insets;
}

namespace service_manager {
class Connector;
}

namespace ash {

class WmWindowAura;

namespace mus {

class WindowManager;
class WmTestBase;
class WmTestHelper;

// RootWindowController manages the windows and state for a single display.
// RootWindowController takes ownership of the WindowTreeHostMus that it passed
// to it.
// TODO(sky): rename this (or possibly just remove entirely).
// http://crbug.com/671246
class RootWindowController {
 public:
  RootWindowController(
      WindowManager* window_manager,
      std::unique_ptr<aura::WindowTreeHostMus> window_tree_host,
      const display::Display& display,
      ash::RootWindowController::RootWindowType root_window_type);
  ~RootWindowController();

  // Returns the RootWindowController for |window|'s root.
  static RootWindowController* ForWindow(aura::Window* window);

  void Shutdown();

  service_manager::Connector* GetConnector();

  aura::Window* root();
  const aura::Window* root() const;

  aura::Window* NewTopLevelWindow(
      ui::mojom::WindowType window_type,
      std::map<std::string, std::vector<uint8_t>>* properties);

  ash::WmWindowAura* GetWindowByShellWindowId(int id);

  void SetWorkAreaInests(const gfx::Insets& insets);
  void SetDisplay(const display::Display& display);

  WindowManager* window_manager() { return window_manager_; }

  aura::WindowTreeHostMus* window_tree_host() { return window_tree_host_; }

  const display::Display& display() const { return display_; }

  ash::RootWindowController* ash_root_window_controller() {
    return ash_root_window_controller_.get();
  }

 private:
  friend class WmTestBase;
  friend class WmTestHelper;

  gfx::Rect CalculateDefaultBounds(
      aura::Window* container_window,
      const std::map<std::string, std::vector<uint8_t>>* properties) const;
  gfx::Rect GetMaximizedWindowBounds() const;

  WindowManager* window_manager_;
  std::unique_ptr<ash::RootWindowController> ash_root_window_controller_;
  // Owned by |ash_root_window_controller_|.
  aura::WindowTreeHostMus* window_tree_host_;
  int window_count_ = 0;

  display::Display display_;

  std::unique_ptr<DisconnectedAppHandler> disconnected_app_handler_;

  DISALLOW_COPY_AND_ASSIGN(RootWindowController);
};

}  // namespace mus
}  // namespace ash

#endif  // ASH_MUS_ROOT_WINDOW_CONTROLLER_H_
