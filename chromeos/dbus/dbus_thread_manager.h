// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_DBUS_THREAD_MANAGER_H_
#define CHROMEOS_DBUS_DBUS_THREAD_MANAGER_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chromeos/chromeos_export.h"

namespace base {
class Thread;
}  // namespace base

namespace dbus {
class Bus;
class ObjectPath;
}  // namespace dbus

namespace chromeos {

// Style Note: Clients are sorted by names.
class BluetoothAdapterClient;
class BluetoothAgentManagerClient;
class BluetoothDeviceClient;
class BluetoothGattCharacteristicClient;
class BluetoothGattDescriptorClient;
class BluetoothGattManagerClient;
class BluetoothGattServiceClient;
class BluetoothInputClient;
class BluetoothProfileManagerClient;
class CrasAudioClient;
class CrosDisksClient;
class CryptohomeClient;
class DBusClient;
class DebugDaemonClient;
class EasyUnlockClient;
class GsmSMSClient;
class ImageBurnerClient;
class IntrospectableClient;
class LorgnetteManagerClient;
class ModemMessagingClient;
class NfcAdapterClient;
class NfcDeviceClient;
class NfcManagerClient;
class NfcRecordClient;
class NfcTagClient;
class PermissionBrokerClient;
class PowerManagerClient;
class PowerPolicyController;
class SessionManagerClient;
class ShillDeviceClient;
class ShillIPConfigClient;
class ShillManagerClient;
class ShillProfileClient;
class ShillServiceClient;
class SMSClient;
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

  // Sets an alternative DBusThreadManager such as MockDBusThreadManager
  // to be used in |Initialize()| for testing. Tests that call
  // DBusThreadManager::Initialize() (such as browser_tests and
  // interactive_ui_tests) should use this instead of calling
  // |InitiailzeForTesting|.  The injected object will be owned by the
  // internal pointer and deleted by Shutdown().
  static void SetInstanceForTesting(DBusThreadManager* dbus_thread_manager);

  // Similar to Initialize(), but injects an alternative
  // DBusThreadManager using SetInstanceForTest first.  The injected
  // object will be owned by the internal pointer and deleted by
  // Shutdown(). Does not create any Fake client implementations.
  static void InitializeForTesting(DBusThreadManager* dbus_thread_manager);

  // Initialize with stub implementations for tests, creating a complete set
  // of fake/stub client implementations. Also initializes a default set of
  // fake Shill devices and services, customizable with switches::kShillStub.
  static void InitializeWithStub();

  // Returns true if DBusThreadManager has been initialized. Call this to
  // avoid initializing + shutting down DBusThreadManager more than once.
  static bool IsInitialized();

  // Destroys the global instance.
  static void Shutdown();

  // Gets the global instance. Initialize() must be called first.
  static DBusThreadManager* Get();

  // Returns various D-Bus bus instances, owned by DBusThreadManager.
  virtual dbus::Bus* GetSystemBus() = 0;

  // All returned objects are owned by DBusThreadManager.  Do not cache these
  // pointers and use them after DBusThreadManager has been shut down.
  virtual BluetoothAdapterClient* GetBluetoothAdapterClient() = 0;
  virtual BluetoothAgentManagerClient* GetBluetoothAgentManagerClient() = 0;
  virtual BluetoothDeviceClient* GetBluetoothDeviceClient() = 0;
  virtual BluetoothGattCharacteristicClient*
      GetBluetoothGattCharacteristicClient() = 0;
  virtual BluetoothGattDescriptorClient* GetBluetoothGattDescriptorClient() = 0;
  virtual BluetoothGattManagerClient* GetBluetoothGattManagerClient() = 0;
  virtual BluetoothGattServiceClient* GetBluetoothGattServiceClient() = 0;
  virtual BluetoothInputClient* GetBluetoothInputClient() = 0;
  virtual BluetoothProfileManagerClient* GetBluetoothProfileManagerClient() = 0;
  virtual CrasAudioClient* GetCrasAudioClient() = 0;
  virtual CrosDisksClient* GetCrosDisksClient() = 0;
  virtual CryptohomeClient* GetCryptohomeClient() = 0;
  virtual DebugDaemonClient* GetDebugDaemonClient() = 0;
  virtual EasyUnlockClient* GetEasyUnlockClient() = 0;
  virtual GsmSMSClient* GetGsmSMSClient() = 0;
  virtual ImageBurnerClient* GetImageBurnerClient() = 0;
  virtual IntrospectableClient* GetIntrospectableClient() = 0;
  virtual LorgnetteManagerClient* GetLorgnetteManagerClient() = 0;
  virtual ModemMessagingClient* GetModemMessagingClient() = 0;
  virtual NfcAdapterClient* GetNfcAdapterClient() = 0;
  virtual NfcDeviceClient* GetNfcDeviceClient() = 0;
  virtual NfcManagerClient* GetNfcManagerClient() = 0;
  virtual NfcRecordClient* GetNfcRecordClient() = 0;
  virtual NfcTagClient* GetNfcTagClient() = 0;
  virtual PermissionBrokerClient* GetPermissionBrokerClient() = 0;
  virtual PowerManagerClient* GetPowerManagerClient() = 0;
  virtual PowerPolicyController* GetPowerPolicyController() = 0;
  virtual SessionManagerClient* GetSessionManagerClient() = 0;
  virtual ShillDeviceClient* GetShillDeviceClient() = 0;
  virtual ShillIPConfigClient* GetShillIPConfigClient() = 0;
  virtual ShillManagerClient* GetShillManagerClient() = 0;
  virtual ShillServiceClient* GetShillServiceClient() = 0;
  virtual ShillProfileClient* GetShillProfileClient() = 0;
  virtual SMSClient* GetSMSClient() = 0;
  virtual SystemClockClient* GetSystemClockClient() = 0;
  virtual UpdateEngineClient* GetUpdateEngineClient() = 0;

  virtual ~DBusThreadManager();

 protected:
  DBusThreadManager();

 private:
  // InitializeClients is called after g_dbus_thread_manager is set.
  // NOTE: Clients that access other clients in their Init() must be
  // initialized in the correct order.
  static void InitializeClients();

  // Initializes |client| with the |system_bus_|.
  static void InitClient(DBusClient* client);

  DISALLOW_COPY_AND_ASSIGN(DBusThreadManager);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_DBUS_THREAD_MANAGER_H_
