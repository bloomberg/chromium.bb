// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_LOW_ENERGY_PERIPHERAL_DELEGATE_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_LOW_ENERGY_PERIPHERAL_DELEGATE_H_

#import <CoreBluetooth/CoreBluetooth.h>

#include "base/memory/ref_counted.h"
#include "build/build_config.h"

#if !defined(OS_IOS)
#import <IOBluetooth/IOBluetooth.h>
#endif

namespace device {

class BluetoothLowEnergyDeviceMac;
class BluetoothLowEnergyPeripheralBridge;

}  // namespace device

// This class will serve as the Objective-C delegate of CBPeripheral.
@interface BluetoothLowEnergyPeripheralDelegate
    : NSObject<CBPeripheralDelegate> {
  std::unique_ptr<device::BluetoothLowEnergyPeripheralBridge> _bridge;
}

- (id)initWithBluetoothLowEnergyDeviceMac:
    (device::BluetoothLowEnergyDeviceMac*)device_mac;

@end

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_LOW_ENERGY_PERIPHERAL_DELEGATE_H_
