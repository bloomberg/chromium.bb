// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/mojo_interface_factory.h"

#include <utility>

#include "ash/common/accelerators/accelerator_controller.h"
#include "ash/common/cast_config_controller.h"
#include "ash/common/media_controller.h"
#include "ash/common/new_window_controller.h"
#include "ash/common/session/session_controller.h"
#include "ash/common/shelf/shelf_controller.h"
#include "ash/common/shell_delegate.h"
#include "ash/common/shutdown_controller.h"
#include "ash/common/system/chromeos/network/vpn_list.h"
#include "ash/common/system/locale/locale_notification_controller.h"
#include "ash/common/system/tray/system_tray_controller.h"
#include "ash/common/wallpaper/wallpaper_controller.h"
#include "ash/common/wm/maximize_mode/maximize_mode_controller.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "services/service_manager/public/cpp/interface_registry.h"
#include "ui/app_list/presenter/app_list.h"

namespace ash {

namespace {

void BindAcceleratorControllerRequestOnMainThread(
    mojom::AcceleratorControllerRequest request) {
  Shell::Get()->accelerator_controller()->BindRequest(std::move(request));
}

void BindAppListRequestOnMainThread(app_list::mojom::AppListRequest request) {
  Shell::Get()->app_list()->BindRequest(std::move(request));
}

void BindCastConfigOnMainThread(mojom::CastConfigRequest request) {
  Shell::Get()->cast_config()->BindRequest(std::move(request));
}

void BindLocaleNotificationControllerOnMainThread(
    mojom::LocaleNotificationControllerRequest request) {
  Shell::Get()->locale_notification_controller()->BindRequest(
      std::move(request));
}

void BindMediaControllerRequestOnMainThread(
    mojom::MediaControllerRequest request) {
  Shell::Get()->media_controller()->BindRequest(std::move(request));
}

void BindNewWindowControllerRequestOnMainThread(
    mojom::NewWindowControllerRequest request) {
  Shell::Get()->new_window_controller()->BindRequest(std::move(request));
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

void BindTouchViewRequestOnMainThread(mojom::TouchViewManagerRequest request) {
  Shell::Get()->maximize_mode_controller()->BindRequest(std::move(request));
}

void BindVpnListRequestOnMainThread(mojom::VpnListRequest request) {
  Shell::Get()->vpn_list()->BindRequest(std::move(request));
}

void BindWallpaperRequestOnMainThread(
    mojom::WallpaperControllerRequest request) {
  Shell::GetInstance()->wallpaper_controller()->BindRequest(std::move(request));
}

}  // namespace

namespace mojo_interface_factory {

void RegisterInterfaces(
    service_manager::InterfaceRegistry* registry,
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner) {
  registry->AddInterface(
      base::Bind(&BindAcceleratorControllerRequestOnMainThread),
      main_thread_task_runner);
  registry->AddInterface(base::Bind(&BindAppListRequestOnMainThread),
                         main_thread_task_runner);
  registry->AddInterface(base::Bind(&BindCastConfigOnMainThread),
                         main_thread_task_runner);
  registry->AddInterface(
      base::Bind(&BindLocaleNotificationControllerOnMainThread),
      main_thread_task_runner);
  registry->AddInterface(base::Bind(&BindMediaControllerRequestOnMainThread),
                         main_thread_task_runner);
  registry->AddInterface(
      base::Bind(&BindNewWindowControllerRequestOnMainThread),
      main_thread_task_runner);
  registry->AddInterface(base::Bind(&BindSessionControllerRequestOnMainThread),
                         main_thread_task_runner);
  registry->AddInterface(base::Bind(&BindShelfRequestOnMainThread),
                         main_thread_task_runner);
  registry->AddInterface(base::Bind(&BindShutdownControllerRequestOnMainThread),
                         main_thread_task_runner);
  registry->AddInterface(base::Bind(&BindSystemTrayRequestOnMainThread),
                         main_thread_task_runner);
  registry->AddInterface(base::Bind(&BindTouchViewRequestOnMainThread),
                         main_thread_task_runner);
  registry->AddInterface(base::Bind(&BindVpnListRequestOnMainThread),
                         main_thread_task_runner);
  registry->AddInterface(base::Bind(&BindWallpaperRequestOnMainThread),
                         main_thread_task_runner);
}

}  // namespace mojo_interface_factory

}  // namespace ash
