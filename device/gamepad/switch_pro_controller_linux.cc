// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/switch_pro_controller_linux.h"

#include "base/posix/eintr_wrapper.h"
#include "device/gamepad/gamepad_standard_mappings.h"

namespace device {

SwitchProControllerLinux::SwitchProControllerLinux(int fd) : fd_(fd) {}

SwitchProControllerLinux::~SwitchProControllerLinux() = default;

void SwitchProControllerLinux::DoShutdown() {
  if (force_usb_hid_)
    SendForceUsbHid(false);
  force_usb_hid_ = false;
}

void SwitchProControllerLinux::ReadUsbPadState(Gamepad* pad) {
  DCHECK_GE(fd_, 0);

  uint8_t report_bytes[kReportSize];
  ssize_t len = 0;
  while ((len = HANDLE_EINTR(read(fd_, report_bytes, kReportSize))) > 0) {
    uint8_t packet_type = report_bytes[0];
    switch (packet_type) {
      case kPacketTypeStatus:
        if (len >= 2) {
          uint8_t status_type = report_bytes[1];
          switch (status_type) {
            case kStatusTypeSerial:
              if (!sent_handshake_) {
                sent_handshake_ = true;
                SendHandshake();
              }
              break;
            case kStatusTypeInit:
              SendForceUsbHid(true);
              force_usb_hid_ = true;
              break;
          }
        }
        break;
      case kPacketTypeControllerData: {
        ControllerDataReport* report =
            reinterpret_cast<ControllerDataReport*>(report_bytes);
        UpdatePadStateFromControllerData(*report, pad);
        pad->timestamp = ++report_id_;
        break;
      }
      default:
        break;
    }
  }
}

void SwitchProControllerLinux::WriteOutputReport(void* report,
                                                 size_t report_length) {
  write(fd_, report, report_length);
}

}  // namespace device
