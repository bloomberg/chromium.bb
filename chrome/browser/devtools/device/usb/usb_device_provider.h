// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVICE_USB_USB_DEVICE_PROVIDER_H_
#define CHROME_BROWSER_DEVTOOLS_DEVICE_USB_USB_DEVICE_PROVIDER_H_

#include "chrome/browser/devtools/device/android_device_manager.h"

namespace crypto {
class RSAPrivateKey;
}

class AndroidUsbDevice;

class UsbDeviceProvider : public AndroidDeviceManager::DeviceProvider {
 public:
  static void CountDevices(const base::Callback<void(int)>& callback);

  explicit UsbDeviceProvider(Profile* profile);

  virtual void QueryDevices(const SerialsCallback& callback) OVERRIDE;

  virtual void QueryDeviceInfo(const std::string& serial,
                               const DeviceInfoCallback& callback) OVERRIDE;

  virtual void OpenSocket(const std::string& serial,
                          const std::string& socket_name,
                          const SocketCallback& callback) OVERRIDE;

  virtual void ReleaseDevice(const std::string& serial) OVERRIDE;

 private:
  virtual ~UsbDeviceProvider();

  void EnumeratedDevices(
      const SerialsCallback& callback,
      const std::vector<scoped_refptr<AndroidUsbDevice> >& devices);

  typedef std::map<std::string, scoped_refptr<AndroidUsbDevice> > UsbDeviceMap;

  scoped_ptr<crypto::RSAPrivateKey>  rsa_key_;
  UsbDeviceMap device_map_;
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVICE_USB_USB_DEVICE_PROVIDER_H_
