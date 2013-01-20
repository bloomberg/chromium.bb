// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_SERVICE_RECORD_WIN_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_SERVICE_RECORD_WIN_H_

#include <string>

#include "base/basictypes.h"
#include "device/bluetooth/bluetooth_service_record.h"

namespace device {

class BluetoothServiceRecordWin : public BluetoothServiceRecord {
 public:
  BluetoothServiceRecordWin(const std::string& name,
                            const std::string& address,
                            uint64 blob_size,
                            uint8* blob_data);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_SERVICE_RECORD_WIN_H_
