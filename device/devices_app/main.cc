// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sequenced_task_runner.h"
#include "device/devices_app/devices_app.h"
#include "mojo/application/public/cpp/application_runner.h"
#include "third_party/mojo/src/mojo/public/c/system/main.h"

MojoResult MojoMain(MojoHandle shell_handle) {
  mojo::ApplicationRunner runner(
      device::DevicesApp::CreateDelegate(nullptr).release());
  return runner.Run(shell_handle);
}
