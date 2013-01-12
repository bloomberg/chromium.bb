// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_FACTORY_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_FACTORY_H_

#include <string>

#include "base/callback.h"
#include "base/memory/ref_counted.h"

namespace device {

class BluetoothAdapter;

// BluetoothAdapterFactory is a class that contains static methods, which
// instantiate either a specific Bluetooth adapter, or the generic "default
// adapter" which may change depending on availability.
class BluetoothAdapterFactory {
 public:
  typedef base::Callback<void(scoped_refptr<device::BluetoothAdapter> adapter)>
      AdapterCallback;

  // Returns true if the Bluetooth adapter is available for the current
  // platform.
  static bool IsBluetoothAdapterAvailable();

  // Runs the callback with the shared instance for the default adapter when the
  // adapter is available to be used.
  static void RunCallbackOnAdapterReady(const AdapterCallback& callback);

  // Returns the shared instance of the adapter that has already been created.
  // It returns NULL if no adapter has been created at the time.
  static scoped_refptr<BluetoothAdapter> GetAdapter();

  // Creates an instance for a specific adapter at address |address|.
  static BluetoothAdapter* Create(const std::string& address);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_FACTORY_H_
