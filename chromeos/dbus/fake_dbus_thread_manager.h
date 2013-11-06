// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_DBUS_THREAD_MANAGER_H_
#define CHROMEOS_DBUS_FAKE_DBUS_THREAD_MANAGER_H_

#include <string>

#include "base/logging.h"
#include "base/observer_list.h"
#include "chromeos/dbus/dbus_thread_manager.h"

namespace dbus {
class Bus;
class ObjectPath;
}  // namespace dbus

namespace chromeos {

class DBusThreadManagerObserver;
class FakeBluetoothAdapterClient;
class FakeBluetoothAgentManagerClient;
class FakeBluetoothDeviceClient;
class FakeBluetoothInputClient;
class FakeBluetoothProfileManagerClient;
class FakeCrosDisksClient;
class FakeCryptohomeClient;
class FakeGsmSMSClient;
class FakeNfcAdapterClient;
class FakeNfcDeviceClient;
class FakeNfcManagerClient;
class FakeNfcTagClient;
class FakePowerManagerClient;
class FakeImageBurnerClient;
class FakeSessionManagerClient;
class FakeShillDeviceClient;
class FakeShillManagerClient;
class FakeSystemClockClient;
class FakeUpdateEngineClient;
class MockIBusClient;

// This class provides a fake implementation of DBusThreadManager, which
// hosts fake D-Bus clients.
class FakeDBusThreadManager : public DBusThreadManager {
 public:
  FakeDBusThreadManager();
  virtual ~FakeDBusThreadManager();

  virtual void AddObserver(DBusThreadManagerObserver* observer) OVERRIDE;
  virtual void RemoveObserver(DBusThreadManagerObserver* observer) OVERRIDE;
  virtual void InitIBusBus(const std::string& ibus_address,
                           const base::Closure& closure) OVERRIDE;
  virtual dbus::Bus* GetSystemBus() OVERRIDE;

  virtual BluetoothAdapterClient* GetBluetoothAdapterClient() OVERRIDE;
  virtual BluetoothAgentManagerClient*
      GetBluetoothAgentManagerClient() OVERRIDE;
  virtual BluetoothDeviceClient* GetBluetoothDeviceClient() OVERRIDE;
  virtual BluetoothInputClient* GetBluetoothInputClient() OVERRIDE;
  virtual BluetoothProfileManagerClient*
      GetBluetoothProfileManagerClient() OVERRIDE;
  virtual CrasAudioClient* GetCrasAudioClient() OVERRIDE;
  virtual CrosDisksClient* GetCrosDisksClient() OVERRIDE;
  virtual CryptohomeClient* GetCryptohomeClient() OVERRIDE;
  virtual DebugDaemonClient* GetDebugDaemonClient() OVERRIDE;
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
  virtual NfcTagClient* GetNfcTagClient() OVERRIDE;
  virtual PermissionBrokerClient* GetPermissionBrokerClient() OVERRIDE;
  virtual PowerManagerClient* GetPowerManagerClient() OVERRIDE;
  virtual PowerPolicyController* GetPowerPolicyController() OVERRIDE;
  virtual SessionManagerClient* GetSessionManagerClient() OVERRIDE;
  virtual SMSClient* GetSMSClient() OVERRIDE;
  virtual SystemClockClient* GetSystemClockClient() OVERRIDE;
  virtual UpdateEngineClient* GetUpdateEngineClient() OVERRIDE;
  virtual IBusClient* GetIBusClient() OVERRIDE;

  FakeBluetoothAdapterClient* fake_bluetooth_adapter_client() {
    return fake_bluetooth_adapter_client_.get();
  }

  FakeBluetoothAgentManagerClient* fake_bluetooth_agent_manager_client() {
    return fake_bluetooth_agent_manager_client_.get();
  }

  FakeBluetoothDeviceClient* fake_bluetooth_device_client() {
    return fake_bluetooth_device_client_.get();
  }

  FakeBluetoothInputClient* fake_bluetooth_input_client() {
    return fake_bluetooth_input_client_.get();
  }

  FakeBluetoothProfileManagerClient* fake_bluetooth_profile_manager_client() {
    return fake_bluetooth_profile_manager_client_.get();
  }

