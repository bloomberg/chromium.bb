// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/mock_dbus_thread_manager_without_gmock.h"

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
#include "chromeos/dbus/fake_power_manager_client.h"
#include "chromeos/dbus/fake_session_manager_client.h"
#include "chromeos/dbus/fake_shill_device_client.h"
#include "chromeos/dbus/fake_shill_manager_client.h"
#include "chromeos/dbus/fake_system_clock_client.h"
#include "chromeos/dbus/fake_update_engine_client.h"
#include "chromeos/dbus/ibus/mock_ibus_client.h"
#include "chromeos/dbus/ibus/mock_ibus_config_client.h"
#include "chromeos/dbus/ibus/mock_ibus_engine_factory_service.h"
#include "chromeos/dbus/ibus/mock_ibus_engine_service.h"
#include "chromeos/dbus/ibus/mock_ibus_input_context_client.h"
#include "chromeos/dbus/ibus/mock_ibus_panel_service.h"
#include "chromeos/dbus/power_policy_controller.h"

namespace chromeos {

MockDBusThreadManagerWithoutGMock::MockDBusThreadManagerWithoutGMock()
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

MockDBusThreadManagerWithoutGMock::~MockDBusThreadManagerWithoutGMock() {
  FOR_EACH_OBSERVER(DBusThreadManagerObserver, observers_,
                    OnDBusThreadManagerDestroying(this));
}

void MockDBusThreadManagerWithoutGMock::AddObserver(
    DBusThreadManagerObserver* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void MockDBusThreadManagerWithoutGMock::RemoveObserver(
    DBusThreadManagerObserver* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

void MockDBusThreadManagerWithoutGMock::InitIBusBus(
    const std::string& ibus_address,
    const base::Closure& closure) {
  // Non-null bus address is used to ensure the connection to ibus-daemon.
  ibus_bus_ = reinterpret_cast<dbus::Bus*>(0xdeadbeef);
  mock_ibus_client_.reset(new MockIBusClient);
  mock_ibus_config_client_.reset(new MockIBusConfigClient);
  mock_ibus_input_context_client_.reset(new MockIBusInputContextClient);
  mock_ibus_engine_service_.reset(new MockIBusEngineService);
  mock_ibus_engine_factory_service_.reset(new MockIBusEngineFactoryService);
  mock_ibus_panel_service_.reset(new MockIBusPanelService);
}

dbus::Bus* MockDBusThreadManagerWithoutGMock::GetSystemBus() {
  return NULL;
}

dbus::Bus* MockDBusThreadManagerWithoutGMock::GetIBusBus() {
  return ibus_bus_;
}

BluetoothAdapterClient*
    MockDBusThreadManagerWithoutGMock::GetBluetoothAdapterClient() {
  return fake_bluetooth_adapter_client_.get();
}

BluetoothAgentManagerClient*
    MockDBusThreadManagerWithoutGMock::GetBluetoothAgentManagerClient() {
  return fake_bluetooth_agent_manager_client_.get();
}

BluetoothDeviceClient*
    MockDBusThreadManagerWithoutGMock::GetBluetoothDeviceClient() {
  return fake_bluetooth_device_client_.get();
}

BluetoothInputClient*
    MockDBusThreadManagerWithoutGMock::GetBluetoothInputClient() {
  return fake_bluetooth_input_client_.get();
}

BluetoothProfileManagerClient*
    MockDBusThreadManagerWithoutGMock::GetBluetoothProfileManagerClient() {
  return fake_bluetooth_profile_manager_client_.get();
}

CrasAudioClient* MockDBusThreadManagerWithoutGMock::GetCrasAudioClient() {
  return NULL;
}

CrosDisksClient* MockDBusThreadManagerWithoutGMock::GetCrosDisksClient() {
  return fake_cros_disks_client_.get();
}

CryptohomeClient* MockDBusThreadManagerWithoutGMock::GetCryptohomeClient() {
  return fake_cryptohome_client_.get();
}

DebugDaemonClient* MockDBusThreadManagerWithoutGMock::GetDebugDaemonClient() {
  NOTIMPLEMENTED();
  return NULL;
}

ShillDeviceClient*
    MockDBusThreadManagerWithoutGMock::GetShillDeviceClient() {
  return fake_shill_device_client_.get();
}

ShillIPConfigClient*
    MockDBusThreadManagerWithoutGMock::GetShillIPConfigClient() {
  NOTIMPLEMENTED();
  return NULL;
}

ShillManagerClient*
    MockDBusThreadManagerWithoutGMock::GetShillManagerClient() {
  return fake_shill_manager_client_.get();
}

ShillProfileClient*
    MockDBusThreadManagerWithoutGMock::GetShillProfileClient() {
  NOTIMPLEMENTED();
  return NULL;
}

ShillServiceClient*
    MockDBusThreadManagerWithoutGMock::GetShillServiceClient() {
  NOTIMPLEMENTED();
  return NULL;
}

GsmSMSClient* MockDBusThreadManagerWithoutGMock::GetGsmSMSClient() {
  return fake_gsm_sms_client_.get();
}

ImageBurnerClient* MockDBusThreadManagerWithoutGMock::GetImageBurnerClient() {
  return fake_image_burner_client_.get();
}

IntrospectableClient*
    MockDBusThreadManagerWithoutGMock::GetIntrospectableClient() {
  NOTIMPLEMENTED();
  return NULL;
}

ModemMessagingClient*
    MockDBusThreadManagerWithoutGMock::GetModemMessagingClient() {
  NOTIMPLEMENTED();
  return NULL;
}

PermissionBrokerClient*
    MockDBusThreadManagerWithoutGMock::GetPermissionBrokerClient() {
  NOTIMPLEMENTED();
  return NULL;
}

PowerManagerClient* MockDBusThreadManagerWithoutGMock::GetPowerManagerClient() {
  return fake_power_manager_client_.get();
}

PowerPolicyController*
MockDBusThreadManagerWithoutGMock::GetPowerPolicyController() {
  return power_policy_controller_.get();
}

SessionManagerClient*
    MockDBusThreadManagerWithoutGMock::GetSessionManagerClient() {
  return fake_session_manager_client_.get();
}

SMSClient* MockDBusThreadManagerWithoutGMock::GetSMSClient() {
  NOTIMPLEMENTED();
  return NULL;
}

SystemClockClient* MockDBusThreadManagerWithoutGMock::GetSystemClockClient() {
  return fake_system_clock_client_.get();
}

UpdateEngineClient* MockDBusThreadManagerWithoutGMock::GetUpdateEngineClient() {
  return fake_update_engine_client_.get();
}

IBusClient* MockDBusThreadManagerWithoutGMock::GetIBusClient() {
  return mock_ibus_client_.get();
}

IBusConfigClient* MockDBusThreadManagerWithoutGMock::GetIBusConfigClient() {
  return mock_ibus_config_client_.get();
}

IBusInputContextClient*
    MockDBusThreadManagerWithoutGMock::GetIBusInputContextClient() {
  return mock_ibus_input_context_client_.get();
}

IBusEngineFactoryService*
    MockDBusThreadManagerWithoutGMock::GetIBusEngineFactoryService() {
  return mock_ibus_engine_factory_service_.get();
}

IBusEngineService* MockDBusThreadManagerWithoutGMock::GetIBusEngineService(
    const dbus::ObjectPath& object_path) {
  return mock_ibus_engine_service_.get();
}

void MockDBusThreadManagerWithoutGMock::RemoveIBusEngineService(
    const dbus::ObjectPath& object_path) {
}

IBusPanelService* MockDBusThreadManagerWithoutGMock::GetIBusPanelService() {
  return mock_ibus_panel_service_.get();
}

}  // namespace chromeos
