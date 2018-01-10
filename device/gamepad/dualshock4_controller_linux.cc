// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/dualshock4_controller_linux.h"

namespace device {

Dualshock4ControllerLinux::Dualshock4ControllerLinux(int fd) : fd_(fd) {}

Dualshock4ControllerLinux::~Dualshock4ControllerLinux() = default;

void Dualshock4ControllerLinux::WriteOutputReport(void* report,
                                                  size_t report_length) {
  ssize_t res = write(fd_, report, report_length);
  DCHECK_EQ(res, static_cast<ssize_t>(report_length));
}

}  // namespace device
