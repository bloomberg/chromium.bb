// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_DBUS_THREAD_MANAGER_H_
#define CHROMEOS_DBUS_DBUS_THREAD_MANAGER_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/dbus_client_bundle.h"

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
class DBusThreadManager;
class DBusThreadManagerSetter;
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
  // This will initialize real or stub DBusClients depending on command-line
  // arguments and whether this process runs in a ChromeOS environment.
  static void Initialize();

  // Returns a DBusThreadManagerSetter instance that allows tests to
  // replace individual D-Bus clients with their own implementations.
  // Also initializes the main DBusThreadManager for testing if necessary.
  static scoped_ptr<DBusThreadManagerSetter> GetSetterForTesting();

  // Returns true if DBusThreadManager has been initialized. Call this to
  // avoid initializing + shutting down DBusThreadManager more than once.
  static bool IsInitialized();

  // Destroys the global instance.
  static void Shutdown();

  // Gets the global instance. Initialize() must be called first.
  static DBusThreadManager* Get();

  // Returns true if |client| is stubbed.
  bool IsUsingStub(DBusClientBundle::DBusClientType client);

  // Returns various D-Bus bus instances, owned by DBusThreadManager.
  dbus::Bus* GetSystemBus();

  // All returned objects are owned by DBusThreadManager.  Do not cache these
  // pointers and use them after DBusThreadManager has been shut down.
  BluetoothAdapterClient* GetBluetoothAdapterClient();
  BluetoothAgentManagerClient* GetBluetoothAgentManagerClient();
  BluetoothDeviceClient* GetBluetoothDeviceClient();
  BluetoothGattCharacteristicClient* GetBluetoothGattCharacteristicClient();
  BluetoothGattDescriptorClient* GetBluetoothGattDescriptorClient();
  BluetoothGattManagerClient* GetBluetoothGattManagerClient();
  BluetoothGattServiceClient* GetBluetoothGattServiceClient();
  BluetoothInputClient* GetBluetoothInputClient();
  BluetoothProfileManagerClient* GetBluetoothProfileManagerClient();
  CrasAudioClient* GetCrasAudioClient();
  CrosDisksClient* GetCrosDisksClient();
  CryptohomeClient* GetCryptohomeClient();
  DebugDaemonClient* GetDebugDaemonClient();
  EasyUnlockClient* GetEasyUnlockClient();
  GsmSMSClient* GetGsmSMSClient();
  ImageBurnerClient* GetImageBurnerClient();
  IntrospectableClient* GetIntrospectableClient();
  LorgnetteManagerClient* GetLorgnetteManagerClient();
  ModemMessagingClient* GetModemMessagingClient();
  NfcAdapterClient* GetNfcAdapterClient();
  NfcDeviceClient* GetNfcDeviceClient();
  NfcManagerClient* GetNfcManagerClient();
  NfcRecordClient* GetNfcRecordClient();
  NfcTagClient* GetNfcTagClient();
  PermissionBrokerClient* GetPermissionBrokerClient();
  PowerManagerClient* GetPowerManagerClient();
  PowerPolicyController* GetPowerPolicyController();
  SessionManagerClient* GetSessionManagerClient();
  ShillDeviceClient* GetShillDeviceClient();
  ShillIPConfigClient* GetShillIPConfigClient();
  ShillManagerClient* GetShillManagerClient();
  ShillServiceClient* GetShillServiceClient();
  ShillProfileClient* GetShillProfileClient();
  SMSClient* GetSMSClient();
  SystemClockClient* GetSystemClockClient();
  UpdateEngineClient* GetUpdateEngineClient();

 private:
  friend class DBusThreadManagerSetter;

  // Creates a new DBusThreadManager using the DBusClients set in
  // |client_bundle|.
  explicit DBusThreadManager(scoped_ptr<DBusClientBundle> client_bundle);
  ~DBusThreadManager();

  // Creates a global instance of DBusThreadManager with the real
  // implementations for all clients that are listed in |unstub_client_mask| and
  // stub implementations for all clients that are not included. Cannot be
  // called more than once.
  static void CreateGlobalInstance(
      DBusClientBundle::DBusClientTypeMask unstub_client_mask);

  // Initialize global thread manager instance with all real dbus client
  // implementations.
  static void InitializeWithRealClients();

  // Initialize global thread manager instance with stubbed-out dbus clients
  // implementation.
  static void InitializeWithStubs();

  // Initialize with stub implementations for only certain clients that are
  // not included in the comma-separated |unstub_clients| list.
  static void InitializeWithPartialStub(const std::string& unstub_clients);

  // Initializes all currently stored DBusClients with the system bus and
  // performs additional setup.
  void InitializeClients();

  scoped_ptr<base::Thread> dbus_thread_;
  scoped_refptr<dbus::Bus> system_bus_;
  scoped_ptr<DBusClientBundle> client_bundle_;
  scoped_ptr<PowerPolicyController> power_policy_controller_;

  DISALLOW_COPY_AND_ASSIGN(DBusThreadManager);
};

