// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/window_manager_application.h"

#include <utility>

#include "ash/common/material_design/material_design_controller.h"
#include "ash/mus/accelerators/accelerator_registrar_impl.h"
#include "ash/mus/native_widget_factory_mus.h"
#include "ash/mus/window_manager.h"
#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "services/shell/public/cpp/connection.h"
#include "services/shell/public/cpp/connector.h"
#include "services/tracing/public/cpp/provider.h"
#include "services/ui/common/event_matcher_util.h"
#include "services/ui/public/cpp/gpu_service.h"
#include "services/ui/public/cpp/window.h"
#include "services/ui/public/cpp/window_tree_client.h"
#include "ui/aura/env.h"
#include "ui/events/event.h"
#include "ui/message_center/message_center.h"
#include "ui/views/mus/aura_init.h"
#include "ui/views/mus/surface_context_factory.h"

#if defined(OS_CHROMEOS)
#include "ash/common/system/chromeos/power/power_status.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"  // nogncheck
#endif

namespace ash {
namespace mus {
namespace {

void InitializeComponents() {
  message_center::MessageCenter::Initialize();
#if defined(OS_CHROMEOS)
  // Must occur after mojo::ApplicationRunner has initialized AtExitManager, but
  // before WindowManager::Init().
  chromeos::DBusThreadManager::Initialize();
  bluez::BluezDBusManager::Initialize(
      chromeos::DBusThreadManager::Get()->GetSystemBus(),
      chromeos::DBusThreadManager::Get()->IsUsingStub(
          chromeos::DBusClientBundle::BLUETOOTH));
  // TODO(jamescook): Initialize real audio handler.
  chromeos::CrasAudioHandler::InitializeForTesting();
  PowerStatus::Initialize();
#endif
}

void ShutdownComponents() {
#if defined(OS_CHROMEOS)
  PowerStatus::Shutdown();
  chromeos::CrasAudioHandler::Shutdown();
  bluez::BluezDBusManager::Shutdown();
  chromeos::DBusThreadManager::Shutdown();
#endif
  message_center::MessageCenter::Shutdown();
}

}  // namespace

WindowManagerApplication::WindowManagerApplication()
    : screenlock_state_listener_binding_(this) {}

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
  gpu_service_.reset();
  ShutdownComponents();
}

void WindowManagerApplication::OnAcceleratorRegistrarDestroyed(
    AcceleratorRegistrarImpl* registrar) {
  accelerator_registrars_.erase(registrar);
}

void WindowManagerApplication::InitWindowManager(
    ui::WindowTreeClient* window_tree_client) {
  InitializeComponents();

  window_manager_->Init(window_tree_client);
}

void WindowManagerApplication::OnStart(const shell::Identity& identity) {
  aura_init_.reset(new views::AuraInit(connector(), "ash_mus_resources.pak"));
  gpu_service_ = ui::GpuService::Create(connector());
  compositor_context_factory_.reset(
      new views::SurfaceContextFactory(gpu_service_.get()));
  aura::Env::GetInstance()->set_context_factory(
      compositor_context_factory_.get());
  window_manager_.reset(new WindowManager(connector()));

  MaterialDesignController::Initialize();

  tracing_.Initialize(connector(), identity.name());

  ui::WindowTreeClient* window_tree_client = new ui::WindowTreeClient(
      window_manager_.get(), window_manager_.get(), nullptr);
  window_tree_client->ConnectAsWindowManager(connector());

  native_widget_factory_mus_.reset(
      new NativeWidgetFactoryMus(window_manager_.get()));

  InitWindowManager(window_tree_client);
}

bool WindowManagerApplication::OnConnect(const shell::Identity& remote_identity,
                                         shell::InterfaceRegistry* registry) {
  registry->AddInterface<ui::mojom::AcceleratorRegistrar>(this);
  if (remote_identity.name() == "mojo:mash_session") {
    connector()->ConnectToInterface(remote_identity, &session_);
    session_->AddScreenlockStateListener(
        screenlock_state_listener_binding_.CreateInterfacePtrAndBind());
  }
  return true;
}

void WindowManagerApplication::Create(
    const shell::Identity& remote_identity,
    mojo::InterfaceRequest<ui::mojom::AcceleratorRegistrar> request) {
  if (!window_manager_->window_manager_client())
    return;  // Can happen during shutdown.

  uint16_t accelerator_namespace_id;
  if (!window_manager_->GetNextAcceleratorNamespaceId(
          &accelerator_namespace_id)) {
    DVLOG(1) << "Max number of accelerators registered, ignoring request.";
    // All ids are used. Normally shouldn't happen, so we close the connection.
    return;
  }
  accelerator_registrars_.insert(new AcceleratorRegistrarImpl(
      window_manager_.get(), accelerator_namespace_id, std::move(request),
      base::Bind(&WindowManagerApplication::OnAcceleratorRegistrarDestroyed,
                 base::Unretained(this))));
}

void WindowManagerApplication::ScreenlockStateChanged(bool locked) {
  window_manager_->SetScreenLocked(locked);
}

}  // namespace mus
}  // namespace ash
