// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mojo_interface_factory.h"

#include <utility>

#include "ash/accelerators/accelerator_controller.h"
#include "ash/cast_config_controller.h"
#include "ash/display/ash_display_controller.h"
#include "ash/highlighter/highlighter_controller.h"
#include "ash/ime/ime_controller.h"
#include "ash/login/lock_screen_controller.h"
#include "ash/media_controller.h"
#include "ash/new_window_controller.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/session/session_controller.h"
#include "ash/shelf/shelf_controller.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/shutdown_controller.h"
#include "ash/system/locale/locale_notification_controller.h"
#include "ash/system/network/vpn_list.h"
#include "ash/system/night_light/night_light_controller.h"
#include "ash/system/tray/system_tray_controller.h"
#include "ash/tray_action/tray_action.h"
#include "ash/wallpaper/wallpaper_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/single_thread_task_runner.h"
#include "ui/app_list/presenter/app_list.h"

namespace ash {
namespace mojo_interface_factory {
namespace {

base::LazyInstance<RegisterInterfacesCallback>::Leaky
    g_register_interfaces_callback = LAZY_INSTANCE_INITIALIZER;

void BindAcceleratorControllerRequestOnMainThread(
    mojom::AcceleratorControllerRequest request) {
  Shell::Get()->accelerator_controller()->BindRequest(std::move(request));
}

void BindAppListRequestOnMainThread(
    app_list::mojom::AppListRequest request) {
  Shell::Get()->app_list()->BindRequest(std::move(request));
}

void BindAshDisplayControllerRequestOnMainThread(
    mojom::AshDisplayControllerRequest request) {
  Shell::Get()->ash_display_controller()->BindRequest(std::move(request));
}

void BindCastConfigOnMainThread(mojom::CastConfigRequest request) {
  Shell::Get()->cast_config()->BindRequest(std::move(request));
}

void BindHighlighterControllerRequestOnMainThread(
    mojom::HighlighterControllerRequest request) {
  Shell::Get()->highlighter_controller()->BindRequest(std::move(request));
}

void BindImeControllerRequestOnMainThread(mojom::ImeControllerRequest request) {
  Shell::Get()->ime_controller()->BindRequest(std::move(request));
}

void BindLocaleNotificationControllerOnMainThread(
    mojom::LocaleNotificationControllerRequest request) {
  Shell::Get()->locale_notification_controller()->BindRequest(
      std::move(request));
}

void BindLockScreenRequestOnMainThread(mojom::LockScreenRequest request) {
  Shell::Get()->lock_screen_controller()->BindRequest(std::move(request));
}

void BindMediaControllerRequestOnMainThread(
    mojom::MediaControllerRequest request) {
  Shell::Get()->media_controller()->BindRequest(std::move(request));
}

void BindNewWindowControllerRequestOnMainThread(
    mojom::NewWindowControllerRequest request) {
  Shell::Get()->new_window_controller()->BindRequest(std::move(request));
}

void BindNightLightControllerRequestOnMainThread(
    mojom::NightLightControllerRequest request) {
  Shell::Get()->night_light_controller()->BindRequest(std::move(request));
}

void BindSessionControllerRequestOnMainThread(
    mojom::SessionControllerRequest request) {
  Shell::Get()->session_controller()->BindRequest(std::move(request));
}

void BindShelfRequestOnMainThread(mojom::ShelfControllerRequest request) {
  Shell::Get()->shelf_controller()->BindRequest(std::move(request));
}

void BindShutdownControllerRequestOnMainThread(
    mojom::ShutdownControllerRequest request) {
  Shell::Get()->shutdown_controller()->BindRequest(std::move(request));
}

void BindSystemTrayRequestOnMainThread(mojom::SystemTrayRequest request) {
  Shell::Get()->system_tray_controller()->BindRequest(std::move(request));
}

void BindTabletModeRequestOnMainThread(
    mojom::TabletModeControllerRequest request) {
  Shell::Get()->tablet_mode_controller()->BindRequest(std::move(request));
}

void BindTrayActionRequestOnMainThread(mojom::TrayActionRequest request) {
  Shell::Get()->tray_action()->BindRequest(std::move(request));
}

void BindVpnListRequestOnMainThread(mojom::VpnListRequest request) {
  Shell::Get()->vpn_list()->BindRequest(std::move(request));
}

void BindWallpaperRequestOnMainThread(
    mojom::WallpaperControllerRequest request) {
  Shell::Get()->wallpaper_controller()->BindRequest(std::move(request));
}

}  // namespace

void RegisterInterfaces(
    service_manager::BinderRegistry* registry,
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner) {
  registry->AddInterface(
      base::Bind(&BindAcceleratorControllerRequestOnMainThread),
      main_thread_task_runner);
  registry->AddInterface(base::Bind(&BindAppListRequestOnMainThread),
                         main_thread_task_runner);
  registry->AddInterface(
      base::Bind(&BindAshDisplayControllerRequestOnMainThread),
      main_thread_task_runner);
  registry->AddInterface(base::Bind(&BindCastConfigOnMainThread),
                         main_thread_task_runner);
  registry->AddInterface(
      base::Bind(&BindHighlighterControllerRequestOnMainThread),
      main_thread_task_runner);
  registry->AddInterface(base::Bind(&BindImeControllerRequestOnMainThread),
                         main_thread_task_runner);
  registry->AddInterface(
      base::Bind(&BindLocaleNotificationControllerOnMainThread),
      main_thread_task_runner);
  registry->AddInterface(base::Bind(&BindLockScreenRequestOnMainThread),
                         main_thread_task_runner);
  registry->AddInterface(base::Bind(&BindMediaControllerRequestOnMainThread),
                         main_thread_task_runner);
  registry->AddInterface(
      base::Bind(&BindNewWindowControllerRequestOnMainThread),
      main_thread_task_runner);
  if (switches::IsNightLightEnabled()) {
    registry->AddInterface(
        base::Bind(&BindNightLightControllerRequestOnMainThread),
        main_thread_task_runner);
  }
  registry->AddInterface(base::Bind(&BindSessionControllerRequestOnMainThread),
                         main_thread_task_runner);
  registry->AddInterface(base::Bind(&BindShelfRequestOnMainThread),
                         main_thread_task_runner);
  registry->AddInterface(base::Bind(&BindShutdownControllerRequestOnMainThread),
                         main_thread_task_runner);
  registry->AddInterface(base::Bind(&BindSystemTrayRequestOnMainThread),
                         main_thread_task_runner);
  registry->AddInterface(base::Bind(&BindTabletModeRequestOnMainThread),
                         main_thread_task_runner);
  registry->AddInterface(base::Bind(&BindTrayActionRequestOnMainThread),
                         main_thread_task_runner);
  registry->AddInterface(base::Bind(&BindVpnListRequestOnMainThread),
                         main_thread_task_runner);
  registry->AddInterface(base::Bind(&BindWallpaperRequestOnMainThread),
                         main_thread_task_runner);

  // Inject additional optional interfaces.
  if (g_register_interfaces_callback.Get()) {
    std::move(g_register_interfaces_callback.Get())
        .Run(registry, main_thread_task_runner);
  }
}

void SetRegisterInterfacesCallback(RegisterInterfacesCallback callback) {
  g_register_interfaces_callback.Get() = std::move(callback);
}

}  // namespace mojo_interface_factory

}  // namespace ash
