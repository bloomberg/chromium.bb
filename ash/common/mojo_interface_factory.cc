// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/mojo_interface_factory.h"

#include <utility>

#include "ash/common/shelf/shelf_controller.h"
#include "ash/common/shutdown_controller.h"
#include "ash/common/system/locale/locale_notification_controller.h"
#include "ash/common/system/tray/system_tray_controller.h"
#include "ash/common/wallpaper/wallpaper_controller.h"
#include "ash/common/wm_shell.h"
#include "base/bind.h"
#include "services/service_manager/public/cpp/interface_registry.h"

namespace ash {

namespace {

void BindLocaleNotificationControllerOnMainThread(
    mojom::LocaleNotificationControllerRequest request) {
  WmShell::Get()->locale_notification_controller()->BindRequest(
      std::move(request));
}

void BindShelfRequestOnMainThread(mojom::ShelfControllerRequest request) {
  WmShell::Get()->shelf_controller()->BindRequest(std::move(request));
}

void BindShutdownControllerRequestOnMainThread(
    mojom::ShutdownControllerRequest request) {
  WmShell::Get()->shutdown_controller()->BindRequest(std::move(request));
}

void BindSystemTrayRequestOnMainThread(mojom::SystemTrayRequest request) {
  WmShell::Get()->system_tray_controller()->BindRequest(std::move(request));
}

void BindWallpaperRequestOnMainThread(
    mojom::WallpaperControllerRequest request) {
  WmShell::Get()->wallpaper_controller()->BindRequest(std::move(request));
}

}  // namespace

namespace mojo_interface_factory {

void RegisterInterfaces(
    service_manager::InterfaceRegistry* registry,
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner) {
  registry->AddInterface(
      base::Bind(&BindLocaleNotificationControllerOnMainThread),
      main_thread_task_runner);
  registry->AddInterface(base::Bind(&BindShelfRequestOnMainThread),
                         main_thread_task_runner);
  registry->AddInterface(base::Bind(&BindShutdownControllerRequestOnMainThread),
                         main_thread_task_runner);
  registry->AddInterface(base::Bind(&BindSystemTrayRequestOnMainThread),
                         main_thread_task_runner);
  registry->AddInterface(base::Bind(&BindWallpaperRequestOnMainThread),
                         main_thread_task_runner);
}

}  // namespace mojo_interface_factory

}  // namespace ash
