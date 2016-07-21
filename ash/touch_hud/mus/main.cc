// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/touch_hud/mus/touch_hud_application.h"
#include "mojo/public/c/system/main.h"
#include "services/shell/public/cpp/service_runner.h"

MojoResult MojoMain(MojoHandle shell_handle) {
  shell::ServiceRunner runner(new ash::touch_hud::TouchHudApplication);
  runner.set_message_loop_type(base::MessageLoop::TYPE_UI);
  return runner.Run(shell_handle);
}
