// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/test/test_device_client.h"

#include "build/build_config.h"

// This file unconditionally includes these headers despite conditionally
// depending on the corresponding targets. The code below needs the destructors
// of the classes defined even when the classes are never instantiated.
// TODO: This should probably be done more explicitly to avoid ambiguity.
#include "device/hid/hid_service.h"  // nogncheck
#include "device/usb/usb_service.h"  // nogncheck

namespace device {

TestDeviceClient::TestDeviceClient(
    scoped_refptr<base::SingleThreadTaskRunner> blocking_task_runner)
    : blocking_task_runner_(blocking_task_runner) {}

TestDeviceClient::~TestDeviceClient() {
  if (hid_service_)
    hid_service_->Shutdown();
  if (usb_service_)
    usb_service_->Shutdown();
}

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
