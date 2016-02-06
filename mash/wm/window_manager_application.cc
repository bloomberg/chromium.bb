// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/wm/window_manager_application.h"

#include <utility>

#include "base/bind.h"
#include "components/mus/public/cpp/event_matcher.h"
#include "components/mus/public/cpp/window.h"
#include "components/mus/public/interfaces/window_manager_factory.mojom.h"
#include "mash/wm/accelerator_registrar_impl.h"
#include "mash/wm/root_window_controller.h"
#include "mash/wm/root_windows_observer.h"
#include "mash/wm/user_window_controller_impl.h"
#include "mojo/services/tracing/public/cpp/tracing_impl.h"
#include "mojo/shell/public/cpp/application_connection.h"
#include "mojo/shell/public/cpp/application_impl.h"
#include "ui/mojo/init/ui_init.h"
#include "ui/views/mus/aura_init.h"
#include "ui/views/mus/display_converter.h"

namespace mash {
namespace wm {

WindowManagerApplication::WindowManagerApplication()
    : app_(nullptr), window_manager_factory_binding_(this) {}

WindowManagerApplication::~WindowManagerApplication() {
  // AcceleratorRegistrarImpl removes an observer in its destructor. Destroy
  // it early on.
  std::set<AcceleratorRegistrarImpl*> accelerator_registrars(
      accelerator_registrars_);
  for (AcceleratorRegistrarImpl* registrar : accelerator_registrars)
    registrar->Destroy();

  std::set<RootWindowController*> controllers(root_controllers_);
  for (RootWindowController* controller : controllers)
    controller->Destroy();
}

std::set<RootWindowController*> WindowManagerApplication::GetRootControllers() {
  std::set<RootWindowController*> root_controllers;
  for (RootWindowController* controller : root_controllers_) {
    if (controller->root())
      root_controllers.insert(controller);
  }
  return root_controllers;
}

void WindowManagerApplication::OnRootWindowControllerGotRoot(
    RootWindowController* root_controller) {
  if (ui_init_.get())
    return;

  ui_init_.reset(new ui::mojo::UIInit(
      views::GetDisplaysFromWindow(root_controller->root())));
  aura_init_.reset(new views::AuraInit(app_, "mash_wm_resources.pak"));
}

void WindowManagerApplication::OnRootWindowControllerDoneInit(
    RootWindowController* root_controller) {
  // TODO(msw): figure out if this should be per display, or global.
  user_window_controller_->Initialize(root_controller);
  for (auto& request : user_window_controller_requests_)
    user_window_controller_binding_.AddBinding(user_window_controller_.get(),
                                               std::move(*request));
  user_window_controller_requests_.clear();

  FOR_EACH_OBSERVER(RootWindowsObserver, root_windows_observers_,
                    OnRootWindowControllerAdded(root_controller));
}

void WindowManagerApplication::OnRootWindowDestroyed(
    RootWindowController* root_controller) {
  root_controllers_.erase(root_controller);
  user_window_controller_.reset(nullptr);
}

void WindowManagerApplication::OnAccelerator(uint32_t id,
                                             mus::mojom::EventPtr event) {
  for (auto* registrar : accelerator_registrars_) {
    if (registrar->OwnsAccelerator(id)) {
      registrar->ProcessAccelerator(id, std::move(event));
      break;
    }
  }
}

void WindowManagerApplication::AddRootWindowsObserver(
    RootWindowsObserver* observer) {
  root_windows_observers_.AddObserver(observer);
}

void WindowManagerApplication::RemoveRootWindowsObserver(
    RootWindowsObserver* observer) {
  root_windows_observers_.RemoveObserver(observer);
}

void WindowManagerApplication::OnAcceleratorRegistrarDestroyed(
    AcceleratorRegistrarImpl* registrar) {
  accelerator_registrars_.erase(registrar);
}

void WindowManagerApplication::Initialize(mojo::ApplicationImpl* app) {
  app_ = app;
  tracing_.Initialize(app);

  mus::mojom::WindowManagerFactoryServicePtr wm_factory_service;
  app_->ConnectToService("mojo:mus", &wm_factory_service);
  wm_factory_service->SetWindowManagerFactory(
      window_manager_factory_binding_.CreateInterfacePtrAndBind());

  user_window_controller_.reset(new UserWindowControllerImpl());

  root_controllers_.insert(
      RootWindowController::CreateUsingWindowTreeHost(this));
}

bool WindowManagerApplication::AcceptConnection(
    mojo::ApplicationConnection* connection) {
  connection->AddService<mash::wm::mojom::UserWindowController>(this);
  connection->AddService<mus::mojom::AcceleratorRegistrar>(this);
  return true;
}

void WindowManagerApplication::Create(
    mojo::ApplicationConnection* connection,
    mojo::InterfaceRequest<mash::wm::mojom::UserWindowController> request) {
  if (!root_controllers_.empty() && (*root_controllers_.begin())->root()) {
    user_window_controller_binding_.AddBinding(user_window_controller_.get(),
                                               std::move(request));
  } else {
    user_window_controller_requests_.push_back(make_scoped_ptr(
        new mojo::InterfaceRequest<mash::wm::mojom::UserWindowController>(
            std::move(request))));
  }
}

void WindowManagerApplication::Create(
    mojo::ApplicationConnection* connection,
    mojo::InterfaceRequest<mus::mojom::AcceleratorRegistrar> request) {
  static int accelerator_registrar_count = 0;
  if (accelerator_registrar_count == std::numeric_limits<int>::max()) {
    // Restart from zero if we have reached the limit. It is technically
    // possible to end up with multiple active registrars with the same
    // namespace, but it is highly unlikely. In the event that multiple
    // registrars have the same namespace, this new registrar will be unable to
    // install accelerators.
    accelerator_registrar_count = 0;
  }
  accelerator_registrars_.insert(new AcceleratorRegistrarImpl(
      this, ++accelerator_registrar_count, std::move(request),
      base::Bind(&WindowManagerApplication::OnAcceleratorRegistrarDestroyed,
                 base::Unretained(this))));
}

void WindowManagerApplication::CreateWindowManager(
    mus::mojom::DisplayPtr display,
    mojo::InterfaceRequest<mus::mojom::WindowTreeClient> client_request) {
  root_controllers_.insert(RootWindowController::CreateFromDisplay(
      this, std::move(display), std::move(client_request)));
}

}  // namespace wm
}  // namespace mash
