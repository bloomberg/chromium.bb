// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_FACTORY_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_FACTORY_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_export.h"

namespace device {

// A factory class for building a Bluetooth adapter on platforms where Bluetooth
// is available.
class DEVICE_BLUETOOTH_EXPORT BluetoothAdapterFactory {
 public:
  typedef base::Callback<void(scoped_refptr<BluetoothAdapter> adapter)>
      AdapterCallback;

  // Returns true if the platform supports Bluetooth. It does not imply that
  // there is a Bluetooth radio. Use BluetoothAdapter::IsPresent to know
  // if there is a Bluetooth radio present.
  static bool IsBluetoothSupported();

  // Returns true if the platform supports Bluetooth Low Energy. This is
  // independent of whether or not there is a Bluetooth radio present e.g.
  // Windows 7 does not support BLE so IsLowEnergySupported would return
  // false. Windows 10, on the other hand, supports BLE so this function
  // returns true even if there is no Bluetooth radio on the system.
  static bool IsLowEnergySupported();

  // Returns the shared instance of the default adapter, creating and
  // initializing it if necessary. |callback| is called with the adapter
  // instance passed only once the adapter is fully initialized and ready to
  // use.
  static void GetAdapter(const AdapterCallback& callback);

#if defined(OS_CHROMEOS) || defined(OS_LINUX)
  // Calls |BluetoothAdapter::Shutdown| on the adapter if
  // present.
  static void Shutdown();
#endif

  // Sets the shared instance of the default adapter for testing purposes only,
  // no reference is retained after completion of the call, removing the last
  // reference will reset the factory.
  static void SetAdapterForTesting(scoped_refptr<BluetoothAdapter> adapter);

  // Returns true iff the implementation has a (non-NULL) shared instance of the
  // adapter. Exposed for testing.
  static bool HasSharedInstanceForTesting();
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_FACTORY_H_
