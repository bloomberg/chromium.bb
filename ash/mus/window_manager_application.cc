// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/window_manager_application.h"

#include <utility>

#include "ash/mus/accelerator_registrar_impl.h"
#include "ash/mus/root_window_controller.h"
#include "ash/mus/shelf_layout_impl.h"
#include "ash/mus/user_window_controller_impl.h"
#include "ash/mus/window_manager.h"
#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "services/shell/public/cpp/connection.h"
#include "services/shell/public/cpp/connector.h"
#include "services/tracing/public/cpp/tracing_impl.h"
#include "services/ui/common/event_matcher_util.h"
#include "services/ui/common/gpu_service.h"
#include "services/ui/public/cpp/window.h"
#include "services/ui/public/cpp/window_tree_client.h"
#include "ui/events/event.h"
#include "ui/views/mus/aura_init.h"

#if defined(OS_CHROMEOS)
#include "chromeos/dbus/dbus_thread_manager.h"
#endif

namespace ash {
namespace mus {

WindowManagerApplication::WindowManagerApplication()
    : connector_(nullptr), screenlock_state_listener_binding_(this) {}

WindowManagerApplication::~WindowManagerApplication() {
  // AcceleratorRegistrarImpl removes an observer in its destructor. Destroy
  // it early on.
  std::set<AcceleratorRegistrarImpl*> accelerator_registrars(
      accelerator_registrars_);
  for (AcceleratorRegistrarImpl* registrar : accelerator_registrars)
    registrar->Destroy();

  // Destroy the WindowManager while still valid. This way we ensure
  // OnWillDestroyRootWindowController() is called (if it hasn't been already).
  window_manager_.reset();

#if defined(OS_CHROMEOS)
  chromeos::DBusThreadManager::Shutdown();
#endif
}

void WindowManagerApplication::OnAcceleratorRegistrarDestroyed(
    AcceleratorRegistrarImpl* registrar) {
  accelerator_registrars_.erase(registrar);
}

void WindowManagerApplication::InitWindowManager(
    ::ui::WindowTreeClient* window_tree_client) {
#if defined(OS_CHROMEOS)
  // Must occur after mojo::ApplicationRunner has initialized AtExitManager, but
  // before WindowManager::Init().
  chromeos::DBusThreadManager::Initialize();
#endif
  window_manager_->Init(window_tree_client);
  window_manager_->AddObserver(this);
}

void WindowManagerApplication::OnStart(shell::Connector* connector,
                                       const shell::Identity& identity,
                                       uint32_t id) {
  connector_ = connector;
  ::ui::GpuService::Initialize(connector);
  window_manager_.reset(new WindowManager(connector_));

  aura_init_.reset(new views::AuraInit(connector_, "ash_mus_resources.pak"));

  tracing_.Initialize(connector, identity.name());

  ::ui::WindowTreeClient* window_tree_client = new ::ui::WindowTreeClient(
      window_manager_.get(), window_manager_.get(), nullptr);
  window_tree_client->ConnectAsWindowManager(connector);

  InitWindowManager(window_tree_client);
}

bool WindowManagerApplication::OnConnect(shell::Connection* connection) {
  connection->AddInterface<mojom::ShelfLayout>(this);
  connection->AddInterface<mojom::UserWindowController>(this);
  connection->AddInterface<::ui::mojom::AcceleratorRegistrar>(this);
  if (connection->GetRemoteIdentity().name() == "mojo:mash_session") {
    connection->GetInterface(&session_);
    session_->AddScreenlockStateListener(
        screenlock_state_listener_binding_.CreateInterfacePtrAndBind());
  }
  return true;
}

void WindowManagerApplication::Create(
    shell::Connection* connection,
    mojo::InterfaceRequest<mojom::ShelfLayout> request) {
  // TODO(msw): Handle multiple shelves (one per display).
  if (!window_manager_->GetRootWindowControllers().empty()) {
    shelf_layout_bindings_.AddBinding(shelf_layout_.get(), std::move(request));
  } else {
    shelf_layout_requests_.push_back(std::move(request));
  }
}

void WindowManagerApplication::Create(
    shell::Connection* connection,
    mojo::InterfaceRequest<mojom::UserWindowController> request) {
  if (!window_manager_->GetRootWindowControllers().empty()) {
    user_window_controller_bindings_.AddBinding(user_window_controller_.get(),
                                                std::move(request));
  } else {
    user_window_controller_requests_.push_back(std::move(request));
  }
}

void WindowManagerApplication::Create(
    shell::Connection* connection,
    mojo::InterfaceRequest<::ui::mojom::AcceleratorRegistrar> request) {
  if (!window_manager_->window_manager_client())
    return;  // Can happen during shutdown.

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
      window_manager_.get(), ++accelerator_registrar_count, std::move(request),
      base::Bind(&WindowManagerApplication::OnAcceleratorRegistrarDestroyed,
                 base::Unretained(this))));
}

void WindowManagerApplication::ScreenlockStateChanged(bool locked) {
  window_manager_->SetScreenLocked(locked);
}

void WindowManagerApplication::OnRootWindowControllerAdded(
    RootWindowController* controller) {
  if (user_window_controller_)
    return;

  // TODO(sky): |shelf_layout_| and |user_window_controller_| should really
  // be owned by WindowManager and/or RootWindowController. But this code is
  // temporary while migrating away from sysui.

  shelf_layout_.reset(new ShelfLayoutImpl);
  user_window_controller_.reset(new UserWindowControllerImpl());

  // TODO(msw): figure out if this should be per display, or global.
  user_window_controller_->Initialize(controller);
  for (auto& request : user_window_controller_requests_)
    user_window_controller_bindings_.AddBinding(user_window_controller_.get(),
                                                std::move(request));
  user_window_controller_requests_.clear();

  // TODO(msw): figure out if this should be per display, or global.
  shelf_layout_->Initialize(controller);
  for (auto& request : shelf_layout_requests_)
    shelf_layout_bindings_.AddBinding(shelf_layout_.get(), std::move(request));
  shelf_layout_requests_.clear();
}

void WindowManagerApplication::OnWillDestroyRootWindowController(
    RootWindowController* controller) {
  // TODO(msw): this isn't right, ownership should belong in WindowManager
  // and/or RootWindowController. But this is temporary until we get rid of
  // sysui.
  shelf_layout_.reset();
  user_window_controller_.reset();
}

}  // namespace mus
}  // namespace ash
