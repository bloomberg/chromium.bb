// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_LOW_ENERGY_PERIPHERAL_MANAGER_DELEGATE_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_LOW_ENERGY_PERIPHERAL_MANAGER_DELEGATE_H_

#include "base/mac/sdk_forward_declarations.h"
#include "base/memory/ref_counted.h"
#include "build/build_config.h"

#if defined(OS_IOS)
#import <CoreBluetooth/CoreBluetooth.h>
#else
#import <IOBluetooth/IOBluetooth.h>
#endif

namespace device {
class BluetoothAdapterMac;
class BluetoothLowEnergyAdvertisementManagerMac;
}  // namespace device

@interface BluetoothLowEnergyPeripheralManagerDelegate
    : NSObject<CBPeripheralManagerDelegate>

- (instancetype)
initWithAdvertisementManager:
    (device::BluetoothLowEnergyAdvertisementManagerMac*)advertisementManager
                  andAdapter:(device::BluetoothAdapterMac*)adapter;

@end

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_LOW_ENERGY_PERIPHERAL_MANAGER_DELEGATE_H_
