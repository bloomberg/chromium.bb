// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_BLUETOOTH_BLUETOOTH_OBSERVER_H_
#define ASH_SYSTEM_BLUETOOTH_BLUETOOTH_OBSERVER_H_

namespace ash {

class BluetoothObserver {
 public:
  virtual ~BluetoothObserver() {}

  // Called when the state of Bluetooth in the system changes.
  virtual void OnBluetoothSystemStateChanged() {}

  // Called when a Bluetooth scan has started or stopped.
  virtual void OnBluetoothScanStateChanged() {}

  // Called when a device was added, removed, or changed.
  virtual void OnBluetoothDeviceListChanged() {}
};

}  // namespace ash

#endif  // ASH_SYSTEM_BLUETOOTH_BLUETOOTH_OBSERVER_H_
