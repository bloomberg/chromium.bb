// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/test/test_device_client.h"

#include "device/hid/hid_service.h"
#include "device/usb/usb_service.h"

namespace device {

TestDeviceClient::TestDeviceClient(
    scoped_refptr<base::SingleThreadTaskRunner> blocking_task_runner)
    : blocking_task_runner_(blocking_task_runner) {}

TestDeviceClient::~TestDeviceClient() {}

HidService* TestDeviceClient::GetHidService() {
#if !defined(OS_ANDROID) && !defined(OS_IOS) && \
    !(defined(OS_LINUX) && !defined(USE_UDEV))
  if (!hid_service_) {
    hid_service_ = HidService::Create(blocking_task_runner_);
  }
#endif
  return hid_service_.get();
}

UsbService* TestDeviceClient::GetUsbService() {
#if !defined(OS_ANDROID) && !defined(OS_IOS)
  if (!usb_service_) {
    usb_service_ = UsbService::Create(blocking_task_runner_);
  }
#endif
  return usb_service_.get();
}

}  // namespace device