  FakeCrosDisksClient* fake_cros_disks_client() {
    return fake_cros_disks_client_.get();
  }

  FakeCryptohomeClient* fake_cryptohome_client() {
    return fake_cryptohome_client_.get();
  }

  FakeGsmSMSClient* fake_gsm_sms_client() {
    return fake_gsm_sms_client_.get();
  }

  FakeImageBurnerClient* fake_image_burner_client() {
    return fake_image_burner_client_.get();
  }

  FakeNfcAdapterClient* fake_nfc_adapter_client() {
    return fake_nfc_adapter_client_.get();
  }

  FakeNfcDeviceClient* fake_nfc_device_client() {
    return fake_nfc_device_client_.get();
  }

  FakeNfcManagerClient* fake_nfc_manager_client() {
    return fake_nfc_manager_client_.get();
  }

  FakeNfcTagClient* fake_nfc_tag_client() {
    return fake_nfc_tag_client_.get();
  }

  FakeSessionManagerClient* fake_session_manager_client() {
    return fake_session_manager_client_.get();
  }

  FakeShillDeviceClient* fake_shill_device_client() {
    return fake_shill_device_client_.get();
  }

  FakeShillManagerClient* fake_shill_manager_client() {
    return fake_shill_manager_client_.get();
  }

  FakeSystemClockClient* fake_system_clock_client() {
    return fake_system_clock_client_.get();
  }

  FakePowerManagerClient* fake_power_manager_client() {
    return fake_power_manager_client_.get();
  }

  FakeUpdateEngineClient* fake_update_engine_client() {
    return fake_update_engine_client_.get();
  }

  // TODO(komatsu): Remove IBus related code. crbug.com/275262
  MockIBusClient* mock_ibus_client() {
    return mock_ibus_client_.get();
  }

  void set_ibus_bus(dbus::Bus* ibus_bus) {
    ibus_bus_ = ibus_bus;
  }

 private:
  // Note: Keep this before other members so they can call AddObserver() in
  // their c'tors.
  ObserverList<DBusThreadManagerObserver> observers_;

  scoped_ptr<FakeBluetoothAdapterClient> fake_bluetooth_adapter_client_;
  scoped_ptr<FakeBluetoothAgentManagerClient>
      fake_bluetooth_agent_manager_client_;
  scoped_ptr<FakeBluetoothDeviceClient> fake_bluetooth_device_client_;
  scoped_ptr<FakeBluetoothInputClient> fake_bluetooth_input_client_;
  scoped_ptr<FakeBluetoothProfileManagerClient>
      fake_bluetooth_profile_manager_client_;
  scoped_ptr<FakeCrosDisksClient> fake_cros_disks_client_;
  scoped_ptr<FakeCryptohomeClient> fake_cryptohome_client_;
  scoped_ptr<FakeGsmSMSClient> fake_gsm_sms_client_;
  scoped_ptr<FakeImageBurnerClient> fake_image_burner_client_;
  scoped_ptr<FakeNfcAdapterClient> fake_nfc_adapter_client_;
  scoped_ptr<FakeNfcDeviceClient> fake_nfc_device_client_;
  scoped_ptr<FakeNfcManagerClient> fake_nfc_manager_client_;
  scoped_ptr<FakeNfcTagClient> fake_nfc_tag_client_;
  scoped_ptr<FakeSessionManagerClient> fake_session_manager_client_;
  scoped_ptr<FakeShillDeviceClient> fake_shill_device_client_;
  scoped_ptr<FakeShillManagerClient> fake_shill_manager_client_;
  scoped_ptr<FakeSystemClockClient> fake_system_clock_client_;
  scoped_ptr<FakePowerManagerClient> fake_power_manager_client_;
  scoped_ptr<FakeUpdateEngineClient> fake_update_engine_client_;

  scoped_ptr<MockIBusClient> mock_ibus_client_;

  scoped_ptr<PowerPolicyController> power_policy_controller_;
  dbus::Bus* ibus_bus_;

  DISALLOW_COPY_AND_ASSIGN(FakeDBusThreadManager);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_DBUS_THREAD_MANAGER_H_
