// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/mock_bluetooth_cbcharacteristic_mac.h"

#include "base/mac/foundation_util.h"
#include "base/mac/scoped_nsobject.h"
#include "device/bluetooth/bluetooth_gatt_characteristic.h"

using base::mac::ObjCCast;
using base::scoped_nsobject;

namespace device {

namespace {

CBCharacteristicProperties GattCharacteristicPropertyToCBCharacteristicProperty(
    BluetoothGattCharacteristic::Properties cb_property) {
  CBCharacteristicProperties result =
      static_cast<CBCharacteristicProperties>(0);
  if (cb_property & BluetoothGattCharacteristic::PROPERTY_BROADCAST) {
    result |= CBCharacteristicPropertyBroadcast;
  }
  if (cb_property & BluetoothGattCharacteristic::PROPERTY_READ) {
    result |= CBCharacteristicPropertyRead;
  }
  if (cb_property &
      BluetoothGattCharacteristic::PROPERTY_WRITE_WITHOUT_RESPONSE) {
    result |= CBCharacteristicPropertyWriteWithoutResponse;
  }
  if (cb_property & BluetoothGattCharacteristic::PROPERTY_WRITE) {
    result |= CBCharacteristicPropertyWrite;
  }
  if (cb_property & BluetoothGattCharacteristic::PROPERTY_NOTIFY) {
    result |= CBCharacteristicPropertyNotify;
  }
  if (cb_property & BluetoothGattCharacteristic::PROPERTY_INDICATE) {
    result |= CBCharacteristicPropertyIndicate;
  }
  if (cb_property &
      BluetoothGattCharacteristic::PROPERTY_AUTHENTICATED_SIGNED_WRITES) {
    result |= CBCharacteristicPropertyAuthenticatedSignedWrites;
  }
  if (cb_property & BluetoothGattCharacteristic::PROPERTY_EXTENDED_PROPERTIES) {
    result |= CBCharacteristicPropertyExtendedProperties;
  }
  if (cb_property & BluetoothGattCharacteristic::PROPERTY_RELIABLE_WRITE) {
  }
  if (cb_property &
      BluetoothGattCharacteristic::PROPERTY_WRITABLE_AUXILIARIES) {
  }
  if (cb_property & BluetoothGattCharacteristic::PROPERTY_READ_ENCRYPTED) {
    result |= CBCharacteristicPropertyRead;
  }
  if (cb_property & BluetoothGattCharacteristic::PROPERTY_WRITE_ENCRYPTED) {
    result |= CBCharacteristicPropertyWrite;
  }
  if (cb_property &
      BluetoothGattCharacteristic::PROPERTY_READ_ENCRYPTED_AUTHENTICATED) {
    result |= CBCharacteristicPropertyRead;
  }
  if (cb_property &
      BluetoothGattCharacteristic::PROPERTY_WRITE_ENCRYPTED_AUTHENTICATED) {
    result |= CBCharacteristicPropertyWrite;
  }
  DCHECK(result != static_cast<CBCharacteristicProperties>(0));
  return result;
}
}  // namespace
}  // device

@interface MockCBCharacteristic () {
  scoped_nsobject<CBUUID> _UUID;
  CBCharacteristicProperties _cb_properties;
}
@end

@implementation MockCBCharacteristic

- (instancetype)initWithCBUUID:(CBUUID*)uuid properties:(int)properties {
  self = [super init];
  if (self) {
    _UUID.reset([uuid retain]);
    _cb_properties =
        device::GattCharacteristicPropertyToCBCharacteristicProperty(
            properties);
  }
  return self;
}

- (BOOL)isKindOfClass:(Class)aClass {
  if (aClass == [CBCharacteristic class] ||
      [aClass isSubclassOfClass:[CBCharacteristic class]]) {
    return YES;
  }
  return [super isKindOfClass:aClass];
}

- (BOOL)isMemberOfClass:(Class)aClass {
  if (aClass == [CBCharacteristic class] ||
      [aClass isSubclassOfClass:[CBCharacteristic class]]) {
    return YES;
  }
  return [super isKindOfClass:aClass];
}

- (CBUUID*)UUID {
  return _UUID.get();
}

- (CBCharacteristic*)characteristic {
  return ObjCCast<CBCharacteristic>(self);
}

- (CBCharacteristicProperties)properties {
  return _cb_properties;
}

@end