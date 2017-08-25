// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_HID_MOCK_HID_SERVICE_H_
#define DEVICE_HID_MOCK_HID_SERVICE_H_

#include <map>

#include "device/hid/hid_service.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace device {

class MockHidService : public HidService {
 public:
  MockHidService();
  ~MockHidService() override;

  // Public wrappers around protected functions needed for tests.
  void AddDevice(scoped_refptr<HidDeviceInfo> info);
  void RemoveDevice(const HidPlatformDeviceId& platform_device_id);
  void FirstEnumerationComplete();
  const std::map<std::string, scoped_refptr<HidDeviceInfo>>& devices() const;

  MOCK_METHOD2(Connect,
               void(const std::string& device_guid,
                    const ConnectCallback& callback));

 private:
  base::WeakPtr<HidService> GetWeakPtr() override;

  base::WeakPtrFactory<MockHidService> weak_factory_;
};

}  // namespace device

#endif  // DEVICE_HID_MOCK_HID_SERVICE_H_
