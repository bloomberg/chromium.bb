// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MASH_WM_WINDOW_MANAGER_APPLICATION_H_
#define MASH_WM_WINDOW_MANAGER_APPLICATION_H_

#include <stdint.h>

#include <set>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "components/mus/common/types.h"
#include "components/mus/public/cpp/window_observer.h"
#include "components/mus/public/cpp/window_tree_delegate.h"
#include "components/mus/public/interfaces/accelerator_registrar.mojom.h"
#include "components/mus/public/interfaces/window_manager.mojom.h"
#include "components/mus/public/interfaces/window_tree_host.mojom.h"
#include "mash/wm/public/interfaces/container.mojom.h"
#include "mash/wm/public/interfaces/user_window_controller.mojom.h"
#include "mojo/common/weak_binding_set.h"
#include "mojo/services/tracing/public/cpp/tracing_impl.h"
#include "mojo/shell/public/cpp/application_delegate.h"
#include "mojo/shell/public/cpp/interface_factory_impl.h"

namespace ui {
namespace mojo {
class UIInit;
}
}

namespace views {
class AuraInit;
}

namespace mash {
namespace wm {

class AcceleratorRegistrarImpl;
class BackgroundLayout;
class ScreenlockLayout;
class ShadowController;
class ShelfLayout;
class UserWindowControllerImpl;
class WindowLayout;
class WindowManagerImpl;

class WindowManagerApplication
    : public mojo::ApplicationDelegate,
      public mus::WindowObserver,
      public mus::mojom::WindowTreeHostClient,
      public mus::WindowTreeDelegate,
      public mojo::InterfaceFactory<mash::wm::mojom::UserWindowController>,
      public mojo::InterfaceFactory<mus::mojom::WindowManagerDeprecated>,
      public mojo::InterfaceFactory<mus::mojom::AcceleratorRegistrar> {
 public:
  WindowManagerApplication();
  ~WindowManagerApplication() override;

  mus::Window* root() { return root_; }

  int window_count() { return window_count_; }
  void IncrementWindowCount() { ++window_count_; }

  mus::Window* GetWindowForContainer(mojom::Container container);
  mus::Window* GetWindowById(mus::Id id);
  bool WindowIsContainer(const mus::Window* window) const;

  mojo::ApplicationImpl* app() { return app_; }

  mus::mojom::WindowTreeHost* window_tree_host() {
    return window_tree_host_.get();
  }

 private:
  void AddAccelerators();
  void OnAcceleratorRegistrarDestroyed(AcceleratorRegistrarImpl* registrar);

  // ApplicationDelegate:
  void Initialize(mojo::ApplicationImpl* app) override;
  bool ConfigureIncomingConnection(
      mojo::ApplicationConnection* connection) override;

  // WindowTreeHostClient:
  void OnAccelerator(uint32_t id, mus::mojom::EventPtr event) override;

  // WindowTreeDelegate:
  void OnEmbed(mus::Window* root) override;
  void OnConnectionLost(mus::WindowTreeConnection* connection) override;

  // InterfaceFactory<mash::wm::mojom::UserWindowController>:
  void Create(mojo::ApplicationConnection* connection,
              mojo::InterfaceRequest<mash::wm::mojom::UserWindowController>
                  request) override;

  // InterfaceFactory<mus::mojom::AcceleratorRegistrar>:
  void Create(mojo::ApplicationConnection* connection,
              mojo::InterfaceRequest<mus::mojom::AcceleratorRegistrar> request)
      override;

  // InterfaceFactory<mus::mojom::WindowManagerDeprecated>:
  void Create(mojo::ApplicationConnection* connection,
              mojo::InterfaceRequest<mus::mojom::WindowManagerDeprecated>
                  request) override;

  // mus::WindowObserver:
  void OnWindowDestroyed(mus::Window* window) override;

  // Sets up the window containers used for z-space management.
  void CreateContainers();

  // nullptr until the Mus connection is established via OnEmbed().
  mus::Window* root_;
  int window_count_;

  mojo::ApplicationImpl* app_;

  mojo::TracingImpl tracing_;

  mus::mojom::WindowTreeHostPtr window_tree_host_;
  mojo::Binding<mus::mojom::WindowTreeHostClient> host_client_binding_;

  scoped_ptr<ui::mojo::UIInit> ui_init_;
  scoped_ptr<views::AuraInit> aura_init_;

  // |window_manager_| is created once OnEmbed() is called. Until that time
  // |requests_| stores any pending WindowManager interface requests.
  scoped_ptr<WindowManagerImpl> window_manager_;
  mojo::WeakBindingSet<mus::mojom::WindowManagerDeprecated>
      window_manager_binding_;
  std::vector<
      scoped_ptr<mojo::InterfaceRequest<mus::mojom::WindowManagerDeprecated>>>
      requests_;

  // |user_window_controller_| is created once OnEmbed() is called. Until that
  // time |user_window_controller_requests_| stores pending interface requests.
  scoped_ptr<UserWindowControllerImpl> user_window_controller_;
  mojo::WeakBindingSet<mash::wm::mojom::UserWindowController>
      user_window_controller_binding_;
  std::vector<
      scoped_ptr<mojo::InterfaceRequest<mash::wm::mojom::UserWindowController>>>
      user_window_controller_requests_;

  scoped_ptr<BackgroundLayout> background_layout_;
  scoped_ptr<ScreenlockLayout> screenlock_layout_;
  scoped_ptr<ShelfLayout> shelf_layout_;
  scoped_ptr<WindowLayout> window_layout_;

  scoped_ptr<ShadowController> shadow_controller_;

  std::set<AcceleratorRegistrarImpl*> accelerator_registrars_;

  DISALLOW_COPY_AND_ASSIGN(WindowManagerApplication);
};

}  // namespace wm
}  // namespace mash

#endif  // MASH_WM_WINDOW_MANAGER_APPLICATION_H_
