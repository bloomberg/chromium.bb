// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_BLUETOOTH_BLUETOOTH_ADAPTER_H_
#define CHROME_BROWSER_CHROMEOS_BLUETOOTH_BLUETOOTH_ADAPTER_H_
#pragma once

#include <string>

#include "base/basictypes.h"

namespace chromeos {

class BluetoothDevice;

// BluetoothAdapter is an interface class representing a local bluetoooth
// interface through which we can discover and interact with remote bluetooth
// devices.
class BluetoothAdapter {
 public:
  // Interface for observing changes from bluetooth adapters.
  class Observer {
   public:
    // The discovery process has started on adapter |adapter_id|.
    virtual void DiscoveryStarted(const std::string& adapter_id) {}

    // The discovery process has ended on adapter |adapter_id|.
    virtual void DiscoveryEnded(const std::string& adapter_id) {}

    // A device has been discovered on adapter |adapter_id|.
    // The observer should not cache the |device| pointer.
    virtual void DeviceFound(const std::string& adapter_id,
                             BluetoothDevice* device) {}

    virtual ~Observer() {}
  };

  virtual ~BluetoothAdapter();

  // Adds and removes the observer.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Returns the unique identifier for this adapter.
  virtual const std::string& Id() const = 0;

  // Starts a device discovery on this adapter.
  // Caller should call AddObserver() first since results will be received via
  // the Observer interface.
  virtual void StartDiscovery() = 0;

  // Cancels any previous device discovery on this adapter.
  virtual void StopDiscovery() = 0;

  // Creates an adapter with id |id|.
  static BluetoothAdapter* Create(const std::string& id);

 protected:
  BluetoothAdapter();

 private:
  DISALLOW_COPY_AND_ASSIGN(BluetoothAdapter);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_BLUETOOTH_BLUETOOTH_ADAPTER_H_
