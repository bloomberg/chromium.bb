// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DBUS_DBUS_THREAD_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_DBUS_DBUS_THREAD_MANAGER_H_
#pragma once

#include "base/memory/scoped_ptr.h"
#include "base/memory/ref_counted.h"

namespace base {
class Thread;
};

namespace dbus {
class Bus;
};

namespace chromeos {

class CrosDBusService;

// DBusThreadManager manages the D-Bus thread, the thread dedicated to
// handling asynchronous D-Bus operations.
//
// This class also manages D-Bus connections and D-Bus clients, which
// depend on the D-Bus thread to ensure the right order of shutdowns for
// the D-Bus thread, the D-Bus connections, and the D-Bus clients.
//
// CALLBACKS IN D-BUS CLIENTS:
//
// D-Bus clients managed by DBusThreadManager are guaranteed to be deleted
// after the D-Bus thread so the clients don't need to worry if new
// incoming messages arrive from the D-Bus thread during shutdown of the
// clients. However, the UI message loop may still be running during the
// shutdown, hence the D-Bus clients should inherit
// base::RefCountedThreadSafe if they export methods or call methods, to
// ensure that callbacks can reference |this| safely on the UI thread
// during the shutdown.
//
class DBusThreadManager {
 public:
  // Sets the global instance. Must be called before any calls to Get().
  // We explicitly initialize and shut down the global object, rather than
  // making it a Singleton, to ensure clean startup and shutdown.
  static void Initialize();

  // Destroys the global instance.
  static void Shutdown();

  // Gets the global instance. Initialize() must be called first.
  static DBusThreadManager* Get();

  // Returns the connection to the system bus.
  dbus::Bus* system_bus() { return system_bus_.get(); }

 private:
  DBusThreadManager();
  virtual ~DBusThreadManager();

  scoped_ptr<base::Thread> dbus_thread_;
  scoped_refptr<dbus::Bus> system_bus_;
  CrosDBusService* cros_dbus_service_;

  DISALLOW_COPY_AND_ASSIGN(DBusThreadManager);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DBUS_DBUS_THREAD_MANAGER_H_
