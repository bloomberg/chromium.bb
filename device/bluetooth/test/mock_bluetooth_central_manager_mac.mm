// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "mock_bluetooth_central_manager_mac.h"

@implementation MockCentralManager

@synthesize scanForPeripheralsCallCount = _scanForPeripheralsCallCount;
@synthesize stopScanCallCount = _stopScanCallCount;
@synthesize delegate = _delegate;
@synthesize state = _state;

- (instancetype)init {
  _scanForPeripheralsCallCount = 0;
  _stopScanCallCount = 0;
  return self;
}

- (instancetype)initWithDelegate:(id<CBCentralManagerDelegate>)delegate
                           queue:(dispatch_queue_t)queue
                         options:(NSDictionary*)options {
  return [self init];
}

- (void)scanForPeripheralsWithServices:(NSArray*)serviceUUIDs
                               options:(NSDictionary*)options {
  _scanForPeripheralsCallCount++;
}

- (void)stopScan {
  _stopScanCallCount++;
}

@end
