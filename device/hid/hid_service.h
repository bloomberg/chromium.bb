// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_HID_HID_SERVICE_H_
#define DEVICE_HID_HID_SERVICE_H_

#include <map>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "device/hid/hid_device_info.h"

namespace device {

class HidConnection;

class HidService {
 public:
  typedef base::Callback<void(scoped_refptr<HidConnection> connection)>
      ConnectCallback;

  static HidService* GetInstance(
      scoped_refptr<base::SingleThreadTaskRunner> file_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner);

  static void SetInstanceForTest(HidService* instance);

  // Enumerates and returns a list of device identifiers.
  virtual void GetDevices(std::vector<HidDeviceInfo>* devices);

  // Fills in a DeviceInfo struct with info for the given device_id.
  // Returns |true| if successful or |false| if |device_id| is invalid.
  bool GetDeviceInfo(const HidDeviceId& device_id, HidDeviceInfo* info) const;

  // Opens a connection to a device. The callback will be run with null on
  // failure.
  virtual void Connect(const HidDeviceId& device_id,
                       const ConnectCallback& callback) = 0;

 protected:
  friend class HidConnectionTest;

  typedef std::map<HidDeviceId, HidDeviceInfo> DeviceMap;

  HidService();
  virtual ~HidService();

  void AddDevice(const HidDeviceInfo& info);
  void RemoveDevice(const HidDeviceId& device_id);

  const DeviceMap& devices() const { return devices_; }

  base::ThreadChecker thread_checker_;

 private:
  class Destroyer;

  DeviceMap devices_;

  DISALLOW_COPY_AND_ASSIGN(HidService);
};

}  // namespace device

#endif  // DEVICE_HID_HID_SERVICE_H_
