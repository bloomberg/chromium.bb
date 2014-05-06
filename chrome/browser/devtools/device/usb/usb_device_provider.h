// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVICE_USB_USB_DEVICE_PROVIDER_H_
#define CHROME_BROWSER_DEVTOOLS_DEVICE_USB_USB_DEVICE_PROVIDER_H_

#include "chrome/browser/devtools/device/android_device_manager.h"

namespace crypto {
class RSAPrivateKey;
}

class UsbDeviceProvider : public AndroidDeviceManager::DeviceProvider {
 public:
  typedef DeviceProvider::QueryDevicesCallback QueryDevicesCallback;

  static void CountDevices(const base::Callback<void(int)>& callback);

  explicit UsbDeviceProvider(Profile* profile);

  virtual void QueryDevices(const QueryDevicesCallback& callback) OVERRIDE;

 private:
  virtual ~UsbDeviceProvider();

  scoped_ptr<crypto::RSAPrivateKey>  rsa_key_;
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVICE_USB_USB_DEVICE_PROVIDER_H_
