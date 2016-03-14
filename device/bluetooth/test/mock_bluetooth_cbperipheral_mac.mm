// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "mock_bluetooth_cbperipheral_mac.h"

#include "base/mac/scoped_nsobject.h"

@interface MockCBPeripheral () {
  base::scoped_nsobject<NSUUID> _identifier;
  base::scoped_nsobject<NSString> _name;
}

@end

@implementation MockCBPeripheral

@synthesize state = _state;

- (instancetype)initWithIdentifier:(NSUUID*)identifier {
  self = [super init];
  if (self) {
    _identifier.reset([identifier retain]);
    _name.reset([@"FakeBluetoothDevice" retain]);
    _state = CBPeripheralStateDisconnected;
  }
  return self;
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

@end
