// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MASH_WM_ROOT_WINDOW_CONTROLLER_H_
#define MASH_WM_ROOT_WINDOW_CONTROLLER_H_

#include <memory>

#include "components/mus/public/cpp/window_observer.h"
#include "components/mus/public/cpp/window_tree_delegate.h"
#include "components/mus/public/interfaces/window_manager_constants.mojom.h"
#include "components/mus/public/interfaces/window_tree_host.mojom.h"
#include "mash/wm/public/interfaces/container.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "ui/display/display.h"

namespace ash {
class AlwaysOnTopController;
}

namespace mus {
class WindowManagerClient;
}

namespace shell {
class Connector;
}

namespace ui {
class Event;
}

namespace mash {
namespace wm {

class LayoutManager;
class ShadowController;
class ShelfLayoutManager;
class StatusLayoutManager;
class WindowManager;
class WindowManagerApplication;
class WmRootWindowControllerMus;
class WmShelfMus;
class WmTestBase;
class WmTestHelper;

// RootWindowController manages the windows and state for a single display.
//
// RootWindowController deletes itself when the root mus::Window is destroyed.
// You can trigger deletion explicitly by way of Destroy().
class RootWindowController : public mus::WindowObserver,
                             public mus::WindowTreeDelegate {
 public:
  static RootWindowController* CreateFromDisplay(
      WindowManagerApplication* app,
      mus::mojom::DisplayPtr display,
      mojo::InterfaceRequest<mus::mojom::WindowTreeClient> client_request);

  // Deletes this.
  void Destroy();

  shell::Connector* GetConnector();

  mus::Window* root() { return root_; }

  int window_count() { return window_count_; }
  void IncrementWindowCount() { ++window_count_; }

  mus::Window* GetWindowForContainer(mojom::Container container);
  bool WindowIsContainer(const mus::Window* window) const;

  WindowManager* window_manager() { return window_manager_.get(); }

  mus::WindowManagerClient* window_manager_client();

  void OnAccelerator(uint32_t id, const ui::Event& event);

  const display::Display& display() const { return display_; }

  ShelfLayoutManager* GetShelfLayoutManager();
  StatusLayoutManager* GetStatusLayoutManager();

  ash::AlwaysOnTopController* always_on_top_controller() {
    return always_on_top_controller_.get();
  }

  WmShelfMus* wm_shelf() { return wm_shelf_.get(); }

 private:
  friend class WmTestBase;
  friend class WmTestHelper;

  explicit RootWindowController(WindowManagerApplication* app);
  ~RootWindowController() override;

  void AddAccelerators();

  // WindowTreeDelegate:
  void OnEmbed(mus::Window* root) override;
  void OnConnectionLost(mus::WindowTreeConnection* connection) override;
  void OnEventObserved(const ui::Event& event, mus::Window* target) override;

  // mus::WindowObserver:
  void OnWindowDestroyed(mus::Window* window) override;

  // Sets up the window containers used for z-space management.
  void CreateContainer(mash::wm::mojom::Container container,
                       mash::wm::mojom::Container parent_container);
  void CreateContainers();

  WindowManagerApplication* app_;
  mus::Window* root_;
  int window_count_;

  std::unique_ptr<WmRootWindowControllerMus> wm_root_window_controller_;
  std::unique_ptr<WmShelfMus> wm_shelf_;

  std::unique_ptr<WindowManager> window_manager_;

  std::map<mus::Window*, std::unique_ptr<LayoutManager>> layout_managers_;

  std::unique_ptr<ShadowController> shadow_controller_;

  display::Display display_;

  std::unique_ptr<ash::AlwaysOnTopController> always_on_top_controller_;

  DISALLOW_COPY_AND_ASSIGN(RootWindowController);
};

}  // namespace wm
}  // namespace mash

#endif  // MASH_WM_ROOT_WINDOW_CONTROLLER_H_
