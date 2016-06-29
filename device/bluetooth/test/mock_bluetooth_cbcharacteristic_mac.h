// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_CBCHARACTERISTIC_MAC_H_
#define DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_CBCHARACTERISTIC_MAC_H_

#include "base/mac/sdk_forward_declarations.h"
#include "build/build_config.h"

#import <CoreBluetooth/CoreBluetooth.h>

// This class mocks the behavior of a CBCharacteristic.
@interface MockCBCharacteristic : NSObject

@property(readonly, nonatomic) CBUUID* UUID;
@property(readonly, nonatomic) CBCharacteristic* characteristic;

- (instancetype)initWithService:(CBService*)service
                         CBUUID:(CBUUID*)uuid
                     properties:(int)properties;

// Methods for faking events.
- (void)simulateReadWithValue:(NSData*)value error:(NSError*)error;
- (void)simulateWriteWithError:(NSError*)error;
- (void)simulateGattNotifySessionStarted;
- (void)simulateGattNotifySessionFailedWithError:(NSError*)error;
- (void)simulateGattCharacteristicChangedWithValue:(NSData*)value;

@end

#endif  // DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_CBCHARACTERISTIC_MAC_H_
