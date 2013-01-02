// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_ROOT_POWER_MANAGER_CLIENT_H_
#define CHROMEOS_DBUS_ROOT_POWER_MANAGER_CLIENT_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/dbus_client_implementation_type.h"

namespace dbus {
class Bus;
}

namespace chromeos {

class RootPowerManagerObserver;

// RootPowerManagerClient is used to communicate with the powerm process.
// TODO(derat): Remove this class after the signals that it listens for have
// been moved to powerd: http://crosbug.com/36804
class CHROMEOS_EXPORT RootPowerManagerClient {
 public:
  // Adds and removes observers.
  virtual void AddObserver(RootPowerManagerObserver* observer) = 0;
  virtual void RemoveObserver(RootPowerManagerObserver* observer) = 0;
  virtual bool HasObserver(RootPowerManagerObserver* observer) = 0;

  // Creates the instance.
  static RootPowerManagerClient* Create(DBusClientImplementationType type,
                                        dbus::Bus* bus);

  virtual ~RootPowerManagerClient();

 protected:
  // Create() should be used instead.
  RootPowerManagerClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(RootPowerManagerClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_ROOT_POWER_MANAGER_CLIENT_H_
