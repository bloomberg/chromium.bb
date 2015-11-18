// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_USB_USB_SERVICE_ANDROID_H_
#define DEVICE_USB_USB_SERVICE_ANDROID_H_

#include "device/usb/usb_service.h"

namespace device {

// USB service implementation for Android. This is a stub implementation that
// does not return any devices.
class UsbServiceAndroid : public UsbService {
 public:
  UsbServiceAndroid();
  ~UsbServiceAndroid() override;

  scoped_refptr<UsbDevice> GetDevice(const std::string& guid) override;
  void GetDevices(const GetDevicesCallback& callback) override;
};

}  // namespace device

#endif  // DEVICE_USB_USB_SERVICE_ANDROID_H_
