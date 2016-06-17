// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUEZ_BLUETOOTH_SERVICE_RECORD_BLUEZ_H_
#define DEVICE_BLUETOOTH_BLUEZ_BLUETOOTH_SERVICE_RECORD_BLUEZ_H_

#include <stdint.h>
#include <vector>

#include "base/macros.h"
#include "device/bluetooth/bluetooth_export.h"

namespace base {
class Value;
}

namespace bluez {

class DEVICE_BLUETOOTH_EXPORT BluetoothServiceRecordBlueZ {
 public:
  // Possible types of errors raised when creating, removing or getting service
  // records.
  enum ErrorCode {
    ERROR_ADAPTER_NOT_READY,
    ERROR_RECORD_EXISTS,
    ERROR_RECORD_DOES_NOT_EXIST,
    ERROR_DEVICE_DISCONNECTED,
    UNKNOWN
  };

  BluetoothServiceRecordBlueZ();
  ~BluetoothServiceRecordBlueZ();

  // Returns a list of Attribute IDs that exist within this service record.
  std::vector<uint16_t> GetAttributeIds();
  // Returns the value associated with a given attribute ID.
  base::Value* GetAttributeValue(uint16_t attribute_id);

 private:
  DISALLOW_COPY_AND_ASSIGN(BluetoothServiceRecordBlueZ);
};

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_BLUEZ_BLUETOOTH_SERVICE_RECORD_BLUEZ_H_
