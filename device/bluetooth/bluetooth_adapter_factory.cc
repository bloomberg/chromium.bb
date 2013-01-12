// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_adapter_factory.h"

#include "base/lazy_instance.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "device/bluetooth/bluetooth_adapter.h"

#if defined(OS_CHROMEOS)
#include "device/bluetooth/bluetooth_adapter_chromeos.h"
#endif

namespace {

// Shared default adapter instance, we don't want to keep this class around
// if nobody is using it so use a WeakPtr and create the object when needed;
// since Google C++ Style (and clang's static analyzer) forbids us having
// exit-time destructors we use a leaky lazy instance for it.
base::LazyInstance<base::WeakPtr<device::BluetoothAdapter> >::Leaky
    default_adapter = LAZY_INSTANCE_INITIALIZER;

}  // namespace

namespace device {

// static
bool BluetoothAdapterFactory::IsBluetoothAdapterAvailable() {
#if defined(OS_CHROMEOS)
  return true;
#endif
  return false;
}

// static
void BluetoothAdapterFactory::RunCallbackOnAdapterReady(
    const AdapterCallback& callback) {
  if (!default_adapter.Get().get()) {
#if defined(OS_CHROMEOS)
    chromeos::BluetoothAdapterChromeOs* new_adapter =
        new chromeos::BluetoothAdapterChromeOs;
    new_adapter->TrackDefaultAdapter();
    default_adapter.Get() = new_adapter->weak_ptr_factory_.GetWeakPtr();
#endif
  }

  callback.Run(scoped_refptr<BluetoothAdapter>(default_adapter.Get()));
}

// static
scoped_refptr<BluetoothAdapter> BluetoothAdapterFactory::GetAdapter() {
  return scoped_refptr<BluetoothAdapter>(default_adapter.Get());
}

// static
BluetoothAdapter* BluetoothAdapterFactory::Create(const std::string& address) {
  BluetoothAdapter* adapter = NULL;
#if defined(OS_CHROMEOS)
  chromeos::BluetoothAdapterChromeOs* adapter_chromeos =
      new chromeos::BluetoothAdapterChromeOs;
  adapter_chromeos->FindAdapter(address);
  adapter = adapter_chromeos;
#endif
  return adapter;
}

}  // namespace device
