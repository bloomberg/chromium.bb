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
class CrosDBusService;
class PowerManagerClient;
class SessionManagerClient;
class SensorsClient;
class SpeechSynthesizerClient;

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

  // Returns the bluetooth manager client, owned by DBusThreadManager.
  // Do not cache this pointer and use it after DBusThreadManager is shut
  // down.
  BluetoothManagerClient* bluetooth_manager_client() {
    return bluetooth_manager_client_.get();
  }

  // Returns the bluetooth adapter client, owned by DBusThreadManager.
  // Do not cache this pointer and use it after DBusThreadManager is shut
  // down.
  BluetoothAdapterClient* bluetooth_adapter_client() {
    return bluetooth_adapter_client_.get();
  }

  // Returns the power manager client, owned by DBusThreadManager.
  // See also comments at session_manager_client().
  PowerManagerClient* power_manager_client() {
    return power_manager_client_.get();
  }

  // Returns the session manager client, owned by DBusThreadManager.
  // Do not cache this pointer and use it after DBusThreadManager is shut
  // down.
  SessionManagerClient* session_manager_client() {
    return session_manager_client_.get();
  }

  // Sets the session manager client. Takes the ownership.
  // The function is exported for testing.
  void set_session_manager_client_for_testing(
      SessionManagerClient* session_manager_client);

  // Returns the speech synthesizer client, owned by DBusThreadManager.
  // Do not cache this pointer and use it after DBusThreadManager is shut
  // down.
  SpeechSynthesizerClient* speech_synthesizer_client() {
    return speech_synthesizer_client_.get();
  }

 private:
  DBusThreadManager();
  virtual ~DBusThreadManager();

  scoped_ptr<base::Thread> dbus_thread_;
  scoped_refptr<dbus::Bus> system_bus_;
  CrosDBusService* cros_dbus_service_;
  scoped_ptr<SensorsClient> sensors_client_;
  scoped_ptr<BluetoothManagerClient> bluetooth_manager_client_;
  scoped_ptr<BluetoothAdapterClient> bluetooth_adapter_client_;
  scoped_ptr<PowerManagerClient> power_manager_client_;
  scoped_ptr<SessionManagerClient> session_manager_client_;
  scoped_ptr<SpeechSynthesizerClient> speech_synthesizer_client_;

  DISALLOW_COPY_AND_ASSIGN(DBusThreadManager);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DBUS_DBUS_THREAD_MANAGER_H_
