// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/mixed_dbus_thread_manager.h"

namespace chromeos {

MixedDBusThreadManager::MixedDBusThreadManager(
    DBusThreadManager* real_thread_manager,
    DBusThreadManager* fake_thread_manager)
    : real_thread_manager_(real_thread_manager),
      fake_thread_manager_(fake_thread_manager) {
}

MixedDBusThreadManager::~MixedDBusThreadManager() {
}

// DBusThreadManager overrides.
 dbus::Bus* MixedDBusThreadManager::GetSystemBus() {
  return real_thread_manager_->GetSystemBus();
}

 BluetoothAdapterClient* MixedDBusThreadManager::GetBluetoothAdapterClient() {
  return GetThreadManager(
      DBusClientBundle::BLUETOOTH)->GetBluetoothAdapterClient();
}

 BluetoothAgentManagerClient*
 MixedDBusThreadManager::GetBluetoothAgentManagerClient() {
  return GetThreadManager(
      DBusClientBundle::BLUETOOTH)->GetBluetoothAgentManagerClient();
}

 BluetoothDeviceClient* MixedDBusThreadManager::GetBluetoothDeviceClient() {
  return GetThreadManager(
      DBusClientBundle::BLUETOOTH)->GetBluetoothDeviceClient();
}

 BluetoothGattCharacteristicClient*
 MixedDBusThreadManager::GetBluetoothGattCharacteristicClient() {
  return GetThreadManager(DBusClientBundle::BLUETOOTH)->
      GetBluetoothGattCharacteristicClient();
}

 BluetoothGattDescriptorClient*
 MixedDBusThreadManager::GetBluetoothGattDescriptorClient() {
  return GetThreadManager(DBusClientBundle::BLUETOOTH)->
      GetBluetoothGattDescriptorClient();
}

 BluetoothGattManagerClient*
 MixedDBusThreadManager::GetBluetoothGattManagerClient() {
  return GetThreadManager(
      DBusClientBundle::BLUETOOTH)->GetBluetoothGattManagerClient();
}

 BluetoothGattServiceClient*
 MixedDBusThreadManager::GetBluetoothGattServiceClient() {
  return GetThreadManager(
      DBusClientBundle::BLUETOOTH)->GetBluetoothGattServiceClient();
}

 BluetoothInputClient* MixedDBusThreadManager::GetBluetoothInputClient() {
  return GetThreadManager(
      DBusClientBundle::BLUETOOTH)->GetBluetoothInputClient();
}

 BluetoothProfileManagerClient*
 MixedDBusThreadManager::GetBluetoothProfileManagerClient() {
  return GetThreadManager(
      DBusClientBundle::BLUETOOTH)->GetBluetoothProfileManagerClient();
}

 CrasAudioClient* MixedDBusThreadManager::GetCrasAudioClient() {
  return GetThreadManager(
      DBusClientBundle::CRAS)->GetCrasAudioClient();
}

 CrosDisksClient* MixedDBusThreadManager::GetCrosDisksClient() {
  return GetThreadManager(DBusClientBundle::CROS_DISKS)->GetCrosDisksClient();
}

 CryptohomeClient* MixedDBusThreadManager::GetCryptohomeClient() {
  return GetThreadManager(
      DBusClientBundle::CRYPTOHOME)->GetCryptohomeClient();
}

 DebugDaemonClient* MixedDBusThreadManager::GetDebugDaemonClient() {
  return GetThreadManager(
      DBusClientBundle::DEBUG_DAEMON)->GetDebugDaemonClient();
}

 EasyUnlockClient* MixedDBusThreadManager::GetEasyUnlockClient() {
  return GetThreadManager(
      DBusClientBundle::EASY_UNLOCK)->GetEasyUnlockClient();
}

 LorgnetteManagerClient* MixedDBusThreadManager::GetLorgnetteManagerClient() {
  return GetThreadManager(
      DBusClientBundle::LORGNETTE_MANAGER)->GetLorgnetteManagerClient();
}

