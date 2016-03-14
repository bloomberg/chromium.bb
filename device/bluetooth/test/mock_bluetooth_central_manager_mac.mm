// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "device/bluetooth/test/mock_bluetooth_central_manager_mac.h"

#import "device/bluetooth/test/bluetooth_test_mac.h"
#import "device/bluetooth/test/mock_bluetooth_cbperipheral_mac.h"

@implementation MockCentralManager

@synthesize scanForPeripheralsCallCount = _scanForPeripheralsCallCount;
@synthesize stopScanCallCount = _stopScanCallCount;
@synthesize delegate = _delegate;
@synthesize state = _state;
@synthesize bluetoothTestMac = _bluetoothTestMac;

- (BOOL)isKindOfClass:(Class)aClass {
  if (aClass == [CBCentralManager class] ||
      [aClass isSubclassOfClass:[CBCentralManager class]]) {
    return YES;
  }
  return [super isKindOfClass:aClass];
}

- (BOOL)isMemberOfClass:(Class)aClass {
  if (aClass == [CBCentralManager class] ||
      [aClass isSubclassOfClass:[CBCentralManager class]]) {
    return YES;
  }
  return [super isKindOfClass:aClass];
}

- (void)scanForPeripheralsWithServices:(NSArray*)serviceUUIDs
                               options:(NSDictionary*)options {
  _scanForPeripheralsCallCount++;
}

- (void)stopScan {
  _stopScanCallCount++;
}

- (void)connectPeripheral:(CBPeripheral*)peripheral
                  options:(NSDictionary*)options {
  if (_bluetoothTestMac) {
    _bluetoothTestMac->OnFakeBluetoothDeviceConnectGattCalled();
  }
}

- (void)cancelPeripheralConnection:(CBPeripheral*)peripheral {
  if (_bluetoothTestMac) {
    _bluetoothTestMac->OnFakeBluetoothGattDisconnect();
  }
}

@end
