// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_MOCK_DBUS_THREAD_MANAGER_WITHOUT_GMOCK_H_
#define CHROMEOS_DBUS_MOCK_DBUS_THREAD_MANAGER_WITHOUT_GMOCK_H_

#include <string>

#include "base/logging.h"
#include "chromeos/dbus/dbus_thread_manager.h"

namespace dbus {
class Bus;
class ObjectPath;
}  // namespace dbus

namespace chromeos {

class MockIBusClient;
class MockIBusEngineFactoryService;
class MockIBusEngineService;
class MockIBusInputContextClient;

// This class provides an another mock DBusThreadManager without gmock
// dependency. This class is used only for places where GMock is not allowed
// (ex. ui/).
class MockDBusThreadManagerWithoutGMock : public DBusThreadManager {
 public:
  MockDBusThreadManagerWithoutGMock();
  virtual ~MockDBusThreadManagerWithoutGMock();

  virtual void InitIBusBus(const std::string& ibus_address) OVERRIDE;
  virtual dbus::Bus* GetSystemBus() OVERRIDE;
  virtual dbus::Bus* GetIBusBus() OVERRIDE;

  virtual BluetoothAdapterClient* GetBluetoothAdapterClient() OVERRIDE;
  virtual BluetoothDeviceClient* GetBluetoothDeviceClient() OVERRIDE;
  virtual BluetoothInputClient* GetBluetoothInputClient() OVERRIDE;
  virtual BluetoothManagerClient* GetBluetoothManagerClient() OVERRIDE;
  virtual BluetoothNodeClient* GetBluetoothNodeClient() OVERRIDE;
  virtual CashewClient* GetCashewClient() OVERRIDE;
  virtual CrosDisksClient* GetCrosDisksClient() OVERRIDE;
  virtual CryptohomeClient* GetCryptohomeClient() OVERRIDE;
  virtual DebugDaemonClient* GetDebugDaemonClient() OVERRIDE;
  virtual ShillDeviceClient* GetShillDeviceClient() OVERRIDE;
  virtual ShillIPConfigClient* GetShillIPConfigClient() OVERRIDE;
  virtual ShillManagerClient* GetShillManagerClient() OVERRIDE;
  virtual ShillNetworkClient* GetShillNetworkClient() OVERRIDE;
  virtual ShillProfileClient* GetShillProfileClient() OVERRIDE;
  virtual ShillServiceClient* GetShillServiceClient() OVERRIDE;
  virtual GsmSMSClient* GetGsmSMSClient() OVERRIDE;
  virtual ImageBurnerClient* GetImageBurnerClient() OVERRIDE;
  virtual IntrospectableClient* GetIntrospectableClient() OVERRIDE;
  virtual MediaTransferProtocolDaemonClient*
  GetMediaTransferProtocolDaemonClient() OVERRIDE;
  virtual ModemMessagingClient* GetModemMessagingClient() OVERRIDE;
  virtual PermissionBrokerClient* GetPermissionBrokerClient() OVERRIDE;
  virtual PowerManagerClient* GetPowerManagerClient() OVERRIDE;
  virtual SessionManagerClient* GetSessionManagerClient() OVERRIDE;
  virtual SMSClient* GetSMSClient() OVERRIDE;
  virtual SpeechSynthesizerClient* GetSpeechSynthesizerClient() OVERRIDE;
  virtual UpdateEngineClient* GetUpdateEngineClient() OVERRIDE;
  virtual BluetoothOutOfBandClient* GetBluetoothOutOfBandClient() OVERRIDE;
  virtual IBusClient* GetIBusClient() OVERRIDE;
  virtual IBusInputContextClient* GetIBusInputContextClient() OVERRIDE;
  virtual IBusEngineFactoryService* GetIBusEngineFactoryService() OVERRIDE;
  virtual IBusEngineService* GetIBusEngineService(
      const dbus::ObjectPath& object_path) OVERRIDE;
  virtual void RemoveIBusEngineService(
      const dbus::ObjectPath& object_path) OVERRIDE;

  MockIBusClient* mock_ibus_client() {
    return mock_ibus_client_.get();
  }

  MockIBusInputContextClient* mock_ibus_input_context_client() {
    return mock_ibus_input_context_client_.get();
  }

  MockIBusEngineService* mock_ibus_engine_service() {
    return mock_ibus_engine_service_.get();
  }

  MockIBusEngineFactoryService* mock_ibus_engine_factory_service() {
    return mock_ibus_engine_factory_service_.get();
  }

  void set_ibus_bus(dbus::Bus* ibus_bus) {
    ibus_bus_ = ibus_bus;
  }

 private:
  scoped_ptr<MockIBusClient> mock_ibus_client_;
  scoped_ptr<MockIBusInputContextClient> mock_ibus_input_context_client_;
  scoped_ptr<MockIBusEngineService> mock_ibus_engine_service_;
  scoped_ptr<MockIBusEngineFactoryService> mock_ibus_engine_factory_service_;

  dbus::Bus* ibus_bus_;
  DISALLOW_COPY_AND_ASSIGN(MockDBusThreadManagerWithoutGMock);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_MOCK_DBUS_THREAD_MANAGER_WITHOUT_GMOCK_H_
