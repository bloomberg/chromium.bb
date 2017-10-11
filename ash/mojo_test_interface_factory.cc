// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mojo_test_interface_factory.h"

#include <utility>

#include "ash/public/interfaces/system_tray_test_api.mojom.h"
#include "ash/system/tray/system_tray_test_api.h"
#include "base/bind.h"
#include "base/single_thread_task_runner.h"

namespace ash {
namespace mojo_test_interface_factory {
namespace {

// This isn't strictly necessary, but exists to make threading and arguments
// clearer.
void BindSystemTrayTestApiOnMainThread(
    mojom::SystemTrayTestApiRequest request) {
  SystemTrayTestApi::BindRequest(std::move(request));
}

}  // namespace

void RegisterInterfaces(
    service_manager::BinderRegistry* registry,
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner) {
  registry->AddInterface(base::Bind(&BindSystemTrayTestApiOnMainThread),
                         main_thread_task_runner);
}

}  // namespace mojo_test_interface_factory
}  // namespace ash
