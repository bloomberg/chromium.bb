// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_DBUS_THREAD_MANAGER_H_
#define CHROMEOS_DBUS_FAKE_DBUS_THREAD_MANAGER_H_

#include "base/logging.h"
#include "base/observer_list.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/dbus_thread_manager.h"

namespace dbus {
class Bus;
class ObjectPath;
}  // namespace dbus

namespace chromeos {

class DBusThreadManagerObserver;

// This class provides a fake implementation of DBusThreadManager, which
// hosts fake D-Bus clients.
class CHROMEOS_EXPORT FakeDBusThreadManager : public DBusThreadManager {
 public:
  FakeDBusThreadManager();
  virtual ~FakeDBusThreadManager();

  // Creates and sets all fake DBusClients and the PowerPolicyController.
  void SetFakeClients();

  // Creates and sets all fake Shill DBusClients.
  void SetFakeShillClients();

  // Sets up any default environment for fake clients, e.g. for UI testing.
  void SetupDefaultEnvironment();

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
  void SetPowerPolicyController(scoped_ptr<PowerPolicyController> client);
  void SetSessionManagerClient(scoped_ptr<SessionManagerClient> client);
  void SetSMSClient(scoped_ptr<SMSClient> client);
  void SetSystemClockClient(scoped_ptr<SystemClockClient> client);
  void SetUpdateEngineClient(scoped_ptr<UpdateEngineClient> client);

  virtual void AddObserver(DBusThreadManagerObserver* observer) OVERRIDE;
  virtual void RemoveObserver(DBusThreadManagerObserver* observer) OVERRIDE;
  virtual dbus::Bus* GetSystemBus() OVERRIDE;

  virtual BluetoothAdapterClient* GetBluetoothAdapterClient() OVERRIDE;
  virtual BluetoothAgentManagerClient*
      GetBluetoothAgentManagerClient() OVERRIDE;
  virtual BluetoothDeviceClient* GetBluetoothDeviceClient() OVERRIDE;
  virtual BluetoothGattCharacteristicClient*
      GetBluetoothGattCharacteristicClient() OVERRIDE;
  virtual BluetoothGattDescriptorClient*
      GetBluetoothGattDescriptorClient() OVERRIDE;
  virtual BluetoothGattManagerClient* GetBluetoothGattManagerClient() OVERRIDE;
  virtual BluetoothGattServiceClient* GetBluetoothGattServiceClient() OVERRIDE;
  virtual BluetoothInputClient* GetBluetoothInputClient() OVERRIDE;
  virtual BluetoothProfileManagerClient*
      GetBluetoothProfileManagerClient() OVERRIDE;
  virtual CrasAudioClient* GetCrasAudioClient() OVERRIDE;
  virtual CrosDisksClient* GetCrosDisksClient() OVERRIDE;
  virtual CryptohomeClient* GetCryptohomeClient() OVERRIDE;
  virtual DebugDaemonClient* GetDebugDaemonClient() OVERRIDE;
  virtual EasyUnlockClient* GetEasyUnlockClient() OVERRIDE;
  virtual LorgnetteManagerClient* GetLorgnetteManagerClient() OVERRIDE;
  virtual ShillDeviceClient* GetShillDeviceClient() OVERRIDE;
  virtual ShillIPConfigClient* GetShillIPConfigClient() OVERRIDE;
  virtual ShillManagerClient* GetShillManagerClient() OVERRIDE;
  virtual ShillProfileClient* GetShillProfileClient() OVERRIDE;
  virtual ShillServiceClient* GetShillServiceClient() OVERRIDE;
  virtual GsmSMSClient* GetGsmSMSClient() OVERRIDE;
  virtual ImageBurnerClient* GetImageBurnerClient() OVERRIDE;
  virtual IntrospectableClient* GetIntrospectableClient() OVERRIDE;
  virtual ModemMessagingClient* GetModemMessagingClient() OVERRIDE;
  virtual NfcAdapterClient* GetNfcAdapterClient() OVERRIDE;
  virtual NfcDeviceClient* GetNfcDeviceClient() OVERRIDE;
  virtual NfcManagerClient* GetNfcManagerClient() OVERRIDE;
  virtual NfcRecordClient* GetNfcRecordClient() OVERRIDE;
  virtual NfcTagClient* GetNfcTagClient() OVERRIDE;
  virtual PermissionBrokerClient* GetPermissionBrokerClient() OVERRIDE;
  virtual PowerManagerClient* GetPowerManagerClient() OVERRIDE;
  virtual PowerPolicyController* GetPowerPolicyController() OVERRIDE;
  virtual SessionManagerClient* GetSessionManagerClient() OVERRIDE;
  virtual SMSClient* GetSMSClient() OVERRIDE;
  virtual SystemClockClient* GetSystemClockClient() OVERRIDE;
  virtual UpdateEngineClient* GetUpdateEngineClient() OVERRIDE;

