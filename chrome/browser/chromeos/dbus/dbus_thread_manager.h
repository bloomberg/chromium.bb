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

class BluetoothAdapterClient;
class BluetoothManagerClient;
class CrosDisksClient;
class PowerManagerClient;
class SessionManagerClient;
class SensorsClient;
class SpeechSynthesizerClient;
class UpdateEngineClient;

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
// clients. The UI message loop is not running during the shutdown hence
// the UI message loop won't post tasks to D-BUS clients during the
// shutdown. However, to be extra cautious, clients should use
// WeakPtrFactory when creating callbacks that run on UI thread. See
// session_manager_client.cc for examples.
//
class DBusThreadManager {
 public:
  // Sets the global instance. Must be called before any calls to Get().
  // We explicitly initialize and shut down the global object, rather than
  // making it a Singleton, to ensure clean startup and shutdown.
  static void Initialize();

  // Similar to Initialize(), but can inject an alternative
  // DBusThreadManager such as MockDBusThreadManager for testing.
  // The injected object will be owned by the internal pointer and deleted
  // by Shutdown().
  static void InitializeForTesting(DBusThreadManager* dbus_thread_manager);

  // Destroys the global instance.
  static void Shutdown();

  // Gets the global instance. Initialize() must be called first.
  static DBusThreadManager* Get();

  // TODO(satorux): Rename the following getters to something like
  // GetFooClient() as they are now virtual.

  // Returns the bluetooth adapter client, owned by DBusThreadManager.
  // Do not cache this pointer and use it after DBusThreadManager is shut
  // down.
  virtual BluetoothAdapterClient* GetBluetoothAdapterClient() = 0;

  // Returns the bluetooth manager client, owned by DBusThreadManager.
  // Do not cache this pointer and use it after DBusThreadManager is shut
  // down.
  virtual BluetoothManagerClient* GetBluetoothManagerClient() = 0;

  // Returns the power manager client, owned by DBusThreadManager.
  // See also comments at session_manager_client().
  virtual PowerManagerClient* GetPowerManagerClient() = 0;

  // Returns the session manager client, owned by DBusThreadManager.
  // Do not cache this pointer and use it after DBusThreadManager is shut
  // down.
  virtual SensorsClient* GetSensorsClient() = 0;

  // Returns the session manager client, owned by DBusThreadManager.
  // Do not cache this pointer and use it after DBusThreadManager is shut
  // down.
  virtual SessionManagerClient* GetSessionManagerClient() = 0;

  // Returns the speech synthesizer client, owned by DBusThreadManager.
  // Do not cache this pointer and use it after DBusThreadManager is shut
  // down.
  virtual SpeechSynthesizerClient* GetSpeechSynthesizerClient() = 0;

  // Returns the cros-disks client, owned by DBusThreadManager.
  // Do not cache this pointer and use it after DBusThreadManager is shut
  // down.
  virtual CrosDisksClient* GetCrosDisksClient() = 0;

  // Returns the update engine client, owned by DBusThreadManager.  Do not
  // cache this pointer and use it after DBusThreadManager is shut down.
  virtual UpdateEngineClient* GetUpdateEngineClient() = 0;

  virtual ~DBusThreadManager();

 protected:
  DBusThreadManager();

  DISALLOW_COPY_AND_ASSIGN(DBusThreadManager);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DBUS_DBUS_THREAD_MANAGER_H_
