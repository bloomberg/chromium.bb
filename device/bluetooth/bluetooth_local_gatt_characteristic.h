// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_LOCAL_GATT_CHARACTERISTIC_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_LOCAL_GATT_CHARACTERISTIC_H_

#include <stdint.h>
#include <vector>

#include "base/macros.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/bluetooth_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_local_gatt_service.h"
#include "device/bluetooth/bluetooth_uuid.h"

namespace device {

// BluetoothLocalGattCharacteristic represents a local GATT characteristic. This
// class is used to represent GATT characteristics that belong to a locally
// hosted service. To achieve this, users need to specify the instance of the
// GATT service that contains this characteristic during construction.
//
// Note: We use virtual inheritance on the GATT characteristic since it will be
// inherited by platform specific versions of the GATT characteristic classes
// also. The platform specific remote GATT characteristic classes will inherit
// both this class and their GATT characteristic class, hence causing an
// inheritance diamond.
class DEVICE_BLUETOOTH_EXPORT BluetoothLocalGattCharacteristic
    : public virtual BluetoothGattCharacteristic {
 public:
  // Constructs a BluetoothLocalGattCharacteristic associated with a local GATT
  // service when the adapter is in the peripheral role.
  //
  // This method constructs a characteristic with UUID |uuid|, initial cached
  // value |value|, properties |properties|, and permissions |permissions|.
  // |value| will be cached and returned for read requests and automatically set
  // for write requests by default, unless an instance of
  // BluetoothRemoteGattService::Delegate has been provided to the associated
  // BluetoothRemoteGattService instance, in which case the delegate will handle
  // read and write requests. The service instance will contain this
  // characteristic.
  // TODO(rkc): Investigate how to handle |PROPERTY_EXTENDED_PROPERTIES|
  // correctly.
  static BluetoothLocalGattCharacteristic* Create(
      const BluetoothUUID& uuid,
      const std::vector<uint8_t>& value,
      Properties properties,
      Permissions permissions,
      BluetoothLocalGattService* service);

 protected:
  BluetoothLocalGattCharacteristic();
  ~BluetoothLocalGattCharacteristic() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(BluetoothLocalGattCharacteristic);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_LOCAL_GATT_CHARACTERISTIC_H_
