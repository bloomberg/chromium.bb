// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_DBUS_THREAD_MANAGER_H_
#define CHROMEOS_DBUS_DBUS_THREAD_MANAGER_H_

#include <string>

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

// Style Note: Clients are sorted by names.
class BluetoothAdapterClient;
class BluetoothDeviceClient;
class BluetoothInputClient;
class BluetoothManagerClient;
class BluetoothNodeClient;
class BluetoothOutOfBandClient;
class CashewClient;
class CrosDisksClient;
class CryptohomeClient;
class DebugDaemonClient;
class ShillDeviceClient;
class ShillIPConfigClient;
class ShillManagerClient;
class ShillNetworkClient;
class ShillProfileClient;
class ShillServiceClient;
class GsmSMSClient;
class IBusClient;
class IBusEngineFactoryService;
class IBusEngineService;
class IBusInputContextClient;
class ImageBurnerClient;
class IntrospectableClient;
class MediaTransferProtocolDaemonClient;
class ModemMessagingClient;
class PermissionBrokerClient;
class PowerManagerClient;
class SMSClient;
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

  // Returns the bluetooth node client, owned by DBusThreadManager.
  // Do not cache this pointer and use it after DBusThreadManager is shut
  // down.
  virtual BluetoothOutOfBandClient* GetBluetoothOutOfBandClient() = 0;

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

  // Returns the Shill Device client, owned by DBusThreadManager.
  // Do not cache this pointer and use it after DBusThreadManager is shut
  // down.
  virtual ShillDeviceClient* GetShillDeviceClient() = 0;

  // Returns the Shill IPConfig client, owned by DBusThreadManager.
  // Do not cache this pointer and use it after DBusThreadManager is shut
  // down.
  virtual ShillIPConfigClient* GetShillIPConfigClient() = 0;

  // Returns the Shill Manager client, owned by DBusThreadManager.
  // Do not cache this pointer and use it after DBusThreadManager is shut
  // down.
  virtual ShillManagerClient* GetShillManagerClient() = 0;

  // Returns the Shill Network client, owned by DBusThreadManager.
  // Do not cache this pointer and use it after DBusThreadManager is shut
  // down.
  virtual ShillNetworkClient* GetShillNetworkClient() = 0;

  // Returns the Shill Profile client, owned by DBusThreadManager.
  // Do not cache this pointer and use it after DBusThreadManager is shut
  // down.
  virtual ShillProfileClient* GetShillProfileClient() = 0;

  // Returns the Shill Service client, owned by DBusThreadManager.
  // Do not cache this pointer and use it after DBusThreadManager is shut
  // down.
  virtual ShillServiceClient* GetShillServiceClient() = 0;

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

  // Returns the media transfer protocol client, owned by DBusThreadManager.
  // Do not cache this pointer and use it after DBusThreadManager is shut
  // down.
  virtual MediaTransferProtocolDaemonClient*
      GetMediaTransferProtocolDaemonClient() = 0;

  // Returns the Modem Messaging client, owned by DBusThreadManager.
  // Do not cache this pointer and use it after DBusThreadManager is shut
  // down.
  virtual ModemMessagingClient* GetModemMessagingClient() = 0;

  // Returns the Permission Broker client, owned by DBusThreadManager. Do not
  // cache this pointer and use it after DBusThreadManager is shut down.
  virtual PermissionBrokerClient* GetPermissionBrokerClient() = 0;

  // Returns the power manager client, owned by DBusThreadManager.
  // See also comments at session_manager_client().
  virtual PowerManagerClient* GetPowerManagerClient() = 0;

  // Returns the session manager client, owned by DBusThreadManager.
  // Do not cache this pointer and use it after DBusThreadManager is shut
  // down.
  virtual SessionManagerClient* GetSessionManagerClient() = 0;

  // Returns the SMS client, owned by DBusThreadManager.
  // Do not cache this pointer and use it after DBusThreadManager is shut
  // down.
  virtual SMSClient* GetSMSClient() = 0;

  // Returns the speech synthesizer client, owned by DBusThreadManager.
  // Do not cache this pointer and use it after DBusThreadManager is shut
  // down.
  virtual SpeechSynthesizerClient* GetSpeechSynthesizerClient() = 0;

  // Returns the update engine client, owned by DBusThreadManager.  Do not
  // cache this pointer and use it after DBusThreadManager is shut down.
  virtual UpdateEngineClient* GetUpdateEngineClient() = 0;

  // Returns the ibus client, owned by DBusThreadManager. Do not cache this
  // pointer and use it after DBusThreadManager is shut down.
  virtual IBusClient* GetIBusClient() = 0;

  // Returns the ibus input context client, owned by DBusThreadManager. Do not
  // cache this pointer and use it after DBusThreadManager is shut down.
  virtual IBusInputContextClient* GetIBusInputContextClient() = 0;

  // Returns the ibus engine factory service, owned by DBusThreadManager. Do not
  // cache this pointer and use it after DBusThreadManager is shut down.
  virtual IBusEngineFactoryService* GetIBusEngineFactoryService() = 0;

  // Returns the ibus engine service, owned by DBusThreadManager. Do not cache
  // this pointer and use it after DBusThreadManager is shut down.
  virtual IBusEngineService* GetIBusEngineService(
      const dbus::ObjectPath& object_path) = 0;

  // Removes the ibus engine services for |object_path|.
  virtual void RemoveIBusEngineService(const dbus::ObjectPath& object_path) = 0;

  virtual ~DBusThreadManager();

 protected:
  DBusThreadManager();

  DISALLOW_COPY_AND_ASSIGN(DBusThreadManager);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_DBUS_THREAD_MANAGER_H_
