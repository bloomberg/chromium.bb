// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_SWITCH_PRO_CONTROLLER_BASE_
#define DEVICE_GAMEPAD_SWITCH_PRO_CONTROLLER_BASE_

#include "device/gamepad/abstract_haptic_gamepad.h"
#include "device/gamepad/gamepad_standard_mappings.h"

namespace device {

class SwitchProControllerBase : public AbstractHapticGamepad {
 public:
  // Switch Pro Controller USB packet types.
  static const uint8_t kPacketTypeStatus = 0x81;
  static const uint8_t kPacketTypeControllerData = 0x30;

  // Status packet subtypes.
  static const uint8_t kStatusTypeSerial = 0x01;
  static const uint8_t kStatusTypeInit = 0x02;

  // Axis extents, used for normalization.
  static const int kAxisMin = -128;
  static const int kAxisMax = 127;

  // Maximum size of a Switch HID report, in bytes.
  static const int kReportSize = 64;

#pragma pack(push, 1)
  struct ControllerDataReport {
    uint8_t type;  // must be kPacketTypeControllerData
    uint8_t timestamp;
    uint8_t dummy1;
    bool button_y : 1;
    bool button_x : 1;
    bool button_b : 1;
    bool button_a : 1;
    bool dummy2 : 2;
    bool button_r : 1;
    bool button_zr : 1;
    bool button_minus : 1;
    bool button_plus : 1;
    bool button_thumb_r : 1;
    bool button_thumb_l : 1;
    bool button_home : 1;
    bool button_capture : 1;
    bool dummy3 : 2;
    bool dpad_down : 1;
    bool dpad_up : 1;
    bool dpad_right : 1;
    bool dpad_left : 1;
    bool dummy4 : 2;
    bool button_l : 1;
    bool button_zl : 1;
    uint8_t analog[6];
  };
#pragma pack(pop)

  SwitchProControllerBase() = default;
  ~SwitchProControllerBase() override;

  static bool IsSwitchPro(int vendor_id, int product_id);

  static void UpdatePadStateFromControllerData(
      const ControllerDataReport& report,
      Gamepad* pad);

  void SendConnectionStatusQuery();
  void SendHandshake();
  void SendForceUsbHid(bool enable);
  void SetVibration(double strong_magnitude, double weak_magnitude) override;

  virtual void WriteOutputReport(void* report, size_t report_length) {}

 private:
  uint32_t counter_ = 0;
};

}  // namespace device

#endif  // DEVICE_GAMEPAD_SWITCH_PRO_CONTROLLER_BASE_
