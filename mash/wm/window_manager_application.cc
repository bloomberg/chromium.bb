// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/wm/window_manager_application.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "components/mus/common/event_matcher_util.h"
#include "components/mus/public/cpp/window.h"
#include "components/mus/public/interfaces/window_manager_factory.mojom.h"
#include "mash/wm/accelerator_registrar_impl.h"
#include "mash/wm/root_window_controller.h"
#include "mash/wm/root_windows_observer.h"
#include "mash/wm/shelf_layout_impl.h"
#include "mash/wm/user_window_controller_impl.h"
#include "mojo/converters/input_events/input_events_type_converters.h"
#include "services/shell/public/cpp/connection.h"
#include "services/shell/public/cpp/connector.h"
#include "services/tracing/public/cpp/tracing_impl.h"
#include "ui/events/event.h"
#include "ui/views/mus/aura_init.h"
#include "ui/views/mus/display_converter.h"
#include "ui/views/mus/screen_mus.h"

namespace mash {
namespace wm {

WindowManagerApplication::WindowManagerApplication()
    : connector_(nullptr), window_manager_factory_binding_(this) {}

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
  aura_init_.reset(new views::AuraInit(connector_, "mash_wm_resources.pak"));
}

void WindowManagerApplication::OnRootWindowControllerDoneInit(
    RootWindowController* root_controller) {
  screen_.reset(new views::ScreenMus(nullptr));
  screen_->Init(connector_);

  // TODO(msw): figure out if this should be per display, or global.
  user_window_controller_->Initialize(root_controller);
  for (auto& request : user_window_controller_requests_)
    user_window_controller_bindings_.AddBinding(user_window_controller_.get(),
                                                std::move(*request));
  user_window_controller_requests_.clear();

  // TODO(msw): figure out if this should be per display, or global.
  shelf_layout_->Initialize(root_controller);
  for (auto& request : shelf_layout_requests_)
    shelf_layout_bindings_.AddBinding(shelf_layout_.get(), std::move(*request));
  shelf_layout_requests_.clear();

  FOR_EACH_OBSERVER(RootWindowsObserver, root_windows_observers_,
                    OnRootWindowControllerAdded(root_controller));
}

void WindowManagerApplication::OnRootWindowDestroyed(
    RootWindowController* root_controller) {
  root_controllers_.erase(root_controller);
  user_window_controller_.reset(nullptr);
}

void WindowManagerApplication::OnAccelerator(uint32_t id,
                                             const ui::Event& event) {
  for (auto* registrar : accelerator_registrars_) {
    if (registrar->OwnsAccelerator(id)) {
      registrar->ProcessAccelerator(id, mus::mojom::Event::From(event));
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

void WindowManagerApplication::Initialize(shell::Connector* connector,
                                          const shell::Identity& identity,
                                          uint32_t id) {
  connector_ = connector;
  tracing_.Initialize(connector, identity.name());

  mus::mojom::WindowManagerFactoryServicePtr wm_factory_service;
  connector_->ConnectToInterface("mojo:mus", &wm_factory_service);
  wm_factory_service->SetWindowManagerFactory(
      window_manager_factory_binding_.CreateInterfacePtrAndBind());

  shelf_layout_.reset(new ShelfLayoutImpl);
  user_window_controller_.reset(new UserWindowControllerImpl());
}

bool WindowManagerApplication::AcceptConnection(shell::Connection* connection) {
  connection->AddInterface<mojom::ShelfLayout>(this);
  connection->AddInterface<mojom::UserWindowController>(this);
  connection->AddInterface<mus::mojom::AcceleratorRegistrar>(this);
  if (connection->GetRemoteIdentity().name() == "mojo:mash_session")
    connection->GetInterface(&session_);
  return true;
}

void WindowManagerApplication::Create(
    shell::Connection* connection,
    mojo::InterfaceRequest<mojom::ShelfLayout> request) {
  // TODO(msw): Handle multiple shelves (one per display).
  if (!root_controllers_.empty() && (*root_controllers_.begin())->root()) {
    shelf_layout_bindings_.AddBinding(shelf_layout_.get(), std::move(request));
  } else {
    shelf_layout_requests_.push_back(base::WrapUnique(
        new mojo::InterfaceRequest<mojom::ShelfLayout>(std::move(request))));
  }
}

void WindowManagerApplication::Create(
    shell::Connection* connection,
    mojo::InterfaceRequest<mojom::UserWindowController> request) {
  if (!root_controllers_.empty() && (*root_controllers_.begin())->root()) {
    user_window_controller_bindings_.AddBinding(user_window_controller_.get(),
                                                std::move(request));
  } else {
    user_window_controller_requests_.push_back(base::WrapUnique(
        new mojo::InterfaceRequest<mojom::UserWindowController>(
            std::move(request))));
  }
}

void WindowManagerApplication::Create(
    shell::Connection* connection,
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
