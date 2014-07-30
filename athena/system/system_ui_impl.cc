// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/system/public/system_ui.h"

#include "athena/system/power_button_controller.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"

namespace athena {
namespace {

SystemUI* instance = NULL;

class SystemUIImpl : public SystemUI {
 public:
  SystemUIImpl() : power_button_controller_(new PowerButtonController) {
  }

  virtual ~SystemUIImpl() {
  }

 private:
  scoped_ptr<PowerButtonController> power_button_controller_;

  DISALLOW_COPY_AND_ASSIGN(SystemUIImpl);
};

}  // namespace

// static
SystemUI* SystemUI::Create() {
  instance = new SystemUIImpl;
  return instance;
}

// static
void SystemUI::Shutdown() {
  CHECK(instance);
  delete instance;
  instance = NULL;
}

}  // namespace athena
