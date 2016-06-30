// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_WINDOW_MANAGER_APPLICATION_H_
#define ASH_MUS_WINDOW_MANAGER_APPLICATION_H_

#include <stdint.h>

#include <memory>
#include <set>

#include "ash/mus/window_manager_observer.h"
#include "ash/public/interfaces/shelf_layout.mojom.h"
#include "ash/public/interfaces/user_window_controller.mojom.h"
#include "base/macros.h"
#include "components/mus/common/types.h"
#include "components/mus/public/interfaces/accelerator_registrar.mojom.h"
#include "mash/session/public/interfaces/session.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/shell/public/cpp/shell_client.h"
#include "services/tracing/public/cpp/tracing_impl.h"

namespace mus {
class WindowTreeClient;
}

namespace views {
class AuraInit;
}

namespace ui {
class Event;
}

namespace ash {
namespace mus {

class AcceleratorRegistrarImpl;
class RootWindowController;
class ShelfLayoutImpl;
class UserWindowControllerImpl;
class WindowManager;

class WindowManagerApplication
    : public shell::ShellClient,
      public shell::InterfaceFactory<mojom::ShelfLayout>,
      public shell::InterfaceFactory<mojom::UserWindowController>,
      public shell::InterfaceFactory<::mus::mojom::AcceleratorRegistrar>,
      public mash::session::mojom::ScreenlockStateListener,
      public WindowManagerObserver {
 public:
  WindowManagerApplication();
  ~WindowManagerApplication() override;

  shell::Connector* connector() { return connector_; }

  WindowManager* window_manager() { return window_manager_.get(); }

  mash::session::mojom::Session* session() { return session_.get(); }

 private:
  friend class WmTestBase;
  friend class WmTestHelper;

  void OnAcceleratorRegistrarDestroyed(AcceleratorRegistrarImpl* registrar);

  void InitWindowManager(::mus::WindowTreeClient* window_tree_client);

  // shell::ShellClient:
  void Initialize(shell::Connector* connector,
                  const shell::Identity& identity,
                  uint32_t id) override;
  bool AcceptConnection(shell::Connection* connection) override;

  // shell::InterfaceFactory<mojom::ShelfLayout>:
  void Create(shell::Connection* connection,
              mojo::InterfaceRequest<mojom::ShelfLayout> request) override;

  // shell::InterfaceFactory<mojom::UserWindowController>:
  void Create(
      shell::Connection* connection,
      mojo::InterfaceRequest<mojom::UserWindowController> request) override;

  // shell::InterfaceFactory<mus::mojom::AcceleratorRegistrar>:
  void Create(shell::Connection* connection,
              mojo::InterfaceRequest<::mus::mojom::AcceleratorRegistrar>
                  request) override;

  // session::mojom::ScreenlockStateListener:
  void ScreenlockStateChanged(bool locked) override;

  // WindowManagerObserver:
  void OnRootWindowControllerAdded(RootWindowController* controller) override;
  void OnWillDestroyRootWindowController(
      RootWindowController* controller) override;

  shell::Connector* connector_;

  mojo::TracingImpl tracing_;

  std::unique_ptr<views::AuraInit> aura_init_;

  // The |shelf_layout_| object is created once OnEmbed() is called. Until that
  // time |shelf_layout_requests_| stores pending interface requests.
  std::unique_ptr<ShelfLayoutImpl> shelf_layout_;
  mojo::BindingSet<mojom::ShelfLayout> shelf_layout_bindings_;
  std::vector<mojo::InterfaceRequest<mojom::ShelfLayout>>
      shelf_layout_requests_;

  // |user_window_controller_| is created once OnEmbed() is called. Until that
  // time |user_window_controller_requests_| stores pending interface requests.
  std::unique_ptr<UserWindowControllerImpl> user_window_controller_;
  mojo::BindingSet<mojom::UserWindowController>
      user_window_controller_bindings_;
  std::vector<mojo::InterfaceRequest<mojom::UserWindowController>>
      user_window_controller_requests_;

  std::unique_ptr<WindowManager> window_manager_;

  std::set<AcceleratorRegistrarImpl*> accelerator_registrars_;

  mash::session::mojom::SessionPtr session_;

  mojo::Binding<mash::session::mojom::ScreenlockStateListener>
      screenlock_state_listener_binding_;

  DISALLOW_COPY_AND_ASSIGN(WindowManagerApplication);
};

}  // namespace mus
}  // namespace ash

#endif  // ASH_MUS_WINDOW_MANAGER_APPLICATION_H_
