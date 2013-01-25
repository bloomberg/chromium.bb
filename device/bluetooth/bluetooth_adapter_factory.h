// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_FACTORY_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_FACTORY_H_

#include <string>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "device/bluetooth/bluetooth_adapter.h"

namespace device {

// BluetoothAdapterFactory is a class that contains static methods, which
// instantiate either a specific Bluetooth adapter, or the generic "default
// adapter" which may change depending on availability.
class BluetoothAdapterFactory {
 public:
  typedef base::Callback<void(scoped_refptr<BluetoothAdapter> adapter)>
      AdapterCallback;

  // Returns true if the Bluetooth adapter is available for the current
  // platform.
  static bool IsBluetoothAdapterAvailable();

  // Returns the shared instance of the default adapter, creating and
  // initializing it if necessary. |callback| is called with the adapter
  // instance passed only once the adapter is fully initialized and ready to
  // use.
  static void GetAdapter(const AdapterCallback& callback);

  // Returns the shared instance of the adapter that has already been created,
  // but may or may not have been initialized.
  // It returns NULL if no adapter has been created at the time.
  static scoped_refptr<BluetoothAdapter> MaybeGetAdapter();

  // Creates an instance for a specific adapter at address |address|.
  static BluetoothAdapter* Create(const std::string& address);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_FACTORY_H_