 ShillDeviceClient* MixedDBusThreadManager::GetShillDeviceClient() {
  return GetThreadManager(DBusClientBundle::SHILL)->GetShillDeviceClient();
}

ShillIPConfigClient* MixedDBusThreadManager::GetShillIPConfigClient() {
  return GetThreadManager(DBusClientBundle::SHILL)->GetShillIPConfigClient();
}

ShillManagerClient* MixedDBusThreadManager::GetShillManagerClient() {
  return GetThreadManager(DBusClientBundle::SHILL)->GetShillManagerClient();
}

ShillProfileClient* MixedDBusThreadManager::GetShillProfileClient() {
  return GetThreadManager(DBusClientBundle::SHILL)->GetShillProfileClient();
}

ShillServiceClient* MixedDBusThreadManager::GetShillServiceClient() {
  return GetThreadManager(DBusClientBundle::SHILL)->GetShillServiceClient();
}

GsmSMSClient* MixedDBusThreadManager::GetGsmSMSClient() {
  return GetThreadManager(DBusClientBundle::GSM_SMS)->GetGsmSMSClient();
}

ImageBurnerClient* MixedDBusThreadManager::GetImageBurnerClient() {
  return GetThreadManager(
      DBusClientBundle::IMAGE_BURNER)->GetImageBurnerClient();
}

IntrospectableClient* MixedDBusThreadManager::GetIntrospectableClient() {
  return GetThreadManager(
      DBusClientBundle::INTROSPECTABLE)->GetIntrospectableClient();
}

ModemMessagingClient* MixedDBusThreadManager::GetModemMessagingClient() {
  return GetThreadManager(
      DBusClientBundle::MODEM_MESSAGING)->GetModemMessagingClient();
}

NfcAdapterClient* MixedDBusThreadManager::GetNfcAdapterClient() {
  return GetThreadManager(DBusClientBundle::NFC)->GetNfcAdapterClient();
}

NfcDeviceClient* MixedDBusThreadManager::GetNfcDeviceClient() {
  return GetThreadManager(DBusClientBundle::NFC)->GetNfcDeviceClient();
}

NfcManagerClient* MixedDBusThreadManager::GetNfcManagerClient() {
  return GetThreadManager(DBusClientBundle::NFC)->GetNfcManagerClient();
}

NfcRecordClient* MixedDBusThreadManager::GetNfcRecordClient() {
  return GetThreadManager(DBusClientBundle::NFC)->GetNfcRecordClient();
}

NfcTagClient* MixedDBusThreadManager::GetNfcTagClient() {
  return GetThreadManager(DBusClientBundle::NFC)->GetNfcTagClient();
}

PermissionBrokerClient* MixedDBusThreadManager::GetPermissionBrokerClient() {
  return GetThreadManager(
      DBusClientBundle::PERMISSION_BROKER)->GetPermissionBrokerClient();
}

PowerManagerClient* MixedDBusThreadManager::GetPowerManagerClient() {
  return GetThreadManager(
      DBusClientBundle::POWER_MANAGER)->GetPowerManagerClient();
}

PowerPolicyController* MixedDBusThreadManager::GetPowerPolicyController() {
  return GetThreadManager(
      DBusClientBundle::POWER_MANAGER)->GetPowerPolicyController();
}

SessionManagerClient* MixedDBusThreadManager::GetSessionManagerClient() {
  return GetThreadManager(
      DBusClientBundle::SESSION_MANAGER)->GetSessionManagerClient();
}

SMSClient* MixedDBusThreadManager::GetSMSClient() {
  return GetThreadManager(DBusClientBundle::SMS)->GetSMSClient();
}

SystemClockClient* MixedDBusThreadManager::GetSystemClockClient() {
  return GetThreadManager(
      DBusClientBundle::SYSTEM_CLOCK)->GetSystemClockClient();
}

UpdateEngineClient* MixedDBusThreadManager::GetUpdateEngineClient() {
  return GetThreadManager(
      DBusClientBundle::UPDATE_ENGINE)->GetUpdateEngineClient();
}

DBusThreadManager* MixedDBusThreadManager::GetThreadManager(
    DBusClientBundle::DBusClientType client) {
  if (DBusThreadManager::IsUsingStub(client))
    return fake_thread_manager_.get();

  return real_thread_manager_.get();
}

}  // namespace chromeos
