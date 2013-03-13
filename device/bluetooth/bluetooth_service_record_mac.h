// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_SERVICE_RECORD_MAC_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_SERVICE_RECORD_MAC_H_

#include "device/bluetooth/bluetooth_service_record.h"

#ifdef __OBJC__
@class IOBluetoothDevice;
@class IOBluetoothSDPServiceRecord;
#else
class IOBluetoothDevice;
class IOBluetoothSDPServiceRecord;
#endif

namespace device {

class BluetoothServiceRecordMac : public BluetoothServiceRecord {
 public:
  explicit BluetoothServiceRecordMac(IOBluetoothSDPServiceRecord* record);
  virtual ~BluetoothServiceRecordMac();

  IOBluetoothDevice* GetIOBluetoothDevice() const {
    return device_;
  }

 private:
  IOBluetoothDevice* device_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothServiceRecordMac);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_SERVICE_RECORD_MAC_H_
