// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_CBSERVICE_MAC_H_
#define DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_CBSERVICE_MAC_H_

#include "base/mac/sdk_forward_declarations.h"
#include "build/build_config.h"

#import <CoreBluetooth/CoreBluetooth.h>

// This class mocks the behavior of a CBService.
@interface MockCBService : NSObject

@property(readonly, nonatomic) CBUUID* UUID;
@property(readonly, nonatomic) BOOL isPrimary;
@property(readonly, nonatomic) CBService* service;

- (instancetype)initWithCBUUID:(CBUUID*)uuid primary:(BOOL)isPrimary;

@end

#endif  // DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_CBSERVICE_MAC_H_
