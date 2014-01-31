// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_HID_HID_SERVICE_H_
#define DEVICE_HID_HID_SERVICE_H_

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string16.h"
#include "base/threading/thread_checker.h"
#include "build/build_config.h"
#include "device/hid/hid_device_info.h"

namespace device {

namespace {

class HidServiceContainer;

}  // namespace

class HidConnection;
class HidService;

class HidService : public base::MessageLoop::DestructionObserver {
 public:
  // Must be called on FILE thread.
  static HidService* GetInstance();

  // Enumerates and returns a list of device identifiers.
  virtual void GetDevices(std::vector<HidDeviceInfo>* devices);

  // Fills in the device info struct of the given device_id.
  // Returns true if succeed.
  // Returns false if the device_id is invalid, with info untouched.
  bool GetInfo(std::string device_id, HidDeviceInfo* info) const;

  virtual scoped_refptr<HidConnection> Connect(
      std::string platform_device_id) = 0;

  // Implements base::MessageLoop::DestructionObserver
  virtual void WillDestroyCurrentMessageLoop() OVERRIDE;

  // Gets whether the HidService have been successfully initialized.
  bool initialized() const { return initialized_; }

 protected:
  friend class HidServiceContainer;
  friend struct base::DefaultDeleter<HidService>;
  friend class HidConnectionTest;

  HidService();
  virtual ~HidService();

  static HidService* CreateInstance();

  virtual void AddDevice(HidDeviceInfo info);
  virtual void RemoveDevice(std::string platform_device_id);

  typedef std::map<std::string, HidDeviceInfo> DeviceMap;
  DeviceMap devices_;

  bool initialized_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(HidService);
};

}  // namespace device

#endif  // DEVICE_HID_HID_SERVICE_H_
