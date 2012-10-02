// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TODO(youngki): Guard the ChromeOS specific part with "#ifdef CHROME_OS" block
// once we move this code into common directory.

#include "base/lazy_instance.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/chromeos/bluetooth/bluetooth_adapter_chromeos.h"
#include "chrome/browser/chromeos/bluetooth/bluetooth_adapter_factory.h"

namespace {

// Shared default adapter instance, we don't want to keep this class around
// if nobody is using it so use a WeakPtr and create the object when needed;
// since Google C++ Style (and clang's static analyzer) forbids us having
// exit-time destructors we use a leaky lazy instance for it.
base::LazyInstance<base::WeakPtr<chromeos::BluetoothAdapter> >::Leaky
    default_adapter = LAZY_INSTANCE_INITIALIZER;

}  // namespace

namespace chromeos {

// static
scoped_refptr<BluetoothAdapter> BluetoothAdapterFactory::DefaultAdapter() {
  if (!default_adapter.Get().get()) {
    BluetoothAdapterChromeOs* new_adapter = new BluetoothAdapterChromeOs;
    new_adapter->TrackDefaultAdapter();
    default_adapter.Get() = new_adapter->weak_ptr_factory_.GetWeakPtr();
  }

  return scoped_refptr<BluetoothAdapter>(default_adapter.Get());
}

// static
BluetoothAdapter* BluetoothAdapterFactory::Create(const std::string& address) {
  BluetoothAdapterChromeOs* adapter = new BluetoothAdapterChromeOs;
  adapter->FindAdapter(address);
  return adapter;
}

}  // namespace chromeos
