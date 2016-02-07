// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MASH_WM_WINDOW_MANAGER_APPLICATION_H_
#define MASH_WM_WINDOW_MANAGER_APPLICATION_H_

#include <stdint.h>

#include <set>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "components/mus/common/types.h"
#include "components/mus/public/interfaces/accelerator_registrar.mojom.h"
#include "components/mus/public/interfaces/window_manager.mojom.h"
#include "components/mus/public/interfaces/window_manager_factory.mojom.h"
#include "components/mus/public/interfaces/window_tree_host.mojom.h"
#include "mash/wm/public/interfaces/user_window_controller.mojom.h"
#include "mojo/common/weak_binding_set.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/services/tracing/public/cpp/tracing_impl.h"
#include "mojo/shell/public/cpp/application_delegate.h"

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
class RootWindowController;
class RootWindowsObserver;
class UserWindowControllerImpl;

class WindowManagerApplication
    : public mojo::ApplicationDelegate,
      public mus::mojom::WindowManagerFactory,
      public mojo::InterfaceFactory<mash::wm::mojom::UserWindowController>,
      public mojo::InterfaceFactory<mus::mojom::AcceleratorRegistrar> {
 public:
  WindowManagerApplication();
  ~WindowManagerApplication() override;

  mojo::Shell* shell() { return shell_; }

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
  void OnAccelerator(uint32_t id, mus::mojom::EventPtr event);

  void AddRootWindowsObserver(RootWindowsObserver* observer);
  void RemoveRootWindowsObserver(RootWindowsObserver* observer);

 private:
  void OnAcceleratorRegistrarDestroyed(AcceleratorRegistrarImpl* registrar);

  // ApplicationDelegate:
  void Initialize(mojo::Shell* shell, const std::string& url,
                  uint32_t id) override;
  bool AcceptConnection(
      mojo::ApplicationConnection* connection) override;

  // InterfaceFactory<mash::wm::mojom::UserWindowController>:
  void Create(mojo::ApplicationConnection* connection,
              mojo::InterfaceRequest<mash::wm::mojom::UserWindowController>
                  request) override;

  // InterfaceFactory<mus::mojom::AcceleratorRegistrar>:
  void Create(mojo::ApplicationConnection* connection,
              mojo::InterfaceRequest<mus::mojom::AcceleratorRegistrar> request)
      override;

  // mus::mojom::WindowManagerFactory:
  void CreateWindowManager(mus::mojom::DisplayPtr display,
                           mojo::InterfaceRequest<mus::mojom::WindowTreeClient>
                               client_request) override;

  mojo::Shell* shell_;

  mojo::TracingImpl tracing_;

  scoped_ptr<ui::mojo::UIInit> ui_init_;
  scoped_ptr<views::AuraInit> aura_init_;

  // |user_window_controller_| is created once OnEmbed() is called. Until that
  // time |user_window_controller_requests_| stores pending interface requests.
  scoped_ptr<UserWindowControllerImpl> user_window_controller_;
  mojo::WeakBindingSet<mash::wm::mojom::UserWindowController>
      user_window_controller_binding_;
  std::vector<
      scoped_ptr<mojo::InterfaceRequest<mash::wm::mojom::UserWindowController>>>
      user_window_controller_requests_;

  std::set<AcceleratorRegistrarImpl*> accelerator_registrars_;
  std::set<RootWindowController*> root_controllers_;

  mojo::Binding<mus::mojom::WindowManagerFactory>
      window_manager_factory_binding_;

  base::ObserverList<RootWindowsObserver> root_windows_observers_;

  DISALLOW_COPY_AND_ASSIGN(WindowManagerApplication);
};

}  // namespace wm
}  // namespace mash

#endif  // MASH_WM_WINDOW_MANAGER_APPLICATION_H_
