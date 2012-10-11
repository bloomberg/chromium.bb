// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_DBUS_THREAD_MANAGER_OBSERVER_H_
#define CHROMEOS_DBUS_DBUS_THREAD_MANAGER_OBSERVER_H_

namespace chromeos {

class DBusThreadManager;

// Interface for classes that are interested in observing changes to the
// DBusThreadManager.
class DBusThreadManagerObserver {
 public:
  // Called when |manager| is about to be destroyed.  |manager| is still usable
  // at this point.
  virtual void OnDBusThreadManagerDestroying(DBusThreadManager* manager) = 0;

 protected:
  virtual ~DBusThreadManagerObserver() {}
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_DBUS_THREAD_MANAGER_OBSERVER_H_
