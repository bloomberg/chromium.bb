// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_SYSTEM_CLOCK_CLIENT_H_
#define CHROMEOS_DBUS_SYSTEM_CLOCK_CLIENT_H_

#include "base/observer_list.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/dbus_client_implementation_type.h"

namespace dbus {
class Bus;
}  // namespace

namespace chromeos {

// SystemClockClient is used to communicate with the system clock.
class CHROMEOS_EXPORT SystemClockClient {
 public:
  // Interface for observing changes from the system clock.
  class Observer {
   public:
    // Called when the status is updated.
    virtual void SystemClockUpdated() {}
   protected:
    virtual ~Observer() {}
  };

  virtual ~SystemClockClient();

  // Adds and removes the observer.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;
  // Returns true if this object has the given observer.
  virtual bool HasObserver(Observer* observer) = 0;

  // Creates the instance.
  static SystemClockClient* Create(DBusClientImplementationType type,
                                   dbus::Bus* bus);

 protected:
  // Create() should be used instead.
  SystemClockClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(SystemClockClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_SYSTEM_CLOCK_CLIENT_H_
