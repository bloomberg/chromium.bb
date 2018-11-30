// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_HID_HAPTIC_GAMEPAD_WIN_H_
#define DEVICE_GAMEPAD_HID_HAPTIC_GAMEPAD_WIN_H_

#include "base/win/scoped_handle.h"
#include "device/gamepad/hid_haptic_gamepad_base.h"

#include <memory>

namespace device {

class HidHapticGamepadWin : public HidHapticGamepadBase {
 public:
  HidHapticGamepadWin(HANDLE device_handle, const HapticReportData& data);
  ~HidHapticGamepadWin() override;

  static std::unique_ptr<HidHapticGamepadWin> Create(uint16_t vendor_id,
                                                     uint16_t product_id,
                                                     HANDLE device_handle);

  void DoShutdown() override;

  size_t WriteOutputReport(void* report, size_t report_length) override;

 private:
  base::win::ScopedHandle hid_handle_;
};

}  // namespace device

#endif  // DEVICE_GAMEPAD_HID_HAPTIC_GAMEPAD_WIN_H_
