// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_dbus_thread_manager.h"

#include "chromeos/dbus/dbus_thread_manager_observer.h"
#include "chromeos/dbus/fake_bluetooth_adapter_client.h"
#include "chromeos/dbus/fake_bluetooth_agent_manager_client.h"
#include "chromeos/dbus/fake_bluetooth_device_client.h"
#include "chromeos/dbus/fake_bluetooth_input_client.h"
#include "chromeos/dbus/fake_bluetooth_profile_manager_client.h"
#include "chromeos/dbus/fake_cros_disks_client.h"
#include "chromeos/dbus/fake_cryptohome_client.h"
#include "chromeos/dbus/fake_gsm_sms_client.h"
#include "chromeos/dbus/fake_image_burner_client.h"
#include "chromeos/dbus/fake_nfc_adapter_client.h"
#include "chromeos/dbus/fake_nfc_device_client.h"
#include "chromeos/dbus/fake_nfc_manager_client.h"
#include "chromeos/dbus/fake_nfc_tag_client.h"
#include "chromeos/dbus/fake_power_manager_client.h"
#include "chromeos/dbus/fake_session_manager_client.h"
#include "chromeos/dbus/fake_shill_device_client.h"
#include "chromeos/dbus/fake_shill_manager_client.h"
#include "chromeos/dbus/fake_system_clock_client.h"
#include "chromeos/dbus/fake_update_engine_client.h"
#include "chromeos/dbus/ibus/mock_ibus_client.h"
#include "chromeos/dbus/power_policy_controller.h"

