// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MASH_WM_ROOT_WINDOW_CONTROLLER_H_
#define MASH_WM_ROOT_WINDOW_CONTROLLER_H_

#include "components/mus/public/cpp/window_observer.h"
#include "components/mus/public/cpp/window_tree_delegate.h"
#include "components/mus/public/interfaces/window_manager_constants.mojom.h"
#include "components/mus/public/interfaces/window_tree_host.mojom.h"
#include "mash/wm/public/interfaces/container.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace mojo {
class Shell;
}

namespace mash {
namespace wm {

class BackgroundLayout;
class ScreenlockLayout;
class ShadowController;
class ShelfLayout;
class WindowLayout;
class WindowManager;
class WindowManagerApplication;

// RootWindowController manages the windows and state for a single display.
//
// RootWindowController deletes itself when the root mus::Window is destroyed.
// You can trigger deletion explicitly by way of Destroy().
class RootWindowController : public mus::WindowObserver,
                             public mus::mojom::WindowTreeHostClient,
                             public mus::WindowTreeDelegate {
 public:
  static RootWindowController* CreateUsingWindowTreeHost(
      WindowManagerApplication* app);
  static RootWindowController* CreateFromDisplay(
      WindowManagerApplication* app,
      mus::mojom::DisplayPtr display,
      mojo::InterfaceRequest<mus::mojom::WindowTreeClient> client_request);

  // Deletes this.
  void Destroy();

  mojo::Shell* GetShell();

  mus::Window* root() { return root_; }

  int window_count() { return window_count_; }
  void IncrementWindowCount() { ++window_count_; }

  mus::Window* GetWindowForContainer(mojom::Container container);
  mus::Window* GetWindowById(mus::Id id);
  bool WindowIsContainer(const mus::Window* window) const;

  // TODO(sky): don't expose this, it may be null.
  mus::mojom::WindowTreeHost* window_tree_host() {
    return window_tree_host_.get();
  }

 private:
  explicit RootWindowController(WindowManagerApplication* app);
  ~RootWindowController() override;

  void AddAccelerators();

  // Sets up the window containers used for z-space management.
  void CreateContainers();

  // WindowTreeHostClient:
  void OnAccelerator(uint32_t id, mus::mojom::EventPtr event) override;

  // WindowTreeDelegate:
  void OnEmbed(mus::Window* root) override;
  void OnConnectionLost(mus::WindowTreeConnection* connection) override;

  // mus::WindowObserver:
  void OnWindowDestroyed(mus::Window* window) override;

  WindowManagerApplication* app_;
  mus::Window* root_;
  int window_count_;

  scoped_ptr<WindowManager> window_manager_;

  scoped_ptr<BackgroundLayout> background_layout_;
  scoped_ptr<ScreenlockLayout> screenlock_layout_;
  scoped_ptr<ShelfLayout> shelf_layout_;
  scoped_ptr<WindowLayout> window_layout_;

  scoped_ptr<ShadowController> shadow_controller_;

  mus::mojom::WindowTreeHostPtr window_tree_host_;
  mojo::Binding<mus::mojom::WindowTreeHostClient> host_client_binding_;

  mus::mojom::DisplayPtr display_;

  DISALLOW_COPY_AND_ASSIGN(RootWindowController);
};

}  // namespace wm
}  // namespace mash

#endif  // MASH_WM_ROOT_WINDOW_CONTROLLER_H_
