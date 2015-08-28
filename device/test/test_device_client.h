// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "device/core/device_client.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace device {

class HidService;
class UsbService;

class TestDeviceClient : public DeviceClient {
 public:
  TestDeviceClient(scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  ~TestDeviceClient() override;

 private:
  HidService* GetHidService() override;
  UsbService* GetUsbService() override;

  scoped_ptr<HidService> hid_service_;
  scoped_ptr<UsbService> usb_service_;
  scoped_refptr<base::SingleThreadTaskRunner> blocking_task_runner_;
};

}  // namespace device
