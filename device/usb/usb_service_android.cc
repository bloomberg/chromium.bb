// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/usb/usb_service_android.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/thread_task_runner_handle.h"
#include "device/usb/usb_device.h"

namespace device {

UsbServiceAndroid::UsbServiceAndroid() {}

UsbServiceAndroid::~UsbServiceAndroid() {}

scoped_refptr<UsbDevice> UsbServiceAndroid::GetDevice(const std::string& guid) {
  return nullptr;
}

void UsbServiceAndroid::GetDevices(const GetDevicesCallback& callback) {
  std::vector<scoped_refptr<UsbDevice>> empty;
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                base::Bind(callback, empty));
}

}  // namespace device
