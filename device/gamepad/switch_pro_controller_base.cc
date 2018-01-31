// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/switch_pro_controller_base.h"

namespace {
const uint32_t kVendorNintendo = 0x057e;
const uint32_t kProductSwitchProController = 0x2009;
const uint8_t kRumbleMagnitudeMax = 0xff;

enum ControllerType { UNKNOWN_CONTROLLER, SWITCH_PRO_CONTROLLER };

ControllerType ControllerTypeFromDeviceIds(int vendor_id, int product_id) {
  if (vendor_id == kVendorNintendo) {
    switch (product_id) {
      case kProductSwitchProController:
        return SWITCH_PRO_CONTROLLER;
      default:
        break;
    }
  }
  return UNKNOWN_CONTROLLER;
}

double NormalizeAxis(int value, int min, int max) {
  return (2.0 * (value - min) / static_cast<double>(max - min)) - 1.0;
}

}  // namespace

namespace device {

SwitchProControllerBase::~SwitchProControllerBase() = default;

// static
bool SwitchProControllerBase::IsSwitchPro(int vendor_id, int product_id) {
  return ControllerTypeFromDeviceIds(vendor_id, product_id) !=
         UNKNOWN_CONTROLLER;
}

// static
void SwitchProControllerBase::UpdatePadStateFromControllerData(
    const ControllerDataReport& report,
    Gamepad* pad) {
  pad->buttons[BUTTON_INDEX_PRIMARY].pressed = report.button_b;
  pad->buttons[BUTTON_INDEX_PRIMARY].value = report.button_b ? 1.0 : 0.0;

  pad->buttons[BUTTON_INDEX_SECONDARY].pressed = report.button_a;
  pad->buttons[BUTTON_INDEX_SECONDARY].value = report.button_a ? 1.0 : 0.0;

  pad->buttons[BUTTON_INDEX_TERTIARY].pressed = report.button_y;
  pad->buttons[BUTTON_INDEX_TERTIARY].value = report.button_y ? 1.0 : 0.0;

  pad->buttons[BUTTON_INDEX_QUATERNARY].pressed = report.button_x;
  pad->buttons[BUTTON_INDEX_QUATERNARY].value = report.button_x ? 1.0 : 0.0;

  pad->buttons[BUTTON_INDEX_LEFT_SHOULDER].pressed = report.button_l;
  pad->buttons[BUTTON_INDEX_LEFT_SHOULDER].value = report.button_l ? 1.0 : 0.0;

  pad->buttons[BUTTON_INDEX_RIGHT_SHOULDER].pressed = report.button_r;
  pad->buttons[BUTTON_INDEX_RIGHT_SHOULDER].value = report.button_r ? 1.0 : 0.0;

  pad->buttons[BUTTON_INDEX_LEFT_TRIGGER].pressed = report.button_zl;
  pad->buttons[BUTTON_INDEX_LEFT_TRIGGER].value = report.button_zl ? 1.0 : 0.0;

  pad->buttons[BUTTON_INDEX_RIGHT_TRIGGER].pressed = report.button_zr;
  pad->buttons[BUTTON_INDEX_RIGHT_TRIGGER].value = report.button_zr ? 1.0 : 0.0;

  pad->buttons[BUTTON_INDEX_BACK_SELECT].pressed = report.button_minus;
  pad->buttons[BUTTON_INDEX_BACK_SELECT].value =
      report.button_minus ? 1.0 : 0.0;

  pad->buttons[BUTTON_INDEX_START].pressed = report.button_plus;
  pad->buttons[BUTTON_INDEX_START].value = report.button_plus ? 1.0 : 0.0;

  pad->buttons[BUTTON_INDEX_LEFT_THUMBSTICK].pressed = report.button_thumb_l;
  pad->buttons[BUTTON_INDEX_LEFT_THUMBSTICK].value =
      report.button_thumb_l ? 1.0 : 0.0;

  pad->buttons[BUTTON_INDEX_RIGHT_THUMBSTICK].pressed = report.button_thumb_r;
  pad->buttons[BUTTON_INDEX_RIGHT_THUMBSTICK].value =
      report.button_thumb_r ? 1.0 : 0.0;

  pad->buttons[BUTTON_INDEX_DPAD_UP].pressed = report.dpad_up;
  pad->buttons[BUTTON_INDEX_DPAD_UP].value = report.dpad_up ? 1.0 : 0.0;

  pad->buttons[BUTTON_INDEX_DPAD_DOWN].pressed = report.dpad_down;
  pad->buttons[BUTTON_INDEX_DPAD_DOWN].value = report.dpad_down ? 1.0 : 0.0;

  pad->buttons[BUTTON_INDEX_DPAD_LEFT].pressed = report.dpad_left;
  pad->buttons[BUTTON_INDEX_DPAD_LEFT].value = report.dpad_left ? 1.0 : 0.0;

  pad->buttons[BUTTON_INDEX_DPAD_RIGHT].pressed = report.dpad_right;
  pad->buttons[BUTTON_INDEX_DPAD_RIGHT].value = report.dpad_right ? 1.0 : 0.0;

  pad->buttons[BUTTON_INDEX_META].pressed = report.button_home;
  pad->buttons[BUTTON_INDEX_META].value = report.button_home ? 1.0 : 0.0;

  pad->buttons[BUTTON_INDEX_META + 1].pressed = report.button_capture;
  pad->buttons[BUTTON_INDEX_META + 1].value = report.button_capture ? 1.0 : 0.0;

  int8_t axis_lx =
      (((report.analog[1] & 0x0F) << 4) | ((report.analog[0] & 0xF0) >> 4)) +
      127;
  int8_t axis_ly = report.analog[2] + 127;
  int8_t axis_rx =
      (((report.analog[4] & 0x0F) << 4) | ((report.analog[3] & 0xF0) >> 4)) +
      127;
  int8_t axis_ry = report.analog[5] + 127;
  pad->axes[AXIS_INDEX_LEFT_STICK_X] =
      NormalizeAxis(axis_lx, kAxisMin, kAxisMax);
  pad->axes[AXIS_INDEX_LEFT_STICK_Y] =
      NormalizeAxis(-axis_ly, kAxisMin, kAxisMax);
  pad->axes[AXIS_INDEX_RIGHT_STICK_X] =
      NormalizeAxis(axis_rx, kAxisMin, kAxisMax);
  pad->axes[AXIS_INDEX_RIGHT_STICK_Y] =
      NormalizeAxis(-axis_ry, kAxisMin, kAxisMax);

  pad->buttons_length = BUTTON_INDEX_COUNT + 1;
  pad->axes_length = AXIS_INDEX_COUNT;
}

void SwitchProControllerBase::SendConnectionStatusQuery() {
  // Requests the current connection status and info about the connected
  // controller. The controller will respond with a status packet.
  const size_t report_length = 2;
  uint8_t report[report_length];
  memset(report, 0, report_length);
  report[0] = 0x80;
  report[1] = 0x01;

  WriteOutputReport(report, report_length);
}

void SwitchProControllerBase::SendHandshake() {
  // Sends handshaking packets over UART. This command can only be called once
  // per session. The controller will respond with a status packet.
  uint8_t report[kReportSize];
  memset(report, 0, kReportSize);
  report[0] = 0x80;
  report[1] = 0x02;

  WriteOutputReport(report, kReportSize);
}

void SwitchProControllerBase::SendForceUsbHid(bool enable) {
  // By default, the controller will revert to Bluetooth mode if it does not
  // receive any USB HID commands within a timeout window. Enabling the
  // ForceUsbHid mode forces all communication to go through USB HID and
  // disables the timeout.
  uint8_t report[kReportSize];
  memset(report, 0, kReportSize);
  report[0] = 0x80;
  report[1] = (enable ? 0x04 : 0x05);

  WriteOutputReport(report, kReportSize);
}

void SwitchProControllerBase::SetVibration(double strong_magnitude,
                                           double weak_magnitude) {
  uint8_t strong_magnitude_scaled =
      static_cast<uint8_t>(strong_magnitude * kRumbleMagnitudeMax);
  uint8_t weak_magnitude_scaled =
      static_cast<uint8_t>(weak_magnitude * kRumbleMagnitudeMax);

  uint8_t report[kReportSize];
  memset(report, 0, kReportSize);
  report[0] = 0x10;
  report[1] = static_cast<uint8_t>(counter_++ & 0x0F);
  report[2] = 0x80;
  report[6] = 0x80;
  if (strong_magnitude_scaled > 0) {
    report[2] = 0x80;
    report[3] = 0x20;
    report[4] = 0x62;
    report[5] = strong_magnitude_scaled >> 2;
  }
  if (weak_magnitude_scaled > 0) {
    report[6] = 0x98;
    report[7] = 0x20;
    report[8] = 0x62;
    report[9] = weak_magnitude_scaled >> 2;
  }

  WriteOutputReport(report, kReportSize);
}

}  // namespace device
