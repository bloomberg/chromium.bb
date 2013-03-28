// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_DBUS_THREAD_MANAGER_H_
#define CHROMEOS_DBUS_DBUS_THREAD_MANAGER_H_

#include <string>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chromeos/chromeos_export.h"

namespace base {
class Thread;
};

namespace dbus {
class Bus;
class ObjectPath;
};

namespace chromeos {

class DBusThreadManagerObserver;

// Style Note: Clients are sorted by names.
class BluetoothAdapterClient;
class BluetoothDeviceClient;
class BluetoothInputClient;
class BluetoothManagerClient;
class BluetoothNodeClient;
class BluetoothOutOfBandClient;
class CrasAudioClient;
class CrosDisksClient;
class CryptohomeClient;
class DebugDaemonClient;
class ExperimentalBluetoothAdapterClient;
class ExperimentalBluetoothAgentManagerClient;
class ExperimentalBluetoothDeviceClient;
class ExperimentalBluetoothProfileManagerClient;
class GsmSMSClient;
class IBusClient;
class IBusConfigClient;
class IBusEngineFactoryService;
class IBusEngineService;
class IBusInputContextClient;
class IBusPanelService;
class ImageBurnerClient;
class IntrospectableClient;
class ModemMessagingClient;
class PermissionBrokerClient;
class PowerManagerClient;
class PowerPolicyController;
class SMSClient;
class SessionManagerClient;
class ShillDeviceClient;
class ShillIPConfigClient;
class ShillManagerClient;
class ShillProfileClient;
class ShillServiceClient;
class SystemClockClient;
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

  // Returns true if DBusThreadManager has been initialized. Call this to
  // avoid initializing + shutting down DBusThreadManager more than once.
  static bool IsInitialized();

  // Destroys the global instance.
  static void Shutdown();

  // Gets the global instance. Initialize() must be called first.
  static DBusThreadManager* Get();

  // Adds or removes an observer.
  virtual void AddObserver(DBusThreadManagerObserver* observer) = 0;
  virtual void RemoveObserver(DBusThreadManagerObserver* observer) = 0;

  // Creates new IBusBus instance to communicate with ibus-daemon with specified
  // ibus address. |on_disconnected_callback| will be called when the connection
  // with ibus-daemon is disconnected. Must be called before using ibus related
  // clients.
  // TODO(nona): Support shutdown to enable dynamical ibus-daemon shutdown.
  virtual void InitIBusBus(const std::string& ibus_address,
                           const base::Closure& on_disconnected_callback) = 0;

  // Returns various D-Bus bus instances, owned by DBusThreadManager.
  virtual dbus::Bus* GetSystemBus() = 0;
  virtual dbus::Bus* GetIBusBus() = 0;

  // All returned objects are owned by DBusThreadManager.  Do not cache these
  // pointers and use them after DBusThreadManager has been shut down.
  virtual BluetoothAdapterClient* GetBluetoothAdapterClient() = 0;
  virtual BluetoothDeviceClient* GetBluetoothDeviceClient() = 0;
  virtual BluetoothInputClient* GetBluetoothInputClient() = 0;
  virtual BluetoothManagerClient* GetBluetoothManagerClient() = 0;
  virtual BluetoothNodeClient* GetBluetoothNodeClient() = 0;
  virtual BluetoothOutOfBandClient* GetBluetoothOutOfBandClient() = 0;
  virtual CrasAudioClient* GetCrasAudioClient() = 0;
  virtual CrosDisksClient* GetCrosDisksClient() = 0;
  virtual CryptohomeClient* GetCryptohomeClient() = 0;
  virtual DebugDaemonClient* GetDebugDaemonClient() = 0;
  virtual ExperimentalBluetoothAdapterClient*
      GetExperimentalBluetoothAdapterClient() = 0;
  virtual ExperimentalBluetoothAgentManagerClient*
      GetExperimentalBluetoothAgentManagerClient() = 0;
  virtual ExperimentalBluetoothDeviceClient*
      GetExperimentalBluetoothDeviceClient() = 0;
  virtual ExperimentalBluetoothProfileManagerClient*
      GetExperimentalBluetoothProfileManagerClient() = 0;
  virtual GsmSMSClient* GetGsmSMSClient() = 0;
  virtual IBusClient* GetIBusClient() = 0;
  virtual IBusConfigClient* GetIBusConfigClient() = 0;
  virtual IBusEngineFactoryService* GetIBusEngineFactoryService() = 0;
  virtual IBusEngineService* GetIBusEngineService(
      const dbus::ObjectPath& object_path) = 0;
  virtual IBusInputContextClient* GetIBusInputContextClient() = 0;
  virtual IBusPanelService* GetIBusPanelService() = 0;
  virtual ImageBurnerClient* GetImageBurnerClient() = 0;
  virtual IntrospectableClient* GetIntrospectableClient() = 0;
  virtual ModemMessagingClient* GetModemMessagingClient() = 0;
  virtual PermissionBrokerClient* GetPermissionBrokerClient() = 0;
  virtual PowerManagerClient* GetPowerManagerClient() = 0;
  virtual PowerPolicyController* GetPowerPolicyController() = 0;
  virtual SessionManagerClient* GetSessionManagerClient() = 0;
  virtual ShillDeviceClient* GetShillDeviceClient() = 0;
  virtual ShillIPConfigClient* GetShillIPConfigClient() = 0;
  virtual ShillManagerClient* GetShillManagerClient() = 0;
  virtual ShillProfileClient* GetShillProfileClient() = 0;
  virtual ShillServiceClient* GetShillServiceClient() = 0;
  virtual SMSClient* GetSMSClient() = 0;
  virtual SystemClockClient* GetSystemClockClient() = 0;
  virtual UpdateEngineClient* GetUpdateEngineClient() = 0;

  // Removes the ibus engine services for |object_path|.
  virtual void RemoveIBusEngineService(const dbus::ObjectPath& object_path) = 0;

  virtual ~DBusThreadManager();

 protected:
  DBusThreadManager();

  DISALLOW_COPY_AND_ASSIGN(DBusThreadManager);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_DBUS_THREAD_MANAGER_H_
