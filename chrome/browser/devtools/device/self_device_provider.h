// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVICE_SELF_DEVICE_PROVIDER_H_
#define CHROME_BROWSER_DEVTOOLS_DEVICE_SELF_DEVICE_PROVIDER_H_

#include "chrome/browser/devtools/device/android_device_manager.h"

// Instantiate this class only in a test and/or when DEBUG_DEVTOOLS is defined.
class SelfAsDeviceProvider : public AndroidDeviceManager::DeviceProvider {
 public:
  explicit SelfAsDeviceProvider(int port);

  virtual void QueryDevices(const SerialsCallback& callback) OVERRIDE;

  virtual void QueryDeviceInfo(const std::string& serial,
                               const DeviceInfoCallback& callback) OVERRIDE;

  virtual void OpenSocket(const std::string& serial,
                          const std::string& socket_name,
                          const SocketCallback& callback) OVERRIDE;

 private:
  virtual ~SelfAsDeviceProvider(){}

  int port_;
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVICE_SELF_DEVICE_PROVIDER_H_
