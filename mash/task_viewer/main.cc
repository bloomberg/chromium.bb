// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "mash/task_viewer/task_viewer.h"
#include "services/service_manager/public/cpp/standalone_service/service_main.h"

void ServiceMain(service_manager::mojom::ServiceRequest request) {
  base::MessageLoop message_loop;
  mash::task_viewer::TaskViewer(std::move(request)).RunUntilTermination();
}
