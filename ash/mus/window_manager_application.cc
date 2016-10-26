// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/window_manager_application.h"

#include <utility>

#include "ash/common/material_design/material_design_controller.h"
#include "ash/common/mojo_interface_factory.h"
#include "ash/common/wm_shell.h"
#include "ash/mus/native_widget_factory_mus.h"
#include "ash/mus/window_manager.h"
#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "services/service_manager/public/cpp/connection.h"
#include "services/service_manager/public/cpp/connector.h"
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
#include "ash/mus/network_connect_delegate_mus.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/network/network_connect.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/system/fake_statistics_provider.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"  // nogncheck
#endif

namespace ash {
namespace mus {

WindowManagerApplication::WindowManagerApplication()
    : screenlock_state_listener_binding_(this) {}

WindowManagerApplication::~WindowManagerApplication() {
  // Destroy the WindowManager while still valid. This way we ensure
  // OnWillDestroyRootWindowController() is called (if it hasn't been already).
  window_manager_.reset();

  if (blocking_pool_) {
    // Like BrowserThreadImpl, the goal is to make it impossible for ash to
    // 'infinite loop' during shutdown, but to reasonably expect that all
    // BLOCKING_SHUTDOWN tasks queued during shutdown get run. There's nothing
    // particularly scientific about the number chosen.
    const int kMaxNewShutdownBlockingTasks = 1000;
    blocking_pool_->Shutdown(kMaxNewShutdownBlockingTasks);
  }

  gpu_service_.reset();
#if defined(OS_CHROMEOS)
  statistics_provider_.reset();
#endif
  ShutdownComponents();
}

void WindowManagerApplication::InitWindowManager(
    std::unique_ptr<ui::WindowTreeClient> window_tree_client,
    const scoped_refptr<base::SequencedWorkerPool>& blocking_pool) {
  InitializeComponents();
#if defined(OS_CHROMEOS)
  // TODO(jamescook): Refactor StatisticsProvider so we can get just the data
  // we need in ash. Right now StatisticsProviderImpl launches the crossystem
  // binary to get system data, which we don't want to do twice on startup.
  statistics_provider_.reset(
      new chromeos::system::ScopedFakeStatisticsProvider());
  statistics_provider_->SetMachineStatistic("initial_locale", "en-US");
  statistics_provider_->SetMachineStatistic("keyboard_layout", "");
#endif
  window_manager_->Init(std::move(window_tree_client), blocking_pool);
  native_widget_factory_mus_ =
      base::MakeUnique<NativeWidgetFactoryMus>(window_manager_.get());
}

void WindowManagerApplication::InitializeComponents() {
  message_center::MessageCenter::Initialize();
#if defined(OS_CHROMEOS)
  // Must occur after mojo::ApplicationRunner has initialized AtExitManager, but
  // before WindowManager::Init().
  chromeos::DBusThreadManager::Initialize(
      chromeos::DBusThreadManager::PROCESS_ASH);

  // See ChromeBrowserMainPartsChromeos for ordering details.
  bluez::BluezDBusManager::Initialize(
      chromeos::DBusThreadManager::Get()->GetSystemBus(),
      chromeos::DBusThreadManager::Get()->IsUsingFakes());
  chromeos::NetworkHandler::Initialize();
  network_connect_delegate_.reset(new NetworkConnectDelegateMus());
  chromeos::NetworkConnect::Initialize(network_connect_delegate_.get());
  // TODO(jamescook): Initialize real audio handler.
  chromeos::CrasAudioHandler::InitializeForTesting();
  PowerStatus::Initialize();
#endif
}

void WindowManagerApplication::ShutdownComponents() {
#if defined(OS_CHROMEOS)
  PowerStatus::Shutdown();
  chromeos::CrasAudioHandler::Shutdown();
  chromeos::NetworkConnect::Shutdown();
  network_connect_delegate_.reset();
  chromeos::NetworkHandler::Shutdown();
  bluez::BluezDBusManager::Shutdown();
  chromeos::DBusThreadManager::Shutdown();
#endif
  message_center::MessageCenter::Shutdown();
}

void WindowManagerApplication::OnStart(
    const service_manager::ServiceInfo& info) {
  aura_init_.reset(new views::AuraInit(connector(), "ash_mus_resources.pak",
                                       "ash_mus_resources_200.pak"));
  gpu_service_ = ui::GpuService::Create(connector());
  compositor_context_factory_.reset(
      new views::SurfaceContextFactory(gpu_service_.get()));
  aura::Env::GetInstance()->set_context_factory(
      compositor_context_factory_.get());
  window_manager_.reset(new WindowManager(connector()));

  MaterialDesignController::Initialize();

  tracing_.Initialize(connector(), info.identity.name());

  std::unique_ptr<ui::WindowTreeClient> window_tree_client =
      base::MakeUnique<ui::WindowTreeClient>(window_manager_.get(),
                                             window_manager_.get());
  window_tree_client->ConnectAsWindowManager(connector());

  const size_t kMaxNumberThreads = 3u;  // Matches that of content.
  const char kThreadNamePrefix[] = "MashBlocking";
  blocking_pool_ = new base::SequencedWorkerPool(
      kMaxNumberThreads, kThreadNamePrefix, base::TaskPriority::USER_VISIBLE);
  InitWindowManager(std::move(window_tree_client), blocking_pool_);
}

bool WindowManagerApplication::OnConnect(
    const service_manager::ServiceInfo& remote_info,
    service_manager::InterfaceRegistry* registry) {
  // Register services used in both classic ash and mash.
  mojo_interface_factory::RegisterInterfaces(
      registry, base::ThreadTaskRunnerHandle::Get());

  if (remote_info.identity.name() == "service:mash_session") {
    connector()->ConnectToInterface(remote_info.identity, &session_);
    session_->AddScreenlockStateListener(
        screenlock_state_listener_binding_.CreateInterfacePtrAndBind());
  }
  return true;
}

void WindowManagerApplication::ScreenlockStateChanged(bool locked) {
  window_manager_->SetScreenLocked(locked);
}

}  // namespace mus
}  // namespace ash
