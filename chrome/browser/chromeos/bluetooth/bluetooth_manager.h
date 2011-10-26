// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_BLUETOOTH_BLUETOOTH_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_BLUETOOTH_BLUETOOTH_MANAGER_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"

namespace chromeos {

class BluetoothAdapter;

// BluetoothManager provides the Chrome OS bluetooth API.
// This API is simplified relative to what's available via D-Bus.
// We'll export additional functionality as needed.
class BluetoothManager {
 public:
  // Interface for observing changes from the bluetooth manager.
  class Observer {
   public:
    // Notification that the default adapter has changed. If |adapter| is NULL,
    // it means that there are no longer any adapters.
    // Clients should not cache this pointer.
    virtual void DefaultAdapterChanged(BluetoothAdapter* adapter) {}
    virtual ~Observer() {}
  };

  // Adds and removes the observer.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Returns the default local bluetooth adapter or NULL if there is none.
  // Clients should not cache this pointer.
  virtual BluetoothAdapter* DefaultAdapter() = 0;

  // Creates the global BluetoothManager instance.
  static void Initialize();

  // Destroys the global BluetoothManager instance if it exists.
  static void Shutdown();

  // Returns a pointer to the global BluetoothManager instance.
  // Initialize() should already have been called.
  static BluetoothManager* GetInstance();

 protected:
  BluetoothManager();
  virtual ~BluetoothManager();

 private:
  DISALLOW_COPY_AND_ASSIGN(BluetoothManager);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_BLUETOOTH_BLUETOOTH_MANAGER_H_
