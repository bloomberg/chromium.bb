// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_BLUETOOTH_EVENT_ROUTER_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_BLUETOOTH_EVENT_ROUTER_H_
#pragma once

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/bluetooth/bluetooth_adapter.h"
#include "chrome/browser/profiles/profile.h"

namespace chromeos {

class ExtensionBluetoothEventRouter
    : public chromeos::BluetoothAdapter::Observer {
 public:
  explicit ExtensionBluetoothEventRouter(Profile* profile);
  virtual ~ExtensionBluetoothEventRouter();

  const chromeos::BluetoothAdapter* adapter() const { return adapter_.get(); }
  chromeos::BluetoothAdapter* GetMutableAdapter() { return adapter_.get(); }

  // Override from chromeos::BluetoothAdapter::Observer
  virtual void AdapterPresentChanged(
      chromeos::BluetoothAdapter* adapter, bool present) OVERRIDE;
  virtual void AdapterPoweredChanged(
      chromeos::BluetoothAdapter* adapter, bool has_power) OVERRIDE;
 private:
  void DispatchEvent(const char* event_name, bool value);

  Profile* profile_;
  scoped_ptr<chromeos::BluetoothAdapter> adapter_;
  DISALLOW_COPY_AND_ASSIGN(ExtensionBluetoothEventRouter);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_BLUETOOTH_EVENT_ROUTER_H_
