// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_MOCK_DBUS_THREAD_MANAGER_H_
#define CHROMEOS_DBUS_MOCK_DBUS_THREAD_MANAGER_H_

#include <string>

#include "base/observer_list.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace dbus {

class Bus;

}  // namespace dbus

namespace chromeos {

class DBusThreadManagerObserver;
class MockBluetoothAdapterClient;
class MockBluetoothDeviceClient;
class MockBluetoothInputClient;
class MockBluetoothManagerClient;
class MockBluetoothNodeClient;
class MockBluetoothOutOfBandClient;
class MockCrosDisksClient;
class MockCryptohomeClient;
class MockDebugDaemonClient;
class MockExperimentalBluetoothAdapterClient;
class MockExperimentalBluetoothAgentManagerClient;
class MockExperimentalBluetoothDeviceClient;
class MockExperimentalBluetoothProfileManagerClient;
class MockShillDeviceClient;
class MockShillIPConfigClient;
class MockShillManagerClient;
class MockShillProfileClient;
class MockShillServiceClient;
class MockGsmSMSClient;
class MockImageBurnerClient;
class MockIntrospectableClient;
class MockModemMessagingClient;
class MockPermissionBrokerClient;
class MockPowerManagerClient;
class MockSessionManagerClient;
class MockSMSClient;
class MockSystemClockClient;
class MockUpdateEngineClient;
class PowerPolicyController;

// This class provides a mock DBusThreadManager with mock clients
// installed. You can customize the behaviors of mock clients with
// mock_foo_client() functions.
class MockDBusThreadManager : public DBusThreadManager {
 public:
  MockDBusThreadManager();
  virtual ~MockDBusThreadManager();

  void AddObserver(DBusThreadManagerObserver* observer) OVERRIDE;
  void RemoveObserver(DBusThreadManagerObserver* observer) OVERRIDE;
  MOCK_METHOD2(InitIBusBus, void(const std::string& ibus_address,
                                 const base::Closure& closure));
  MOCK_METHOD0(GetSystemBus, dbus::Bus*(void));
  MOCK_METHOD0(GetIBusBus, dbus::Bus*(void));
  MOCK_METHOD0(GetBluetoothAdapterClient, BluetoothAdapterClient*(void));
  MOCK_METHOD0(GetBluetoothDeviceClient, BluetoothDeviceClient*(void));
  MOCK_METHOD0(GetBluetoothInputClient, BluetoothInputClient*(void));
  MOCK_METHOD0(GetBluetoothManagerClient, BluetoothManagerClient*(void));
  MOCK_METHOD0(GetBluetoothNodeClient, BluetoothNodeClient*(void));
  MOCK_METHOD0(GetBluetoothOutOfBandClient, BluetoothOutOfBandClient*(void));
  MOCK_METHOD0(GetCrosDisksClient, CrosDisksClient*(void));
  MOCK_METHOD0(GetCryptohomeClient, CryptohomeClient*(void));
  MOCK_METHOD0(GetDebugDaemonClient, DebugDaemonClient*(void));
  MOCK_METHOD0(GetExperimentalBluetoothAdapterClient,
               ExperimentalBluetoothAdapterClient*(void));
  MOCK_METHOD0(GetExperimentalBluetoothAgentManagerClient,
               ExperimentalBluetoothAgentManagerClient*(void));
  MOCK_METHOD0(GetExperimentalBluetoothDeviceClient,
               ExperimentalBluetoothDeviceClient*(void));
  MOCK_METHOD0(GetExperimentalBluetoothProfileManagerClient,
               ExperimentalBluetoothProfileManagerClient*(void));
  MOCK_METHOD0(GetShillDeviceClient, ShillDeviceClient*(void));
  MOCK_METHOD0(GetShillIPConfigClient, ShillIPConfigClient*(void));
  MOCK_METHOD0(GetShillManagerClient, ShillManagerClient*(void));
  MOCK_METHOD0(GetShillProfileClient, ShillProfileClient*(void));
  MOCK_METHOD0(GetShillServiceClient, ShillServiceClient*(void));
  MOCK_METHOD0(GetGsmSMSClient, GsmSMSClient*(void));
  MOCK_METHOD0(GetImageBurnerClient, ImageBurnerClient*(void));
  MOCK_METHOD0(GetIntrospectableClient, IntrospectableClient*(void));
  MOCK_METHOD0(GetModemMessagingClient, ModemMessagingClient*(void));
  MOCK_METHOD0(GetPermissionBrokerClient, PermissionBrokerClient*(void));
  MOCK_METHOD0(GetPowerManagerClient, PowerManagerClient*(void));
  MOCK_METHOD0(GetPowerPolicyController, PowerPolicyController*(void));
  MOCK_METHOD0(GetSessionManagerClient, SessionManagerClient*(void));
  MOCK_METHOD0(GetSMSClient, SMSClient*(void));
  MOCK_METHOD0(GetSystemClockClient, SystemClockClient*(void));
  MOCK_METHOD0(GetUpdateEngineClient, UpdateEngineClient*(void));
  MOCK_METHOD0(GetIBusClient, IBusClient*(void));
  MOCK_METHOD0(GetIBusConfigClient, IBusConfigClient*(void));
  MOCK_METHOD0(GetIBusInputContextClient, IBusInputContextClient*(void));
  MOCK_METHOD0(GetIBusEngineFactoryService, IBusEngineFactoryService*(void));
  MOCK_METHOD1(GetIBusEngineService,
               IBusEngineService*(const dbus::ObjectPath& object_path));
  MOCK_METHOD1(RemoveIBusEngineService,
               void(const dbus::ObjectPath& object_path));
  MOCK_METHOD0(GetIBusPanelService, IBusPanelService*(void));

