// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_HID_HAPTIC_GAMEPAD_LINUX_H_
#define DEVICE_GAMEPAD_HID_HAPTIC_GAMEPAD_LINUX_H_

#include "device/gamepad/hid_haptic_gamepad_base.h"

#include <memory>

namespace device {

class HidHapticGamepadLinux : public HidHapticGamepadBase {
 public:
  HidHapticGamepadLinux(int fd, const HapticReportData& data);
  ~HidHapticGamepadLinux() override;

  static std::unique_ptr<HidHapticGamepadLinux> Create(uint16_t vendor_id,
                                                       uint16_t product_id,
                                                       int fd);

  size_t WriteOutputReport(void* report, size_t report_length) override;

 private:
  int fd_;
};

}  // namespace device

#endif  // DEVICE_GAMEPAD_HID_HAPTIC_GAMEPAD_LINUX_H_
