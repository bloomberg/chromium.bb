// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/usb/usb_service_win.h"

namespace device {

UsbServiceWin::UsbServiceWin(
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner)
    : UsbService(blocking_task_runner) {}

UsbServiceWin::~UsbServiceWin() {}

}  // namespace device