  MockBluetoothAdapterClient* mock_bluetooth_adapter_client() {
    return mock_bluetooth_adapter_client_.get();
  }
  MockBluetoothDeviceClient* mock_bluetooth_device_client() {
    return mock_bluetooth_device_client_.get();
  }
  MockBluetoothInputClient* mock_bluetooth_input_client() {
    return mock_bluetooth_input_client_.get();
  }
  MockBluetoothManagerClient* mock_bluetooth_manager_client() {
    return mock_bluetooth_manager_client_.get();
  }
  MockBluetoothNodeClient* mock_bluetooth_node_client() {
    return mock_bluetooth_node_client_.get();
  }
  MockBluetoothOutOfBandClient* mock_bluetooth_out_of_band_client() {
    return mock_bluetooth_out_of_band_client_.get();
  }
  MockCrosDisksClient* mock_cros_disks_client() {
    return mock_cros_disks_client_.get();
  }
  MockCryptohomeClient* mock_cryptohome_client() {
    return mock_cryptohome_client_.get();
  }
  MockDebugDaemonClient* mock_debugdaemon_client() {
    return mock_debugdaemon_client_.get();
  }
  MockExperimentalBluetoothAdapterClient*
        mock_experimental_bluetooth_adapter_client() {
    return mock_experimental_bluetooth_adapter_client_.get();
  }
  MockExperimentalBluetoothAgentManagerClient*
        mock_experimental_bluetooth_agent_manager_client() {
    return mock_experimental_bluetooth_agent_manager_client_.get();
  }
  MockExperimentalBluetoothDeviceClient*
        mock_experimental_bluetooth_device_client() {
    return mock_experimental_bluetooth_device_client_.get();
  }
  MockExperimentalBluetoothProfileManagerClient*
        mock_experimental_bluetooth_profile_manager_client() {
    return mock_experimental_bluetooth_profile_manager_client_.get();
  }
  MockShillDeviceClient* mock_shill_device_client() {
    return mock_shill_device_client_.get();
  }
  MockShillIPConfigClient* mock_shill_ipconfig_client() {
    return mock_shill_ipconfig_client_.get();
  }
  MockShillManagerClient* mock_shill_manager_client() {
    return mock_shill_manager_client_.get();
  }
  MockShillProfileClient* mock_shill_profile_client() {
    return mock_shill_profile_client_.get();
  }
  MockShillServiceClient* mock_shill_service_client() {
    return mock_shill_service_client_.get();
  }
  MockGsmSMSClient* mock_gsm_sms_client() {
    return mock_gsm_sms_client_.get();
  }
  MockImageBurnerClient* mock_image_burner_client() {
    return mock_image_burner_client_.get();
  }
  MockIntrospectableClient* mock_introspectable_client() {
    return mock_introspectable_client_.get();
  }
  MockModemMessagingClient* mock_modem_messaging_client() {
    return mock_modem_messaging_client_.get();
  }
  MockPermissionBrokerClient* mock_permission_broker_client() {
    return mock_permission_broker_client_.get();
  }
  MockPowerManagerClient* mock_power_manager_client() {
    return mock_power_manager_client_.get();
  }
  MockSessionManagerClient* mock_session_manager_client() {
    return mock_session_manager_client_.get();
  }
  MockSMSClient* mock_sms_client() {
    return mock_sms_client_.get();
  }
  MockSystemClockClient* mock_system_clock_client() {
    return mock_system_clock_client_.get();
  }
  MockUpdateEngineClient* mock_update_engine_client() {
    return mock_update_engine_client_.get();
  }

