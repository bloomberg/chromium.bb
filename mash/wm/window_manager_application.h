// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MASH_WM_WINDOW_MANAGER_APPLICATION_H_
#define MASH_WM_WINDOW_MANAGER_APPLICATION_H_

#include <stdint.h>

#include <memory>
#include <set>

#include "base/macros.h"
#include "base/observer_list.h"
#include "components/mus/common/types.h"
#include "components/mus/public/interfaces/accelerator_registrar.mojom.h"
#include "components/mus/public/interfaces/window_manager.mojom.h"
#include "components/mus/public/interfaces/window_manager_factory.mojom.h"
#include "components/mus/public/interfaces/window_tree_host.mojom.h"
#include "mash/session/public/interfaces/session.mojom.h"
#include "mash/wm/public/interfaces/shelf_layout.mojom.h"
#include "mash/wm/public/interfaces/user_window_controller.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/shell/public/cpp/shell_client.h"
#include "services/tracing/public/cpp/tracing_impl.h"

namespace views {
class AuraInit;
class ScreenMus;
}

namespace ui {
class Event;
}

namespace mash {
namespace wm {

class AcceleratorRegistrarImpl;
class RootWindowController;
class RootWindowsObserver;
class UserWindowControllerImpl;

class WindowManagerApplication
    : public shell::ShellClient,
      public mus::mojom::WindowManagerFactory,
      public shell::InterfaceFactory<mojom::ShelfLayout>,
      public shell::InterfaceFactory<mojom::UserWindowController>,
      public shell::InterfaceFactory<mus::mojom::AcceleratorRegistrar> {
 public:
  WindowManagerApplication();
  ~WindowManagerApplication() override;

  shell::Connector* connector() { return connector_; }

  // Returns the RootWindowControllers that have valid roots.
  //
  // NOTE: this does not return |controllers_| as most clients want a
  // RootWindowController that has a valid root window.
  std::set<RootWindowController*> GetRootControllers();

  // Called when the root window of |root_controller| is obtained.
  void OnRootWindowControllerGotRoot(RootWindowController* root_controller);

  // Called after RootWindowController creates the necessary resources.
  void OnRootWindowControllerDoneInit(RootWindowController* root_controller);

  // Called when the root mus::Window of RootWindowController is destroyed.
  // |root_controller| is destroyed after this call.
  void OnRootWindowDestroyed(RootWindowController* root_controller);

  // TODO(sky): figure out right place for this code.
  void OnAccelerator(uint32_t id, const ui::Event& event);

  void AddRootWindowsObserver(RootWindowsObserver* observer);
  void RemoveRootWindowsObserver(RootWindowsObserver* observer);

  session::mojom::Session* session() {
    return session_.get();
  }

 private:
  void OnAcceleratorRegistrarDestroyed(AcceleratorRegistrarImpl* registrar);

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
              mojo::InterfaceRequest<mus::mojom::AcceleratorRegistrar> request)
      override;

  // mus::mojom::WindowManagerFactory:
  void CreateWindowManager(mus::mojom::DisplayPtr display,
                           mojo::InterfaceRequest<mus::mojom::WindowTreeClient>
                               client_request) override;

  shell::Connector* connector_;

  mojo::TracingImpl tracing_;

  std::unique_ptr<views::ScreenMus> screen_;
  std::unique_ptr<views::AuraInit> aura_init_;

  // The ShelfLayout object is created once OnEmbed() is called. Until that
  // time |shelf_layout_requests_| stores pending interface requests.
  mojo::BindingSet<mojom::ShelfLayout> shelf_layout_bindings_;
  std::vector<std::unique_ptr<mojo::InterfaceRequest<mojom::ShelfLayout>>>
      shelf_layout_requests_;

  // |user_window_controller_| is created once OnEmbed() is called. Until that
  // time |user_window_controller_requests_| stores pending interface requests.
  std::unique_ptr<UserWindowControllerImpl> user_window_controller_;
  mojo::BindingSet<mojom::UserWindowController>
      user_window_controller_bindings_;
  std::vector<
      std::unique_ptr<mojo::InterfaceRequest<mojom::UserWindowController>>>
      user_window_controller_requests_;

  std::set<AcceleratorRegistrarImpl*> accelerator_registrars_;
  std::set<RootWindowController*> root_controllers_;

  mojo::Binding<mus::mojom::WindowManagerFactory>
      window_manager_factory_binding_;

  mash::session::mojom::SessionPtr session_;

  base::ObserverList<RootWindowsObserver> root_windows_observers_;

  DISALLOW_COPY_AND_ASSIGN(WindowManagerApplication);
};

}  // namespace wm
}  // namespace mash

#endif  // MASH_WM_WINDOW_MANAGER_APPLICATION_H_
