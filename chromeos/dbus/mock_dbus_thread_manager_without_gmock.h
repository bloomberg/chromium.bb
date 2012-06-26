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

}  // namespace dbus

namespace chromeos {

class  MockIBusClient;
class  MockIBusInputContextClient;

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
  virtual FlimflamDeviceClient* GetFlimflamDeviceClient() OVERRIDE;
  virtual FlimflamIPConfigClient* GetFlimflamIPConfigClient() OVERRIDE;
  virtual FlimflamManagerClient* GetFlimflamManagerClient() OVERRIDE;
  virtual FlimflamNetworkClient* GetFlimflamNetworkClient() OVERRIDE;
  virtual FlimflamProfileClient* GetFlimflamProfileClient() OVERRIDE;
  virtual FlimflamServiceClient* GetFlimflamServiceClient() OVERRIDE;
  virtual GsmSMSClient* GetGsmSMSClient() OVERRIDE;
  virtual ImageBurnerClient* GetImageBurnerClient() OVERRIDE;
  virtual IntrospectableClient* GetIntrospectableClient() OVERRIDE;
  virtual ModemMessagingClient* GetModemMessagingClient() OVERRIDE;
  virtual PowerManagerClient* GetPowerManagerClient() OVERRIDE;
  virtual SessionManagerClient* GetSessionManagerClient() OVERRIDE;
  virtual SMSClient* GetSMSClient() OVERRIDE;
  virtual SpeechSynthesizerClient* GetSpeechSynthesizerClient() OVERRIDE;
  virtual UpdateEngineClient* GetUpdateEngineClient() OVERRIDE;
  virtual BluetoothOutOfBandClient* GetBluetoothOutOfBandClient() OVERRIDE;
  virtual IBusClient* GetIBusClient() OVERRIDE;
  virtual IBusInputContextClient* GetIBusInputContextClient() OVERRIDE;

  MockIBusClient* mock_ibus_client() {
    return mock_ibus_client_.get();
  }

  MockIBusInputContextClient* mock_ibus_input_context_client() {
    return mock_ibus_input_context_client_.get();
  }

  void set_ibus_bus(dbus::Bus* ibus_bus) {
    ibus_bus_ = ibus_bus;
  }

 private:
  scoped_ptr<MockIBusClient> mock_ibus_client_;
  scoped_ptr<MockIBusInputContextClient> mock_ibus_input_context_client_;

  dbus::Bus* ibus_bus_;
  DISALLOW_COPY_AND_ASSIGN(MockDBusThreadManagerWithoutGMock);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_MOCK_DBUS_THREAD_MANAGER_WITHOUT_GMOCK_H_
