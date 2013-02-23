// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_SERVICE_RECORD_CHROMEOS_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_SERVICE_RECORD_CHROMEOS_H_

#include <bluetooth/bluetooth.h>

#include <string>

#include "base/basictypes.h"
#include "device/bluetooth/bluetooth_service_record.h"

class XmlReader;

namespace chromeos {

// BluetoothServiceRecordChromeOS is an implementation of BluetoothServiceRecord
// for the ChromeOS platform.
class BluetoothServiceRecordChromeOS : public device::BluetoothServiceRecord {
 public:
  BluetoothServiceRecordChromeOS(const std::string& address,
                                 const std::string& xml_data);

  void GetBluetoothAddress(bdaddr_t* out_address) const;

 private:
  void ExtractProtocolDescriptors(XmlReader* reader);
  void ExtractServiceClassUuid(XmlReader* reader);

  DISALLOW_COPY_AND_ASSIGN(BluetoothServiceRecordChromeOS);
};

}  // namespace chromeos

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_SERVICE_RECORD_CHROMEOS_H_
