// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_DBUS_THREAD_MANAGER_H_
#define CHROMEOS_DBUS_DBUS_THREAD_MANAGER_H_
#pragma once

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chromeos/chromeos_export.h"

namespace base {
class Thread;
};

namespace dbus {
class Bus;
};

namespace chromeos {

// Style Note: Clients are sorted by names.
class BluetoothAdapterClient;
class BluetoothDeviceClient;
class BluetoothInputClient;
class BluetoothManagerClient;
class BluetoothNodeClient;
class CashewClient;
class CrosDisksClient;
class CryptohomeClient;
class DebugDaemonClient;
class FlimflamDeviceClient;
class FlimflamIPConfigClient;
class FlimflamManagerClient;
class FlimflamNetworkClient;
class FlimflamProfileClient;
class FlimflamServiceClient;
class GsmSMSClient;
class ImageBurnerClient;
class IntrospectableClient;
class PowerManagerClient;
class SessionManagerClient;
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
class CHROMEOS_EXPORT DBusThreadManager {
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

  // Initialize with stub implementations for tests based on stubs.
  static void InitializeWithStub();

  // Destroys the global instance.
  static void Shutdown();

  // Gets the global instance. Initialize() must be called first.
  static DBusThreadManager* Get();

  // Creates new IBusBus instance to communicate with ibus-daemon with specified
  // ibus address. Must be called before using ibus related clients.
  // TODO(nona): Support shutdown to enable dynamical ibus-daemon shutdown.
  virtual void InitIBusBus(const std::string &ibus_address) = 0;

  // Returns the D-Bus system bus instance, owned by DBusThreadManager.
  virtual dbus::Bus* GetSystemBus() = 0;

  // Returns the IBus bus instance, owned by DBusThreadManager.
  virtual dbus::Bus* GetIBusBus() = 0;

  // Returns the bluetooth adapter client, owned by DBusThreadManager.
  // Do not cache this pointer and use it after DBusThreadManager is shut
  // down.
  virtual BluetoothAdapterClient* GetBluetoothAdapterClient() = 0;

  // Returns the bluetooth device client, owned by DBusThreadManager.
  // Do not cache this pointer and use it after DBusThreadManager is shut
  // down.
  virtual BluetoothDeviceClient* GetBluetoothDeviceClient() = 0;

  // Returns the bluetooth input client, owned by DBusThreadManager.
  // Do not cache this pointer and use it after DBusThreadManager is shut
  // down.
  virtual BluetoothInputClient* GetBluetoothInputClient() = 0;

  // Returns the bluetooth manager client, owned by DBusThreadManager.
  // Do not cache this pointer and use it after DBusThreadManager is shut
  // down.
  virtual BluetoothManagerClient* GetBluetoothManagerClient() = 0;

  // Returns the bluetooth node client, owned by DBusThreadManager.
  // Do not cache this pointer and use it after DBusThreadManager is shut
  // down.
  virtual BluetoothNodeClient* GetBluetoothNodeClient() = 0;

  // Returns the Cashew client, owned by DBusThreadManager.
  // Do not cache this pointer and use it after DBusThreadManager is shut
  // down.
  virtual CashewClient* GetCashewClient() = 0;

  // Returns the cros-disks client, owned by DBusThreadManager.
  // Do not cache this pointer and use it after DBusThreadManager is shut
  // down.
  virtual CrosDisksClient* GetCrosDisksClient() = 0;

  // Returns the Cryptohome client, owned by DBusThreadManager.
  // Do not cache this pointer and use it after DBusThreadManager is shut
  // down.
  virtual CryptohomeClient* GetCryptohomeClient() = 0;

  // Returns the DebugDaemon client, owned by DBusThreadManager.
  // Do not cache this pointer and use it after DBusThreadManager is shut
  // down.
  virtual DebugDaemonClient* GetDebugDaemonClient() = 0;

  // Returns the Flimflam Device client, owned by DBusThreadManager.
  // Do not cache this pointer and use it after DBusThreadManager is shut
  // down.
  virtual FlimflamDeviceClient* GetFlimflamDeviceClient() = 0;

  // Returns the Flimflam IPConfig client, owned by DBusThreadManager.
  // Do not cache this pointer and use it after DBusThreadManager is shut
  // down.
  virtual FlimflamIPConfigClient* GetFlimflamIPConfigClient() = 0;

  // Returns the Flimflam Manager client, owned by DBusThreadManager.
  // Do not cache this pointer and use it after DBusThreadManager is shut
  // down.
  virtual FlimflamManagerClient* GetFlimflamManagerClient() = 0;

  // Returns the Flimflam Network client, owned by DBusThreadManager.
  // Do not cache this pointer and use it after DBusThreadManager is shut
  // down.
  virtual FlimflamNetworkClient* GetFlimflamNetworkClient() = 0;

  // Returns the Flimflam Profile client, owned by DBusThreadManager.
  // Do not cache this pointer and use it after DBusThreadManager is shut
  // down.
  virtual FlimflamProfileClient* GetFlimflamProfileClient() = 0;

  // Returns the Flimflam Service client, owned by DBusThreadManager.
  // Do not cache this pointer and use it after DBusThreadManager is shut
  // down.
  virtual FlimflamServiceClient* GetFlimflamServiceClient() = 0;

  // Returns the SMS client, owned by DBusThreadManager.
  // Do not cache this pointer and use it after DBusThreadManager is shut
  // down.
  virtual GsmSMSClient* GetGsmSMSClient() = 0;

  // Returns the image burner client, owned by DBusThreadManager.
  // Do not cache this pointer and use it after DBusThreadManger is shut
  // down.
  virtual ImageBurnerClient* GetImageBurnerClient() = 0;

  // Returns the introspectable object client, owned by DBusThreadManager.
  // Do not cache this pointer and use it after DBusThreadManger is shut
  // down.
  virtual IntrospectableClient* GetIntrospectableClient() = 0;

  // Returns the power manager client, owned by DBusThreadManager.
  // See also comments at session_manager_client().
  virtual PowerManagerClient* GetPowerManagerClient() = 0;

  // Returns the session manager client, owned by DBusThreadManager.
  // Do not cache this pointer and use it after DBusThreadManager is shut
  // down.
  virtual SessionManagerClient* GetSessionManagerClient() = 0;

  // Returns the speech synthesizer client, owned by DBusThreadManager.
  // Do not cache this pointer and use it after DBusThreadManager is shut
  // down.
  virtual SpeechSynthesizerClient* GetSpeechSynthesizerClient() = 0;

  // Returns the update engine client, owned by DBusThreadManager.  Do not
  // cache this pointer and use it after DBusThreadManager is shut down.
  virtual UpdateEngineClient* GetUpdateEngineClient() = 0;

  virtual ~DBusThreadManager();

 protected:
  DBusThreadManager();

  DISALLOW_COPY_AND_ASSIGN(DBusThreadManager);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_DBUS_THREAD_MANAGER_H_
