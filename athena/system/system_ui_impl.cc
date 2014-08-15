// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/system/public/system_ui.h"

#include "athena/system/device_socket_listener.h"
#include "athena/system/orientation_controller.h"
#include "athena/system/power_button_controller.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"

namespace athena {
namespace {

SystemUI* instance = NULL;

class SystemUIImpl : public SystemUI {
 public:
  SystemUIImpl(scoped_refptr<base::TaskRunner> io_task_runner)
      : orientation_controller_(new OrientationController(io_task_runner)),
        power_button_controller_(new PowerButtonController) {
  }

  virtual ~SystemUIImpl() {
  }

 private:
  scoped_refptr<OrientationController> orientation_controller_;
  scoped_ptr<PowerButtonController> power_button_controller_;

  DISALLOW_COPY_AND_ASSIGN(SystemUIImpl);
};

}  // namespace

// static
SystemUI* SystemUI::Create(
    scoped_refptr<base::TaskRunner> io_task_runner) {
  DeviceSocketListener::CreateSocketManager(io_task_runner);
  instance = new SystemUIImpl(io_task_runner);
  return instance;
}

// static
void SystemUI::Shutdown() {
  CHECK(instance);
  delete instance;
  instance = NULL;
  DeviceSocketListener::ShutdownSocketManager();
}

}  // namespace athena
