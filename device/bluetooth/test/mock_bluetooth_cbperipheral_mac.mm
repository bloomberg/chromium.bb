// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/mock_bluetooth_cbperipheral_mac.h"

#include "base/mac/foundation_util.h"
#include "base/mac/scoped_nsobject.h"
#include "device/bluetooth/test/bluetooth_test.h"

using base::mac::ObjCCast;
using base::scoped_nsobject;

@interface MockCBPeripheral () {
  scoped_nsobject<NSUUID> _identifier;
  scoped_nsobject<NSString> _name;
}

@end

@implementation MockCBPeripheral

@synthesize state = _state;

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
}

- (NSUUID*)identifier {
  return _identifier.get();
}

- (NSString*)name {
  return _name.get();
}

- (CBPeripheral*)peripheral {
  return ObjCCast<CBPeripheral>(self);
}

@end
