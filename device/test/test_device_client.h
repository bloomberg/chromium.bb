// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "build/build_config.h"
#include "device/base/device_client.h"

namespace device {

class HidService;
class UsbService;

class TestDeviceClient : public DeviceClient {
 public:
  TestDeviceClient();

  // Must be destroyed when tasks can still be posted to |task_runner|.
  ~TestDeviceClient() override;

#if !defined(OS_ANDROID)
  HidService* GetHidService() override;
#endif
  UsbService* GetUsbService() override;

 private:
#if !defined(OS_ANDROID)
  std::unique_ptr<HidService> hid_service_;
#endif
  std::unique_ptr<UsbService> usb_service_;
};

}  // namespace device
