// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_adapter_factory.h"

#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/chromeos_switches.h"
#include "device/bluetooth/bluetooth_adapter.h"

#if defined(OS_CHROMEOS)
#include "device/bluetooth/bluetooth_adapter_chromeos.h"
#include "device/bluetooth/bluetooth_adapter_chromeos_experimental.h"
#elif defined(OS_WIN)
#include "device/bluetooth/bluetooth_adapter_win.h"
#elif defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#include "device/bluetooth/bluetooth_adapter_mac.h"
#endif

namespace {

using device::BluetoothAdapter;
using device::BluetoothAdapterFactory;

// Shared default adapter instance, we don't want to keep this class around
// if nobody is using it so use a WeakPtr and create the object when needed;
// since Google C++ Style (and clang's static analyzer) forbids us having
// exit-time destructors we use a leaky lazy instance for it.
base::LazyInstance<base::WeakPtr<device::BluetoothAdapter> >::Leaky
    default_adapter = LAZY_INSTANCE_INITIALIZER;

typedef std::vector<BluetoothAdapterFactory::AdapterCallback>
    AdapterCallbackList;

// List of adapter callbacks to be called once the adapter is initialized.
// Since Google C++ Style (and clang's static analyzer) forbids us having
// exit-time destructors we use a lazy instance for it.
base::LazyInstance<AdapterCallbackList> adapter_callbacks =
    LAZY_INSTANCE_INITIALIZER;

void RunAdapterCallbacks() {
  CHECK(default_adapter.Get().get());
  scoped_refptr<BluetoothAdapter> adapter(default_adapter.Get());
  for (std::vector<BluetoothAdapterFactory::AdapterCallback>::const_iterator
           iter = adapter_callbacks.Get().begin();
       iter != adapter_callbacks.Get().end();
       ++iter) {
    iter->Run(adapter);
  }
  adapter_callbacks.Get().clear();
}

}  // namespace

namespace device {

// static
bool BluetoothAdapterFactory::IsBluetoothAdapterAvailable() {
#if defined(OS_CHROMEOS)
  return true;
#elif defined(OS_WIN)
  return true;
#elif defined(OS_MACOSX)
  return base::mac::IsOSLionOrLater();
#endif
  return false;
}

// static
void BluetoothAdapterFactory::GetAdapter(const AdapterCallback& callback) {
  if (!default_adapter.Get().get()) {
#if defined(OS_CHROMEOS)
    if (CommandLine::ForCurrentProcess()->HasSwitch(
        chromeos::switches::kEnableExperimentalBluetooth)) {
      chromeos::BluetoothAdapterChromeOSExperimental* new_adapter =
          new chromeos::BluetoothAdapterChromeOSExperimental;
      new_adapter->TrackDefaultAdapter();
      default_adapter.Get() = new_adapter->weak_ptr_factory_.GetWeakPtr();
    } else {
      chromeos::BluetoothAdapterChromeOS* new_adapter =
          new chromeos::BluetoothAdapterChromeOS;
      new_adapter->TrackDefaultAdapter();
      default_adapter.Get() = new_adapter->weak_ptr_factory_.GetWeakPtr();
    }
#elif defined(OS_WIN)
    BluetoothAdapterWin* new_adapter = new BluetoothAdapterWin(
        base::Bind(&RunAdapterCallbacks));
    new_adapter->TrackDefaultAdapter();
    default_adapter.Get() = new_adapter->weak_ptr_factory_.GetWeakPtr();
#elif defined(OS_MACOSX)
    BluetoothAdapterMac* new_adapter = new BluetoothAdapterMac();
    new_adapter->TrackDefaultAdapter();
    default_adapter.Get() = new_adapter->weak_ptr_factory_.GetWeakPtr();
#endif
  }

  if (default_adapter.Get()->IsInitialized()) {
    callback.Run(scoped_refptr<BluetoothAdapter>(default_adapter.Get()));
  } else {
    adapter_callbacks.Get().push_back(callback);
  }
}

// static
scoped_refptr<BluetoothAdapter> BluetoothAdapterFactory::MaybeGetAdapter() {
  return scoped_refptr<BluetoothAdapter>(default_adapter.Get());
}

}  // namespace device
