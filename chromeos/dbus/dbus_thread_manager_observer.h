// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_DBUS_THREAD_MANAGER_OBSERVER_H_
#define CHROMEOS_DBUS_DBUS_THREAD_MANAGER_OBSERVER_H_

#include "base/basictypes.h"

namespace chromeos {

class DBusThreadManager;

// Interface for classes that are interested in observing changes to the
// DBusThreadManager.
class DBusThreadManagerObserver {
 public:
  DBusThreadManagerObserver() {}
  virtual ~DBusThreadManagerObserver() {}

  // Called when |manager| is about to be destroyed.  |manager| is still usable
  // at this point.
  virtual void OnDBusThreadManagerDestroying(DBusThreadManager* manager) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(DBusThreadManagerObserver);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_DBUS_THREAD_MANAGER_OBSERVER_H_
