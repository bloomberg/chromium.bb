// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/mock_bluetooth_cbdescriptor_mac.h"

#include "base/mac/foundation_util.h"
#include "base/mac/scoped_nsobject.h"
#include "device/bluetooth/bluetooth_gatt_characteristic.h"

using base::mac::ObjCCast;
using base::scoped_nsobject;

@interface MockCBDescriptor () {
  // Owner of this instance.
  CBCharacteristic* _characteristic;
  scoped_nsobject<CBUUID> _UUID;
}
@end

@implementation MockCBDescriptor

- (instancetype)initWithCharacteristic:(CBCharacteristic*)characteristic
                                CBUUID:(CBUUID*)uuid {
  self = [super init];
  if (self) {
    _characteristic = characteristic;
    _UUID.reset([uuid retain]);
  }
  return self;
}

- (BOOL)isKindOfClass:(Class)aClass {
  if (aClass == [CBDescriptor class] ||
      [aClass isSubclassOfClass:[CBDescriptor class]]) {
    return YES;
  }
  return [super isKindOfClass:aClass];
}

- (BOOL)isMemberOfClass:(Class)aClass {
  if (aClass == [CBDescriptor class] ||
      [aClass isSubclassOfClass:[CBDescriptor class]]) {
    return YES;
  }
  return [super isKindOfClass:aClass];
}

- (CBUUID*)UUID {
  return _UUID.get();
}

- (CBDescriptor*)descriptor {
  return ObjCCast<CBDescriptor>(self);
}

- (CBCharacteristic*)characteristic {
  return _characteristic;
}

@end
