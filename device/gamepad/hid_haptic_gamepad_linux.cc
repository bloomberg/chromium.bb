// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/hid_haptic_gamepad_linux.h"

#include "base/posix/eintr_wrapper.h"

namespace device {

HidHapticGamepadLinux::HidHapticGamepadLinux(int fd,
                                             const HapticReportData& data)
    : HidHapticGamepadBase(data), fd_(fd) {}

HidHapticGamepadLinux::~HidHapticGamepadLinux() = default;

// static
std::unique_ptr<HidHapticGamepadLinux>
HidHapticGamepadLinux::Create(uint16_t vendor_id, uint16_t product_id, int fd) {
  const auto* haptic_data = GetHapticReportData(vendor_id, product_id);
  if (!haptic_data)
    return nullptr;
  return std::make_unique<HidHapticGamepadLinux>(fd, *haptic_data);
}

size_t HidHapticGamepadLinux::WriteOutputReport(void* report,
                                                size_t report_length) {
  ssize_t bytes_written = HANDLE_EINTR(write(fd_, report, report_length));
  return bytes_written < 0 ? 0 : static_cast<size_t>(bytes_written);
}

}  // namespace device