class CHROMEOS_EXPORT DBusThreadManagerSetter {
 public:
  ~DBusThreadManagerSetter();

  void SetBluetoothAdapterClient(scoped_ptr<BluetoothAdapterClient> client);
  void SetBluetoothAgentManagerClient(
      scoped_ptr<BluetoothAgentManagerClient> client);
  void SetBluetoothDeviceClient(scoped_ptr<BluetoothDeviceClient> client);
  void SetBluetoothGattCharacteristicClient(
      scoped_ptr<BluetoothGattCharacteristicClient> client);
  void SetBluetoothGattDescriptorClient(
      scoped_ptr<BluetoothGattDescriptorClient> client);
  void SetBluetoothGattManagerClient(
      scoped_ptr<BluetoothGattManagerClient> client);
  void SetBluetoothGattServiceClient(
      scoped_ptr<BluetoothGattServiceClient> client);
  void SetBluetoothInputClient(scoped_ptr<BluetoothInputClient> client);
  void SetBluetoothProfileManagerClient(
      scoped_ptr<BluetoothProfileManagerClient> client);
  void SetCrasAudioClient(scoped_ptr<CrasAudioClient> client);
  void SetCrosDisksClient(scoped_ptr<CrosDisksClient> client);
  void SetCryptohomeClient(scoped_ptr<CryptohomeClient> client);
  void SetDebugDaemonClient(scoped_ptr<DebugDaemonClient> client);
  void SetEasyUnlockClient(scoped_ptr<EasyUnlockClient> client);
  void SetLorgnetteManagerClient(scoped_ptr<LorgnetteManagerClient> client);
  void SetShillDeviceClient(scoped_ptr<ShillDeviceClient> client);
  void SetShillIPConfigClient(scoped_ptr<ShillIPConfigClient> client);
  void SetShillManagerClient(scoped_ptr<ShillManagerClient> client);
  void SetShillServiceClient(scoped_ptr<ShillServiceClient> client);
  void SetShillProfileClient(scoped_ptr<ShillProfileClient> client);
  void SetGsmSMSClient(scoped_ptr<GsmSMSClient> client);
  void SetImageBurnerClient(scoped_ptr<ImageBurnerClient> client);
  void SetIntrospectableClient(scoped_ptr<IntrospectableClient> client);
  void SetModemMessagingClient(scoped_ptr<ModemMessagingClient> client);
  void SetNfcAdapterClient(scoped_ptr<NfcAdapterClient> client);
  void SetNfcDeviceClient(scoped_ptr<NfcDeviceClient> client);
  void SetNfcManagerClient(scoped_ptr<NfcManagerClient> client);
  void SetNfcRecordClient(scoped_ptr<NfcRecordClient> client);
  void SetNfcTagClient(scoped_ptr<NfcTagClient> client);
  void SetPermissionBrokerClient(scoped_ptr<PermissionBrokerClient> client);
  void SetPowerManagerClient(scoped_ptr<PowerManagerClient> client);
  void SetSessionManagerClient(scoped_ptr<SessionManagerClient> client);
  void SetSMSClient(scoped_ptr<SMSClient> client);
  void SetSystemClockClient(scoped_ptr<SystemClockClient> client);
  void SetUpdateEngineClient(scoped_ptr<UpdateEngineClient> client);

 private:
  friend class DBusThreadManager;

  DBusThreadManagerSetter();

  DISALLOW_COPY_AND_ASSIGN(DBusThreadManagerSetter);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_DBUS_THREAD_MANAGER_H_
