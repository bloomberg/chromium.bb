// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/touch_hud/mus/touch_hud_application.h"
#include "services/service_manager/public/c/main.h"
#include "services/service_manager/public/cpp/service_runner.h"

MojoResult ServiceMain(MojoHandle service_request_handle) {
  ash::touch_hud::TouchHudApplication* app =
      new ash::touch_hud::TouchHudApplication;
  app->set_running_standalone(true);
  service_manager::ServiceRunner runner(app);
  return runner.Run(service_request_handle);
}