 private:
  // Note: Keep this before other members so they can call AddObserver() in
  // their c'tors.
  ObserverList<DBusThreadManagerObserver> observers_;

  scoped_ptr<MockBluetoothAdapterClient> mock_bluetooth_adapter_client_;
  scoped_ptr<MockBluetoothDeviceClient> mock_bluetooth_device_client_;
  scoped_ptr<MockBluetoothInputClient> mock_bluetooth_input_client_;
  scoped_ptr<MockBluetoothManagerClient> mock_bluetooth_manager_client_;
  scoped_ptr<MockBluetoothNodeClient> mock_bluetooth_node_client_;
  scoped_ptr<MockBluetoothOutOfBandClient> mock_bluetooth_out_of_band_client_;
  scoped_ptr<MockCrosDisksClient> mock_cros_disks_client_;
  scoped_ptr<MockCryptohomeClient> mock_cryptohome_client_;
  scoped_ptr<MockDebugDaemonClient> mock_debugdaemon_client_;
  scoped_ptr<MockExperimentalBluetoothAdapterClient>
      mock_experimental_bluetooth_adapter_client_;
  scoped_ptr<MockExperimentalBluetoothAgentManagerClient>
      mock_experimental_bluetooth_agent_manager_client_;
  scoped_ptr<MockExperimentalBluetoothDeviceClient>
      mock_experimental_bluetooth_device_client_;
  scoped_ptr<MockExperimentalBluetoothProfileManagerClient>
      mock_experimental_bluetooth_profile_manager_client_;
  scoped_ptr<MockShillDeviceClient> mock_shill_device_client_;
  scoped_ptr<MockShillIPConfigClient> mock_shill_ipconfig_client_;
  scoped_ptr<MockShillManagerClient> mock_shill_manager_client_;
  scoped_ptr<MockShillProfileClient> mock_shill_profile_client_;
  scoped_ptr<MockShillServiceClient> mock_shill_service_client_;
  scoped_ptr<MockGsmSMSClient> mock_gsm_sms_client_;
  scoped_ptr<MockImageBurnerClient> mock_image_burner_client_;
  scoped_ptr<MockIntrospectableClient> mock_introspectable_client_;
  scoped_ptr<MockModemMessagingClient> mock_modem_messaging_client_;
  scoped_ptr<MockPermissionBrokerClient> mock_permission_broker_client_;
  scoped_ptr<MockPowerManagerClient> mock_power_manager_client_;
  scoped_ptr<MockSessionManagerClient> mock_session_manager_client_;
  scoped_ptr<MockSMSClient> mock_sms_client_;
  scoped_ptr<MockSystemClockClient> mock_system_clock_client_;
  scoped_ptr<MockUpdateEngineClient> mock_update_engine_client_;
  scoped_ptr<PowerPolicyController> power_policy_controller_;

  DISALLOW_COPY_AND_ASSIGN(MockDBusThreadManager);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_MOCK_DBUS_THREAD_MANAGER_H_
