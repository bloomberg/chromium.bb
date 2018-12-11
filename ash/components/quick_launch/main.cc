// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/quick_launch/quick_launch_application.h"
#include "base/message_loop/message_loop.h"
#include "services/service_manager/public/cpp/service_executable/service_main.h"

void ServiceMain(service_manager::mojom::ServiceRequest request) {
  base::MessageLoop message_loop;
  quick_launch::QuickLaunchApplication service(std::move(request));
  service.set_running_standalone(true);
  service.RunUntilTermination();
}
