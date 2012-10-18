// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_MOCK_BLUETOOTH_OUT_OF_BAND_CLIENT_H_
#define CHROMEOS_DBUS_MOCK_BLUETOOTH_OUT_OF_BAND_CLIENT_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "chromeos/dbus/bluetooth_out_of_band_client.h"
#include "device/bluetooth/bluetooth_out_of_band_pairing_data.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockBluetoothOutOfBandClient : public BluetoothOutOfBandClient {
 public:
  MockBluetoothOutOfBandClient();
  virtual ~MockBluetoothOutOfBandClient();

  MOCK_METHOD2(ReadLocalData,
      void(const dbus::ObjectPath&,
           const DataCallback&));
  MOCK_METHOD4(AddRemoteData,
      void(const dbus::ObjectPath&,
           const std::string&,
           const device::BluetoothOutOfBandPairingData&,
           const SuccessCallback&));
  MOCK_METHOD3(RemoveRemoteData,
      void(const dbus::ObjectPath&,
           const std::string&,
           const SuccessCallback&));
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_MOCK_BLUETOOTH_OUT_OF_BAND_CLIENT_H_
