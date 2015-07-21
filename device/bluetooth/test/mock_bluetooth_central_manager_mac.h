// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_MOCK_BLUETOOTH_CENTRAL_MANAGER_MAC_H_
#define DEVICE_BLUETOOTH_MOCK_BLUETOOTH_CENTRAL_MANAGER_MAC_H_

#include "base/mac/sdk_forward_declarations.h"

#if defined(OS_IOS)
#import <CoreBluetooth/CoreBluetooth.h>
#else
#import <IOBluetooth/IOBluetooth.h>
#endif

// Class to mock a CBCentralManager. Cannot use a OCMockObject because mocking
// the 'state' property gives a compiler warning when mock_central_manager is of
// type id (multiple methods named 'state' found), and a compiler warning when
// mock_central_manager is of type CBCentralManager (CBCentralManager may not
// respond to 'stub').
@interface MockCentralManager : NSObject

@property(nonatomic, assign) BOOL scanForPeripheralsCallCount;
@property(nonatomic, assign) BOOL stopScanCallCount;
@property(nonatomic, assign) id<CBCentralManagerDelegate> delegate;
@property(nonatomic, assign) CBCentralManagerState state;

// Designated initializer
- (instancetype)init;

- (instancetype)initWithDelegate:(id<CBCentralManagerDelegate>)delegate
                           queue:(dispatch_queue_t)queue
                         options:(NSDictionary*)options;

- (void)scanForPeripheralsWithServices:(NSArray*)serviceUUIDs
                               options:(NSDictionary*)options;

- (void)stopScan;

@end

#endif  // DEVICE_BLUETOOTH_MOCK_BLUETOOTH_CENTRAL_MANAGER_MAC_H_
