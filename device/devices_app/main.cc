// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sequenced_task_runner.h"
#include "device/devices_app/public/cpp/devices_app_factory.h"
#include "mojo/application/public/cpp/application_delegate.h"
#include "mojo/application/public/cpp/application_runner.h"
#include "mojo/public/c/system/main.h"

MojoResult MojoMain(MojoHandle shell_handle) {
  mojo::ApplicationRunner runner(
      device::DevicesAppFactory::CreateApp().release());
  return runner.Run(shell_handle);
}
