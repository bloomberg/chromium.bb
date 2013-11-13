// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_dbus_thread_manager.h"

#include "chromeos/dbus/bluetooth_adapter_client.h"
#include "chromeos/dbus/bluetooth_agent_manager_client.h"
#include "chromeos/dbus/bluetooth_device_client.h"
#include "chromeos/dbus/bluetooth_input_client.h"
#include "chromeos/dbus/bluetooth_profile_manager_client.h"
#include "chromeos/dbus/cras_audio_client.h"
#include "chromeos/dbus/cros_disks_client.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/dbus_thread_manager_observer.h"
#include "chromeos/dbus/debug_daemon_client.h"
#include "chromeos/dbus/fake_nfc_adapter_client.h"
#include "chromeos/dbus/fake_nfc_device_client.h"
#include "chromeos/dbus/fake_nfc_manager_client.h"
#include "chromeos/dbus/fake_nfc_record_client.h"
#include "chromeos/dbus/fake_nfc_tag_client.h"
#include "chromeos/dbus/gsm_sms_client.h"
#include "chromeos/dbus/image_burner_client.h"
#include "chromeos/dbus/introspectable_client.h"
#include "chromeos/dbus/modem_messaging_client.h"
#include "chromeos/dbus/permission_broker_client.h"
#include "chromeos/dbus/power_manager_client.h"
#include "chromeos/dbus/power_policy_controller.h"
#include "chromeos/dbus/session_manager_client.h"
#include "chromeos/dbus/shill_device_client.h"
#include "chromeos/dbus/shill_ipconfig_client.h"
#include "chromeos/dbus/shill_manager_client.h"
#include "chromeos/dbus/shill_profile_client.h"
#include "chromeos/dbus/shill_service_client.h"
#include "chromeos/dbus/sms_client.h"
#include "chromeos/dbus/system_clock_client.h"
#include "chromeos/dbus/update_engine_client.h"