namespace chromeos {

FakeDBusThreadManager::FakeDBusThreadManager()
  : fake_bluetooth_adapter_client_(new FakeBluetoothAdapterClient()),
    fake_bluetooth_agent_manager_client_(new FakeBluetoothAgentManagerClient()),
    fake_bluetooth_device_client_(new FakeBluetoothDeviceClient()),
    fake_bluetooth_input_client_(new FakeBluetoothInputClient()),
    fake_bluetooth_profile_manager_client_(
        new FakeBluetoothProfileManagerClient()),
    fake_cros_disks_client_(new FakeCrosDisksClient),
    fake_cryptohome_client_(new FakeCryptohomeClient),
    fake_gsm_sms_client_(new FakeGsmSMSClient),
    fake_image_burner_client_(new FakeImageBurnerClient),
    fake_nfc_adapter_client_(new FakeNfcAdapterClient()),
    fake_nfc_device_client_(new FakeNfcDeviceClient()),
    fake_nfc_manager_client_(new FakeNfcManagerClient()),
    fake_nfc_tag_client_(new FakeNfcTagClient()),
    fake_session_manager_client_(new FakeSessionManagerClient),
    fake_shill_device_client_(new FakeShillDeviceClient),
    fake_shill_manager_client_(new FakeShillManagerClient),
    fake_system_clock_client_(new FakeSystemClockClient),
    fake_power_manager_client_(new FakePowerManagerClient),
    fake_update_engine_client_(new FakeUpdateEngineClient),
    ibus_bus_(NULL) {
  power_policy_controller_.reset(
      new PowerPolicyController(this, fake_power_manager_client_.get()));
}

FakeDBusThreadManager::~FakeDBusThreadManager() {
  FOR_EACH_OBSERVER(DBusThreadManagerObserver, observers_,
                    OnDBusThreadManagerDestroying(this));
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

void FakeDBusThreadManager::InitIBusBus(
    const std::string& ibus_address,
    const base::Closure& closure) {
  // Non-null bus address is used to ensure the connection to ibus-daemon.
  ibus_bus_ = reinterpret_cast<dbus::Bus*>(0xdeadbeef);
  mock_ibus_client_.reset(new MockIBusClient);
}

dbus::Bus* FakeDBusThreadManager::GetSystemBus() {
  return NULL;
}

BluetoothAdapterClient*
    FakeDBusThreadManager::GetBluetoothAdapterClient() {
  return fake_bluetooth_adapter_client_.get();
}

BluetoothAgentManagerClient*
    FakeDBusThreadManager::GetBluetoothAgentManagerClient() {
  return fake_bluetooth_agent_manager_client_.get();
}

BluetoothDeviceClient*
    FakeDBusThreadManager::GetBluetoothDeviceClient() {
  return fake_bluetooth_device_client_.get();
}

BluetoothInputClient*
    FakeDBusThreadManager::GetBluetoothInputClient() {
  return fake_bluetooth_input_client_.get();
}

BluetoothProfileManagerClient*
    FakeDBusThreadManager::GetBluetoothProfileManagerClient() {
  return fake_bluetooth_profile_manager_client_.get();
}

CrasAudioClient* FakeDBusThreadManager::GetCrasAudioClient() {
  return NULL;
}

CrosDisksClient* FakeDBusThreadManager::GetCrosDisksClient() {
  return fake_cros_disks_client_.get();
}

CryptohomeClient* FakeDBusThreadManager::GetCryptohomeClient() {
  return fake_cryptohome_client_.get();
}

DebugDaemonClient* FakeDBusThreadManager::GetDebugDaemonClient() {
  NOTIMPLEMENTED();
  return NULL;
}

ShillDeviceClient*
    FakeDBusThreadManager::GetShillDeviceClient() {
  return fake_shill_device_client_.get();
}

ShillIPConfigClient*
    FakeDBusThreadManager::GetShillIPConfigClient() {
  NOTIMPLEMENTED();
  return NULL;
}

ShillManagerClient*
    FakeDBusThreadManager::GetShillManagerClient() {
  return fake_shill_manager_client_.get();
}

ShillProfileClient*
    FakeDBusThreadManager::GetShillProfileClient() {
  NOTIMPLEMENTED();
  return NULL;
}

ShillServiceClient*
    FakeDBusThreadManager::GetShillServiceClient() {
  NOTIMPLEMENTED();
  return NULL;
}

GsmSMSClient* FakeDBusThreadManager::GetGsmSMSClient() {
  return fake_gsm_sms_client_.get();
}

ImageBurnerClient* FakeDBusThreadManager::GetImageBurnerClient() {
  return fake_image_burner_client_.get();
}

IntrospectableClient*
    FakeDBusThreadManager::GetIntrospectableClient() {
  NOTIMPLEMENTED();
  return NULL;
}

ModemMessagingClient*
    FakeDBusThreadManager::GetModemMessagingClient() {
  NOTIMPLEMENTED();
  return NULL;
}

NfcAdapterClient* FakeDBusThreadManager::GetNfcAdapterClient() {
  return fake_nfc_adapter_client_.get();
}

NfcDeviceClient* FakeDBusThreadManager::GetNfcDeviceClient() {
  return fake_nfc_device_client_.get();
}

NfcManagerClient* FakeDBusThreadManager::GetNfcManagerClient() {
  return fake_nfc_manager_client_.get();
}

NfcTagClient* FakeDBusThreadManager::GetNfcTagClient() {
  return fake_nfc_tag_client_.get();
}

PermissionBrokerClient*
    FakeDBusThreadManager::GetPermissionBrokerClient() {
  NOTIMPLEMENTED();
  return NULL;
}

PowerManagerClient* FakeDBusThreadManager::GetPowerManagerClient() {
  return fake_power_manager_client_.get();
}

PowerPolicyController*
FakeDBusThreadManager::GetPowerPolicyController() {
  return power_policy_controller_.get();
}

SessionManagerClient*
    FakeDBusThreadManager::GetSessionManagerClient() {
  return fake_session_manager_client_.get();
}

SMSClient* FakeDBusThreadManager::GetSMSClient() {
  NOTIMPLEMENTED();
  return NULL;
}

SystemClockClient* FakeDBusThreadManager::GetSystemClockClient() {
  return fake_system_clock_client_.get();
}

UpdateEngineClient* FakeDBusThreadManager::GetUpdateEngineClient() {
  return fake_update_engine_client_.get();
}

IBusClient* FakeDBusThreadManager::GetIBusClient() {
  return mock_ibus_client_.get();
}

}  // namespace chromeos
