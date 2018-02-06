// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_SWITCH_PRO_CONTROLLER_LINUX_
#define DEVICE_GAMEPAD_SWITCH_PRO_CONTROLLER_LINUX_

#include <memory>

#include "device/gamepad/switch_pro_controller_base.h"

namespace device {

class SwitchProControllerLinux : public SwitchProControllerBase {
 public:
  SwitchProControllerLinux(int fd);
  ~SwitchProControllerLinux() override;

  void ReadUsbPadState(Gamepad* pad);

  void DoShutdown() override;
  void WriteOutputReport(void* report, size_t report_length) override;

 private:
  int fd_ = -1;
  int report_id_ = 0;
  bool force_usb_hid_ = false;
  bool sent_handshake_ = false;
};

}  // namespace device

#endif  // DEVICE_GAMEPAD_SWITCH_PRO_CONTROLLER_LINUX_
