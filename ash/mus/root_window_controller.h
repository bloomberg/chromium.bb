// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_ROOT_WINDOW_CONTROLLER_H_
#define ASH_MUS_ROOT_WINDOW_CONTROLLER_H_

#include <memory>

#include "ash/mus/disconnected_app_handler.h"
#include "services/ui/public/cpp/window_observer.h"
#include "services/ui/public/interfaces/window_manager_constants.mojom.h"
#include "ui/display/display.h"

namespace gfx {
class Insets;
}

namespace service_manager {
class Connector;
}

namespace ash {

namespace mus {

class LayoutManager;
class WindowManager;
class WmRootWindowControllerMus;
class WmShelfMus;
class WmTestBase;
class WmTestHelper;
class WmWindowMus;

// RootWindowController manages the windows and state for a single display.
// RootWindowController takes ownership of the Window that it passed to it.
class RootWindowController {
 public:
  RootWindowController(WindowManager* window_manager,
                       ui::Window* root,
                       const display::Display& display);
  ~RootWindowController();

  void Shutdown();

  service_manager::Connector* GetConnector();

  ui::Window* root() { return root_; }
  WmRootWindowControllerMus* wm_root_window_controller() {
    return wm_root_window_controller_.get();
  }

  ui::Window* NewTopLevelWindow(
      std::map<std::string, std::vector<uint8_t>>* properties);

  WmWindowMus* GetWindowByShellWindowId(int id);

  void SetWorkAreaInests(const gfx::Insets& insets);
  void SetDisplay(const display::Display& display);

  WindowManager* window_manager() { return window_manager_; }

  const display::Display& display() const { return display_; }

  WmShelfMus* wm_shelf() { return wm_shelf_.get(); }

 private:
  friend class WmTestBase;
  friend class WmTestHelper;

  gfx::Rect CalculateDefaultBounds(ui::Window* window) const;
  gfx::Rect GetMaximizedWindowBounds() const;

  // Creates the necessary set of layout managers in the shell windows.
  void CreateLayoutManagers();

  WindowManager* window_manager_;
  ui::Window* root_;
  int window_count_ = 0;

  display::Display display_;

  std::unique_ptr<WmRootWindowControllerMus> wm_root_window_controller_;
  std::unique_ptr<WmShelfMus> wm_shelf_;

  std::map<ui::Window*, std::unique_ptr<LayoutManager>> layout_managers_;

  std::unique_ptr<DisconnectedAppHandler> disconnected_app_handler_;

  DISALLOW_COPY_AND_ASSIGN(RootWindowController);
};

}  // namespace mus
}  // namespace ash

#endif  // ASH_MUS_ROOT_WINDOW_CONTROLLER_H_
