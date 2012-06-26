// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/mock_dbus_thread_manager_without_gmock.h"

#include "chromeos/dbus/ibus/mock_ibus_client.h"
#include "chromeos/dbus/ibus/mock_ibus_input_context_client.h"

namespace chromeos {

MockDBusThreadManagerWithoutGMock::MockDBusThreadManagerWithoutGMock()
  : mock_ibus_client_(new MockIBusClient),
    mock_ibus_input_context_client_(new MockIBusInputContextClient),
    ibus_bus_(NULL) {
}

MockDBusThreadManagerWithoutGMock::~MockDBusThreadManagerWithoutGMock() {}

void MockDBusThreadManagerWithoutGMock::InitIBusBus(
    const std::string& ibus_address) {
  // Non-null bus address is used to ensure the connection to ibus-daemon.
  ibus_bus_ = reinterpret_cast<dbus::Bus*>(0xdeadbeef);
}

dbus::Bus* MockDBusThreadManagerWithoutGMock::GetSystemBus() {
  return NULL;
}

dbus::Bus* MockDBusThreadManagerWithoutGMock::GetIBusBus() {
  return ibus_bus_;
}

BluetoothAdapterClient*
    MockDBusThreadManagerWithoutGMock::GetBluetoothAdapterClient() {
  NOTIMPLEMENTED();
  return NULL;
}

BluetoothDeviceClient*
    MockDBusThreadManagerWithoutGMock::GetBluetoothDeviceClient() {
  NOTIMPLEMENTED();
  return NULL;
}

BluetoothInputClient*
    MockDBusThreadManagerWithoutGMock::GetBluetoothInputClient() {
  NOTIMPLEMENTED();
  return NULL;
}

BluetoothManagerClient*
    MockDBusThreadManagerWithoutGMock::GetBluetoothManagerClient() {
  NOTIMPLEMENTED();
  return NULL;
}

BluetoothNodeClient*
    MockDBusThreadManagerWithoutGMock::GetBluetoothNodeClient() {
  NOTIMPLEMENTED();
  return NULL;
}

CashewClient* MockDBusThreadManagerWithoutGMock::GetCashewClient() {
  NOTIMPLEMENTED();
  return NULL;
}

CrosDisksClient* MockDBusThreadManagerWithoutGMock::GetCrosDisksClient() {
  NOTIMPLEMENTED();
  return NULL;
}

CryptohomeClient* MockDBusThreadManagerWithoutGMock::GetCryptohomeClient() {
  NOTIMPLEMENTED();
  return NULL;
}

DebugDaemonClient* MockDBusThreadManagerWithoutGMock::GetDebugDaemonClient() {
  NOTIMPLEMENTED();
  return NULL;
}

FlimflamDeviceClient*
    MockDBusThreadManagerWithoutGMock::GetFlimflamDeviceClient() {
  NOTIMPLEMENTED();
  return NULL;
}

FlimflamIPConfigClient*
    MockDBusThreadManagerWithoutGMock::GetFlimflamIPConfigClient() {
  NOTIMPLEMENTED();
  return NULL;
}

FlimflamManagerClient*
    MockDBusThreadManagerWithoutGMock::GetFlimflamManagerClient() {
  NOTIMPLEMENTED();
  return NULL;
}

FlimflamNetworkClient*
    MockDBusThreadManagerWithoutGMock::GetFlimflamNetworkClient() {
  NOTIMPLEMENTED();
  return NULL;
}

FlimflamProfileClient*
    MockDBusThreadManagerWithoutGMock::GetFlimflamProfileClient() {
  NOTIMPLEMENTED();
  return NULL;
}

FlimflamServiceClient*
    MockDBusThreadManagerWithoutGMock::GetFlimflamServiceClient() {
  NOTIMPLEMENTED();
  return NULL;
}

GsmSMSClient* MockDBusThreadManagerWithoutGMock::GetGsmSMSClient() {
  NOTIMPLEMENTED();
  return NULL;
}

ImageBurnerClient* MockDBusThreadManagerWithoutGMock::GetImageBurnerClient() {
  NOTIMPLEMENTED();
  return NULL;
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

PowerManagerClient* MockDBusThreadManagerWithoutGMock::GetPowerManagerClient() {
  NOTIMPLEMENTED();
  return NULL;
}

SessionManagerClient*
    MockDBusThreadManagerWithoutGMock::GetSessionManagerClient() {
  NOTIMPLEMENTED();
  return NULL;
}

SMSClient* MockDBusThreadManagerWithoutGMock::GetSMSClient() {
  NOTIMPLEMENTED();
  return NULL;
}

SpeechSynthesizerClient*
    MockDBusThreadManagerWithoutGMock::GetSpeechSynthesizerClient() {
  NOTIMPLEMENTED();
  return NULL;
}

UpdateEngineClient* MockDBusThreadManagerWithoutGMock::GetUpdateEngineClient() {
  NOTIMPLEMENTED();
  return NULL;
}

BluetoothOutOfBandClient*
    MockDBusThreadManagerWithoutGMock::GetBluetoothOutOfBandClient() {
  NOTIMPLEMENTED();
  return NULL;
}

IBusClient* MockDBusThreadManagerWithoutGMock::GetIBusClient() {
  return mock_ibus_client_.get();
}

IBusInputContextClient*
    MockDBusThreadManagerWithoutGMock::GetIBusInputContextClient() {
  return mock_ibus_input_context_client_.get();
}


}  // namespace chromeos
