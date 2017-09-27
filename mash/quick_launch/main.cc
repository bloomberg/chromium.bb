// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/quick_launch/quick_launch.h"
#include "services/service_manager/public/c/main.h"
#include "services/service_manager/public/cpp/service_runner.h"

MojoResult ServiceMain(MojoHandle service_request_handle) {
  mash::quick_launch::QuickLaunch* app = new mash::quick_launch::QuickLaunch;
  app->set_running_standalone(true);
  service_manager::ServiceRunner runner(app);
  return runner.Run(service_request_handle);
}
