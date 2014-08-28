// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_HID_HID_SERVICE_H_
#define DEVICE_HID_HID_SERVICE_H_

#include <map>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/threading/thread_checker.h"
#include "device/hid/hid_device_info.h"

namespace device {

class HidConnection;

class HidService {
 public:
  static HidService* Create(
      scoped_refptr<base::MessageLoopProxy> ui_message_loop);

  virtual ~HidService();

  // Enumerates and returns a list of device identifiers.
  virtual void GetDevices(std::vector<HidDeviceInfo>* devices);

  // Fills in a DeviceInfo struct with info for the given device_id.
  // Returns |true| if successful or |false| if |device_id| is invalid.
  bool GetDeviceInfo(const HidDeviceId& device_id, HidDeviceInfo* info) const;

  virtual scoped_refptr<HidConnection> Connect(
      const HidDeviceId& device_id) = 0;

 protected:
  friend class HidConnectionTest;

  typedef std::map<HidDeviceId, HidDeviceInfo> DeviceMap;

  HidService();

  void AddDevice(const HidDeviceInfo& info);
  void RemoveDevice(const HidDeviceId& device_id);
  const DeviceMap& GetDevicesNoEnumerate() const;

  base::ThreadChecker thread_checker_;

 private:
  DeviceMap devices_;

  DISALLOW_COPY_AND_ASSIGN(HidService);
};

}  // namespace device

#endif  // DEVICE_HID_HID_SERVICE_H_
