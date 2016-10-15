// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/mojo_interface_factory.h"

#include "ash/common/shelf/shelf_controller.h"
#include "ash/common/system/tray/system_tray_controller.h"
#include "ash/common/wm_shell.h"
#include "base/bind.h"
#include "services/service_manager/public/cpp/interface_registry.h"

namespace ash {

namespace {

void BindShelfControllerRequestOnMainThread(
    mojom::ShelfControllerRequest request) {
  WmShell::Get()->shelf_controller()->BindRequest(std::move(request));
}

void BindSystemTrayRequestOnMainThread(mojom::SystemTrayRequest request) {
  WmShell::Get()->system_tray_controller()->BindRequest(std::move(request));
}

}  // namespace

namespace mojo_interface_factory {

void RegisterInterfaces(
    service_manager::InterfaceRegistry* registry,
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner) {
  registry->AddInterface(base::Bind(&BindShelfControllerRequestOnMainThread),
                         main_thread_task_runner);
  registry->AddInterface(base::Bind(&BindSystemTrayRequestOnMainThread),
                         main_thread_task_runner);
}

}  // namespace mojo_interface_factory

}  // namespace ash
