// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/mock_bluetooth_cbservice_mac.h"

#include "base/mac/foundation_util.h"
#include "base/mac/scoped_nsobject.h"
#include "device/bluetooth/test/bluetooth_test.h"

using base::mac::ObjCCast;
using base::scoped_nsobject;

@interface MockCBService () {
  CBUUID* _UUID;
  BOOL _primary;
}

@end

@implementation MockCBService

@synthesize UUID = _UUID;
@synthesize isPrimary = _primary;

- (instancetype)initWithCBUUID:(CBUUID*)uuid primary:(BOOL)isPrimary {
  self = [super init];
  if (self) {
    _UUID = [uuid retain];
    _primary = isPrimary;
  }
  return self;
}

- (void)dealloc {
  [_UUID release];
  [super dealloc];
}

- (BOOL)isKindOfClass:(Class)aClass {
  if (aClass == [CBService class] ||
      [aClass isSubclassOfClass:[CBService class]]) {
    return YES;
  }
  return [super isKindOfClass:aClass];
}

- (BOOL)isMemberOfClass:(Class)aClass {
  if (aClass == [CBService class] ||
      [aClass isSubclassOfClass:[CBService class]]) {
    return YES;
  }
  return [super isKindOfClass:aClass];
}

- (CBService*)service {
  return ObjCCast<CBService>(self);
}

@end