namespace chromeos {

FakeDBusThreadManager::FakeDBusThreadManager() {
}

FakeDBusThreadManager::~FakeDBusThreadManager() {
  FOR_EACH_OBSERVER(DBusThreadManagerObserver, observers_,
                    OnDBusThreadManagerDestroying(this));
}

void FakeDBusThreadManager::SetFakeClients() {
  const DBusClientImplementationType client_type =
      STUB_DBUS_CLIENT_IMPLEMENTATION;
  SetBluetoothAdapterClient(scoped_ptr<BluetoothAdapterClient>(
      BluetoothAdapterClient::Create(client_type)));
  SetBluetoothAgentManagerClient(scoped_ptr<BluetoothAgentManagerClient>(
      BluetoothAgentManagerClient::Create(client_type)));
  SetBluetoothDeviceClient(scoped_ptr<BluetoothDeviceClient>(
      BluetoothDeviceClient::Create(client_type)));
  SetBluetoothInputClient(scoped_ptr<BluetoothInputClient>(
      BluetoothInputClient::Create(client_type)));
  SetBluetoothProfileManagerClient(scoped_ptr<BluetoothProfileManagerClient>(
      BluetoothProfileManagerClient::Create(client_type)));
  SetCrasAudioClient(
      scoped_ptr<CrasAudioClient>(CrasAudioClient::Create(client_type)));
  SetCrosDisksClient(
      scoped_ptr<CrosDisksClient>(CrosDisksClient::Create(client_type)));
  SetCryptohomeClient(
      scoped_ptr<CryptohomeClient>(CryptohomeClient::Create(client_type)));
  SetDebugDaemonClient(
      scoped_ptr<DebugDaemonClient>(DebugDaemonClient::Create(client_type)));
  SetShillManagerClient(
      scoped_ptr<ShillManagerClient>(ShillManagerClient::Create(client_type)));
  SetShillDeviceClient(
      scoped_ptr<ShillDeviceClient>(ShillDeviceClient::Create(client_type)));
  SetShillIPConfigClient(scoped_ptr<ShillIPConfigClient>(
      ShillIPConfigClient::Create(client_type)));
  SetShillServiceClient(
      scoped_ptr<ShillServiceClient>(ShillServiceClient::Create(client_type)));
  SetShillProfileClient(
      scoped_ptr<ShillProfileClient>(ShillProfileClient::Create(client_type)));
  SetGsmSMSClient(scoped_ptr<GsmSMSClient>(GsmSMSClient::Create(client_type)));
  SetImageBurnerClient(
      scoped_ptr<ImageBurnerClient>(ImageBurnerClient::Create(client_type)));
  SetIntrospectableClient(scoped_ptr<IntrospectableClient>(
      IntrospectableClient::Create(client_type)));
  SetModemMessagingClient(scoped_ptr<ModemMessagingClient>(
      ModemMessagingClient::Create(client_type)));
  SetNfcAdapterClient(scoped_ptr<NfcAdapterClient>(new FakeNfcAdapterClient));
  SetNfcDeviceClient(scoped_ptr<NfcDeviceClient>(new FakeNfcDeviceClient));
  SetNfcManagerClient(scoped_ptr<NfcManagerClient>(new FakeNfcManagerClient));
  SetNfcRecordClient(scoped_ptr<NfcRecordClient>(new FakeNfcRecordClient));
  SetNfcTagClient(scoped_ptr<NfcTagClient>(new FakeNfcTagClient));
  SetPermissionBrokerClient(scoped_ptr<PermissionBrokerClient>(
      PermissionBrokerClient::Create(client_type)));
  SetPowerManagerClient(
      scoped_ptr<PowerManagerClient>(PowerManagerClient::Create(client_type)));
  SetSessionManagerClient(scoped_ptr<SessionManagerClient>(
      SessionManagerClient::Create(client_type)));
  SetSMSClient(scoped_ptr<SMSClient>(SMSClient::Create(client_type)));
  SetSystemClockClient(
      scoped_ptr<SystemClockClient>(SystemClockClient::Create(client_type)));
  SetUpdateEngineClient(
      scoped_ptr<UpdateEngineClient>(UpdateEngineClient::Create(client_type)));

  SetPowerPolicyController(make_scoped_ptr(new PowerPolicyController));
}

void FakeDBusThreadManager::SetBluetoothAdapterClient(
    scoped_ptr<BluetoothAdapterClient> client) {
  bluetooth_adapter_client_ = client.Pass();
}

void FakeDBusThreadManager::SetBluetoothAgentManagerClient(
    scoped_ptr<BluetoothAgentManagerClient> client) {
  bluetooth_agent_manager_client_ = client.Pass();
}

void FakeDBusThreadManager::SetBluetoothDeviceClient(
    scoped_ptr<BluetoothDeviceClient> client) {
  bluetooth_device_client_ = client.Pass();
}

void FakeDBusThreadManager::SetBluetoothInputClient(
    scoped_ptr<BluetoothInputClient> client) {
  bluetooth_input_client_ = client.Pass();
}

void FakeDBusThreadManager::SetBluetoothProfileManagerClient(
    scoped_ptr<BluetoothProfileManagerClient> client) {
  bluetooth_profile_manager_client_ = client.Pass();
}

void FakeDBusThreadManager::SetCrasAudioClient(
    scoped_ptr<CrasAudioClient> client) {
  cras_audio_client_ = client.Pass();
}

void FakeDBusThreadManager::SetCrosDisksClient(
    scoped_ptr<CrosDisksClient> client) {
  cros_disks_client_ = client.Pass();
}

void FakeDBusThreadManager::SetCryptohomeClient(
    scoped_ptr<CryptohomeClient> client) {
  cryptohome_client_ = client.Pass();
}

void FakeDBusThreadManager::SetDebugDaemonClient(
    scoped_ptr<DebugDaemonClient> client) {
  debug_daemon_client_ = client.Pass();
}

void FakeDBusThreadManager::SetShillDeviceClient(
    scoped_ptr<ShillDeviceClient> client) {
  shill_device_client_ = client.Pass();
}

void FakeDBusThreadManager::SetShillIPConfigClient(
    scoped_ptr<ShillIPConfigClient> client) {
  shill_ipconfig_client_ = client.Pass();
}

void FakeDBusThreadManager::SetShillManagerClient(
    scoped_ptr<ShillManagerClient> client) {
  shill_manager_client_ = client.Pass();
}

void FakeDBusThreadManager::SetShillServiceClient(
    scoped_ptr<ShillServiceClient> client) {
  shill_service_client_ = client.Pass();
}

void FakeDBusThreadManager::SetShillProfileClient(
    scoped_ptr<ShillProfileClient> client) {
  shill_profile_client_ = client.Pass();
}

void FakeDBusThreadManager::SetGsmSMSClient(
    scoped_ptr<GsmSMSClient> client) {
  gsm_sms_client_ = client.Pass();
}

void FakeDBusThreadManager::SetImageBurnerClient(
    scoped_ptr<ImageBurnerClient> client) {
  image_burner_client_ = client.Pass();
}

void FakeDBusThreadManager::SetIntrospectableClient(
    scoped_ptr<IntrospectableClient> client) {
  introspectable_client_ = client.Pass();
}

void FakeDBusThreadManager::SetModemMessagingClient(
    scoped_ptr<ModemMessagingClient> client) {
  modem_messaging_client_ = client.Pass();
}

void FakeDBusThreadManager::SetNfcAdapterClient(
    scoped_ptr<NfcAdapterClient> client) {
  nfc_adapter_client_ = client.Pass();
}

void FakeDBusThreadManager::SetNfcDeviceClient(
    scoped_ptr<NfcDeviceClient> client) {
  nfc_device_client_ = client.Pass();
}

void FakeDBusThreadManager::SetNfcManagerClient(
    scoped_ptr<NfcManagerClient> client) {
  nfc_manager_client_ = client.Pass();
}

void FakeDBusThreadManager::SetNfcRecordClient(
    scoped_ptr<NfcRecordClient> client) {
  nfc_record_client_ = client.Pass();
}

void FakeDBusThreadManager::SetNfcTagClient(
    scoped_ptr<NfcTagClient> client) {
  nfc_tag_client_ = client.Pass();
}

void FakeDBusThreadManager::SetPermissionBrokerClient(
    scoped_ptr<PermissionBrokerClient> client) {
  permission_broker_client_ = client.Pass();
}

void FakeDBusThreadManager::SetPowerManagerClient(
    scoped_ptr<PowerManagerClient> client) {
  power_manager_client_ = client.Pass();
}

void FakeDBusThreadManager::SetPowerPolicyController(
    scoped_ptr<PowerPolicyController> client) {
  power_policy_controller_ = client.Pass();
}

void FakeDBusThreadManager::SetSessionManagerClient(
    scoped_ptr<SessionManagerClient> client) {
  session_manager_client_ = client.Pass();
}

void FakeDBusThreadManager::SetSMSClient(scoped_ptr<SMSClient> client) {
  sms_client_ = client.Pass();
}

void FakeDBusThreadManager::SetSystemClockClient(
    scoped_ptr<SystemClockClient> client) {
  system_clock_client_ = client.Pass();
}

void FakeDBusThreadManager::SetUpdateEngineClient(
    scoped_ptr<UpdateEngineClient> client) {
  update_engine_client_ = client.Pass();
}

void FakeDBusThreadManager::AddObserver(
    DBusThreadManagerObserver* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void FakeDBusThreadManager::RemoveObserver(
    DBusThreadManagerObserver* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

dbus::Bus* FakeDBusThreadManager::GetSystemBus() {
  return NULL;
}

BluetoothAdapterClient*
    FakeDBusThreadManager::GetBluetoothAdapterClient() {
  return bluetooth_adapter_client_.get();
}

BluetoothAgentManagerClient*
    FakeDBusThreadManager::GetBluetoothAgentManagerClient() {
  return bluetooth_agent_manager_client_.get();
}

BluetoothDeviceClient*
    FakeDBusThreadManager::GetBluetoothDeviceClient() {
  return bluetooth_device_client_.get();
}

BluetoothInputClient*
    FakeDBusThreadManager::GetBluetoothInputClient() {
  return bluetooth_input_client_.get();
}

BluetoothProfileManagerClient*
    FakeDBusThreadManager::GetBluetoothProfileManagerClient() {
  return bluetooth_profile_manager_client_.get();
}

CrasAudioClient* FakeDBusThreadManager::GetCrasAudioClient() {
  return cras_audio_client_.get();
}

CrosDisksClient* FakeDBusThreadManager::GetCrosDisksClient() {
  return cros_disks_client_.get();
}

CryptohomeClient* FakeDBusThreadManager::GetCryptohomeClient() {
  return cryptohome_client_.get();
}

DebugDaemonClient* FakeDBusThreadManager::GetDebugDaemonClient() {
  return debug_daemon_client_.get();
}

ShillDeviceClient*
    FakeDBusThreadManager::GetShillDeviceClient() {
  return shill_device_client_.get();
}

ShillIPConfigClient*
    FakeDBusThreadManager::GetShillIPConfigClient() {
  return shill_ipconfig_client_.get();
}

ShillManagerClient*
    FakeDBusThreadManager::GetShillManagerClient() {
  return shill_manager_client_.get();
}

ShillProfileClient*
    FakeDBusThreadManager::GetShillProfileClient() {
  return shill_profile_client_.get();
}

ShillServiceClient*
    FakeDBusThreadManager::GetShillServiceClient() {
  return shill_service_client_.get();
}

GsmSMSClient* FakeDBusThreadManager::GetGsmSMSClient() {
  return gsm_sms_client_.get();
}

ImageBurnerClient* FakeDBusThreadManager::GetImageBurnerClient() {
  return image_burner_client_.get();
}

IntrospectableClient*
    FakeDBusThreadManager::GetIntrospectableClient() {
  return introspectable_client_.get();
}

ModemMessagingClient*
    FakeDBusThreadManager::GetModemMessagingClient() {
  return modem_messaging_client_.get();
}

NfcAdapterClient* FakeDBusThreadManager::GetNfcAdapterClient() {
  return nfc_adapter_client_.get();
}

NfcDeviceClient* FakeDBusThreadManager::GetNfcDeviceClient() {
  return nfc_device_client_.get();
}

NfcManagerClient* FakeDBusThreadManager::GetNfcManagerClient() {
  return nfc_manager_client_.get();
}

NfcTagClient* FakeDBusThreadManager::GetNfcTagClient() {
  return nfc_tag_client_.get();
}

NfcRecordClient* FakeDBusThreadManager::GetNfcRecordClient() {
  return nfc_record_client_.get();
}

PermissionBrokerClient*
    FakeDBusThreadManager::GetPermissionBrokerClient() {
  return permission_broker_client_.get();
}

PowerManagerClient* FakeDBusThreadManager::GetPowerManagerClient() {
  return power_manager_client_.get();
}

PowerPolicyController*
FakeDBusThreadManager::GetPowerPolicyController() {
  return power_policy_controller_.get();
}

SessionManagerClient*
    FakeDBusThreadManager::GetSessionManagerClient() {
  return session_manager_client_.get();
}

SMSClient* FakeDBusThreadManager::GetSMSClient() {
  return sms_client_.get();
}

SystemClockClient* FakeDBusThreadManager::GetSystemClockClient() {
  return system_clock_client_.get();
}

UpdateEngineClient* FakeDBusThreadManager::GetUpdateEngineClient() {
  return update_engine_client_.get();
}

}  // namespace chromeos