 private:
  // Note: Keep this before other members so they can call AddObserver() in
  // their c'tors.
  ObserverList<DBusThreadManagerObserver> observers_;

  scoped_ptr<BluetoothAdapterClient> bluetooth_adapter_client_;
  scoped_ptr<BluetoothAgentManagerClient> bluetooth_agent_manager_client_;
  scoped_ptr<BluetoothDeviceClient> bluetooth_device_client_;
  scoped_ptr<BluetoothGattCharacteristicClient>
      bluetooth_gatt_characteristic_client_;
  scoped_ptr<BluetoothGattDescriptorClient>
      bluetooth_gatt_descriptor_client_;
  scoped_ptr<BluetoothGattManagerClient> bluetooth_gatt_manager_client_;
  scoped_ptr<BluetoothGattServiceClient> bluetooth_gatt_service_client_;
  scoped_ptr<BluetoothInputClient> bluetooth_input_client_;
  scoped_ptr<BluetoothProfileManagerClient> bluetooth_profile_manager_client_;
  scoped_ptr<CrasAudioClient> cras_audio_client_;
  scoped_ptr<CrosDisksClient> cros_disks_client_;
  scoped_ptr<CryptohomeClient> cryptohome_client_;
  scoped_ptr<DebugDaemonClient> debug_daemon_client_;
  scoped_ptr<EasyUnlockClient> easy_unlock_client_;
  scoped_ptr<LorgnetteManagerClient> lorgnette_manager_client_;
  scoped_ptr<ShillDeviceClient> shill_device_client_;
  scoped_ptr<ShillIPConfigClient> shill_ipconfig_client_;
  scoped_ptr<ShillManagerClient> shill_manager_client_;
  scoped_ptr<ShillServiceClient> shill_service_client_;
  scoped_ptr<ShillProfileClient> shill_profile_client_;
  scoped_ptr<GsmSMSClient> gsm_sms_client_;
  scoped_ptr<ImageBurnerClient> image_burner_client_;
  scoped_ptr<IntrospectableClient> introspectable_client_;
  scoped_ptr<ModemMessagingClient> modem_messaging_client_;
  scoped_ptr<NfcAdapterClient> nfc_adapter_client_;
  scoped_ptr<NfcDeviceClient> nfc_device_client_;
  scoped_ptr<NfcManagerClient> nfc_manager_client_;
  scoped_ptr<NfcRecordClient> nfc_record_client_;
  scoped_ptr<NfcTagClient> nfc_tag_client_;
  scoped_ptr<PermissionBrokerClient> permission_broker_client_;
  scoped_ptr<SystemClockClient> system_clock_client_;
  scoped_ptr<PowerManagerClient> power_manager_client_;
  scoped_ptr<SessionManagerClient> session_manager_client_;
  scoped_ptr<SMSClient> sms_client_;
  scoped_ptr<UpdateEngineClient> update_engine_client_;

  scoped_ptr<PowerPolicyController> power_policy_controller_;

  DISALLOW_COPY_AND_ASSIGN(FakeDBusThreadManager);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_DBUS_THREAD_MANAGER_H_
