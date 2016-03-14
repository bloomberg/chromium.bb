// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_MOCK_BLUETOOTH_CBPERIPHERAL_MAC_H_
#define DEVICE_BLUETOOTH_MOCK_BLUETOOTH_CBPERIPHERAL_MAC_H_

#include "base/mac/sdk_forward_declarations.h"
#include "build/build_config.h"

#import <CoreBluetooth/CoreBluetooth.h>

// This class mocks the behavior of a CBPeripheral.
@interface MockCBPeripheral : NSObject

@property(nonatomic, readonly) CBPeripheralState state;
@property(nonatomic, readonly) NSUUID* identifier;
@property(nonatomic, readonly) NSString* name;

- (instancetype)initWithIdentifier:(NSUUID*)identifier;
- (void)setState:(CBPeripheralState)state;

@end

#endif  // DEVICE_BLUETOOTH_MOCK_BLUETOOTH_CBPERIPHERAL_MAC_H_
