// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/mock_bluetooth_cbperipheral_mac.h"

#include "base/mac/foundation_util.h"
#include "base/mac/scoped_nsobject.h"
#include "device/bluetooth/test/bluetooth_test_mac.h"
#include "device/bluetooth/test/mock_bluetooth_cbservice_mac.h"

using base::mac::ObjCCast;
using base::scoped_nsobject;

@interface MockCBPeripheral () {
  scoped_nsobject<NSUUID> _identifier;
  scoped_nsobject<NSString> _name;
  id<CBPeripheralDelegate> _delegate;
  scoped_nsobject<NSMutableArray> _services;
}

@end

@implementation MockCBPeripheral

@synthesize state = _state;
@synthesize delegate = _delegate;
@synthesize bluetoothTestMac = _bluetoothTestMac;

- (instancetype)init {
  [self doesNotRecognizeSelector:_cmd];
  return self;
}

- (instancetype)initWithUTF8StringIdentifier:(const char*)utf8Identifier {
  scoped_nsobject<NSUUID> identifier(
      [[NSUUID alloc] initWithUUIDString:@(utf8Identifier)]);
  return [self initWithIdentifier:identifier name:nil];
}

- (instancetype)initWithIdentifier:(NSUUID*)identifier {
  return [self initWithIdentifier:identifier name:nil];
}

- (instancetype)initWithIdentifier:(NSUUID*)identifier name:(NSString*)name {
  self = [super init];
  if (self) {
    _identifier.reset([identifier retain]);
    if (name) {
      _name.reset([name retain]);
    } else {
      _name.reset(
          [@(device::BluetoothTestBase::kTestDeviceName.c_str()) retain]);
    }
    _state = CBPeripheralStateDisconnected;
  }
  return self;
}

- (BOOL)isKindOfClass:(Class)aClass {
  if (aClass == [CBPeripheral class] ||
      [aClass isSubclassOfClass:[CBPeripheral class]]) {
    return YES;
  }
  return [super isKindOfClass:aClass];
}

- (BOOL)isMemberOfClass:(Class)aClass {
  if (aClass == [CBPeripheral class] ||
      [aClass isSubclassOfClass:[CBPeripheral class]]) {
    return YES;
  }
  return [super isKindOfClass:aClass];
}

- (void)setState:(CBPeripheralState)state {
  _state = state;
  if (_state == CBPeripheralStateDisconnected) {
    _services.reset();
  }
}

- (void)discoverServices:(NSArray*)serviceUUIDs {
  if (_bluetoothTestMac) {
    _bluetoothTestMac->OnFakeBluetoothServiceDiscovery();
  }
  [_delegate peripheral:self.peripheral didDiscoverServices:nil];
}

- (void)discoverCharacteristics:(NSArray*)characteristics
                     forService:(CBService*)service {
}

- (void)readValueForCharacteristic:(CBCharacteristic*)characteristic {
  DCHECK(_bluetoothTestMac);
  _bluetoothTestMac->OnFakeBluetoothCharacteristicReadValue();
}

- (void)writeValue:(NSData*)data
    forCharacteristic:(CBCharacteristic*)characteristic
                 type:(CBCharacteristicWriteType)type {
  DCHECK(_bluetoothTestMac);
  const uint8_t* buffer = static_cast<const uint8_t*>(data.bytes);
  std::vector<uint8_t> value(buffer, buffer + data.length);
  _bluetoothTestMac->OnFakeBluetoothCharacteristicWriteValue(value);
}

- (void)removeAllServices {
  [_services.get() removeAllObjects];
}

- (void)addServices:(NSArray*)services {
  if (!_services.get()) {
    _services.reset([[NSMutableArray alloc] init]);
  }
  for (CBUUID* uuid in services) {
    base::scoped_nsobject<MockCBService> service([[MockCBService alloc]
        initWithPeripheral:self.peripheral
                    CBUUID:uuid
                   primary:YES]);
    [_services.get() addObject:service.get().service];
  }
}

- (void)didDiscoverServicesWithError:(NSError*)error {
  [_delegate peripheral:self.peripheral didDiscoverServices:error];
}

- (void)removeService:(CBService*)service {
  base::scoped_nsobject<CBService> serviceToRemove(service,
                                                   base::scoped_policy::RETAIN);
  DCHECK(serviceToRemove);
  [_services.get() removeObject:serviceToRemove];
  [self didModifyServices:@[ serviceToRemove ]];
}

- (void)didDiscoverCharactericsForAllServices {
  for (CBService* service in _services.get()) {
    [_delegate peripheral:self.peripheral
        didDiscoverCharacteristicsForService:service
                                       error:nil];
  }
}

- (void)didModifyServices:(NSArray*)invalidatedServices {
  // -[CBPeripheralDelegate peripheral:didModifyServices:] is only available
  // with 10.9. It is safe to call this method (even if chrome is running on
  // 10.8) since WebBluetooth is enabled only with 10.10.
  DCHECK(
      [_delegate respondsToSelector:@selector(peripheral:didModifyServices:)]);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpartial-availability"
  [_delegate peripheral:self.peripheral didModifyServices:invalidatedServices];
#pragma clang diagnostic pop
}

- (NSUUID*)identifier {
  return _identifier.get();
}

- (NSString*)name {
  return _name.get();
}

- (NSArray*)services {
  return _services.get();
}

- (CBPeripheral*)peripheral {
  return ObjCCast<CBPeripheral>(self);
}

- (void)setNotifyValue:(BOOL)notification
     forCharacteristic:(CBCharacteristic*)characteristic {
  _bluetoothTestMac->OnFakeBluetoothGattSetCharacteristicNotification();
}

@end
