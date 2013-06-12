// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/mock_dbus_thread_manager.h"

#include "chromeos/dbus/dbus_thread_manager_observer.h"
#include "chromeos/dbus/fake_bluetooth_adapter_client.h"
#include "chromeos/dbus/fake_bluetooth_agent_manager_client.h"
#include "chromeos/dbus/fake_bluetooth_device_client.h"
#include "chromeos/dbus/fake_bluetooth_input_client.h"
#include "chromeos/dbus/fake_bluetooth_profile_manager_client.h"
#include "chromeos/dbus/ibus/mock_ibus_client.h"
#include "chromeos/dbus/ibus/mock_ibus_config_client.h"
#include "chromeos/dbus/ibus/mock_ibus_engine_factory_service.h"
#include "chromeos/dbus/ibus/mock_ibus_engine_service.h"
#include "chromeos/dbus/ibus/mock_ibus_input_context_client.h"
#include "chromeos/dbus/ibus/mock_ibus_panel_service.h"
#include "chromeos/dbus/mock_cryptohome_client.h"
#include "chromeos/dbus/mock_shill_device_client.h"
#include "chromeos/dbus/mock_shill_ipconfig_client.h"
#include "chromeos/dbus/mock_shill_manager_client.h"
#include "chromeos/dbus/mock_shill_profile_client.h"
#include "chromeos/dbus/mock_shill_service_client.h"
#include "chromeos/dbus/mock_gsm_sms_client.h"
#include "chromeos/dbus/mock_session_manager_client.h"
#include "chromeos/dbus/power_policy_controller.h"

using ::testing::AnyNumber;
using ::testing::Return;
using ::testing::ReturnNull;
using ::testing::SetArgumentPointee;
using ::testing::_;

namespace chromeos {

namespace {

std::vector<uint8>* GetMockSystemSalt() {
  static std::vector<uint8>* s_system_salt = NULL;
  if (!s_system_salt) {
    const char kStubSystemSalt[] = "stub_system_salt";
    s_system_salt = new std::vector<uint8>();
    s_system_salt->assign(kStubSystemSalt,
                          kStubSystemSalt + arraysize(kStubSystemSalt) - 1);
  }
  return s_system_salt;
}

}  // namespace

MockDBusThreadManager::MockDBusThreadManager()
    : fake_bluetooth_adapter_client_(new FakeBluetoothAdapterClient()),
      fake_bluetooth_agent_manager_client_(
          new FakeBluetoothAgentManagerClient()),
      fake_bluetooth_device_client_(new FakeBluetoothDeviceClient()),
      fake_bluetooth_input_client_(new FakeBluetoothInputClient()),
      fake_bluetooth_profile_manager_client_(
          new FakeBluetoothProfileManagerClient()),
      mock_cryptohome_client_(new MockCryptohomeClient),
      mock_shill_device_client_(new MockShillDeviceClient),
      mock_shill_ipconfig_client_(new MockShillIPConfigClient),
      mock_shill_manager_client_(new MockShillManagerClient),
      mock_shill_profile_client_(new MockShillProfileClient),
      mock_shill_service_client_(new MockShillServiceClient),
      mock_gsm_sms_client_(new MockGsmSMSClient),
      mock_session_manager_client_(new MockSessionManagerClient) {
  EXPECT_CALL(*this, GetCryptohomeClient())
      .WillRepeatedly(Return(mock_cryptohome_client()));
  EXPECT_CALL(*this, GetBluetoothAdapterClient())
      .WillRepeatedly(Return(fake_bluetooth_adapter_client_.get()));
  EXPECT_CALL(*this, GetBluetoothAgentManagerClient())
      .WillRepeatedly(Return(fake_bluetooth_agent_manager_client()));
  EXPECT_CALL(*this, GetBluetoothDeviceClient())
      .WillRepeatedly(Return(fake_bluetooth_device_client_.get()));
  EXPECT_CALL(*this, GetBluetoothInputClient())
      .WillRepeatedly(Return(fake_bluetooth_input_client_.get()));
  EXPECT_CALL(*this, GetBluetoothProfileManagerClient())
      .WillRepeatedly(Return(fake_bluetooth_profile_manager_client()));
  EXPECT_CALL(*this, GetShillDeviceClient())
      .WillRepeatedly(Return(mock_shill_device_client()));
  EXPECT_CALL(*this, GetShillIPConfigClient())
      .WillRepeatedly(Return(mock_shill_ipconfig_client()));
  EXPECT_CALL(*this, GetShillManagerClient())
      .WillRepeatedly(Return(mock_shill_manager_client()));
  EXPECT_CALL(*this, GetShillProfileClient())
      .WillRepeatedly(Return(mock_shill_profile_client()));
  EXPECT_CALL(*this, GetShillServiceClient())
      .WillRepeatedly(Return(mock_shill_service_client()));
  EXPECT_CALL(*this, GetGsmSMSClient())
      .WillRepeatedly(Return(mock_gsm_sms_client()));
  EXPECT_CALL(*this, GetSessionManagerClient())
      .WillRepeatedly(Return(mock_session_manager_client_.get()));

  EXPECT_CALL(*this, GetSystemBus())
      .WillRepeatedly(ReturnNull());
  EXPECT_CALL(*this, GetIBusBus())
      .WillRepeatedly(ReturnNull());

  // These observers calls are used in ChromeBrowserMainPartsChromeos.
  EXPECT_CALL(*mock_session_manager_client_.get(), AddObserver(_))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_session_manager_client_.get(), RemoveObserver(_))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_session_manager_client_.get(), HasObserver(_))
      .Times(AnyNumber());

  // Called from AsyncMethodCaller ctor and dtor.
  EXPECT_CALL(*mock_cryptohome_client_.get(), SetAsyncCallStatusHandlers(_, _))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_cryptohome_client_.get(), ResetAsyncCallStatusHandlers())
      .Times(AnyNumber());
  // Called from various locations.
  EXPECT_CALL(*mock_cryptohome_client_.get(), GetSystemSalt(_))
      .WillRepeatedly(DoAll(SetArgumentPointee<0>(*GetMockSystemSalt()),
                            Return(true)));
  EXPECT_CALL(*mock_cryptohome_client_.get(), TpmIsEnabled(_))
      .Times(AnyNumber());

  // Called from GeolocationHandler::Init().
  EXPECT_CALL(*mock_shill_manager_client_.get(), GetProperties(_))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_shill_manager_client_.get(), AddPropertyChangedObserver(_))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_shill_manager_client_.get(),
              RemovePropertyChangedObserver(_))
      .Times(AnyNumber());
}

MockDBusThreadManager::~MockDBusThreadManager() {
  FOR_EACH_OBSERVER(DBusThreadManagerObserver, observers_,
                    OnDBusThreadManagerDestroying(this));
}

void MockDBusThreadManager::AddObserver(DBusThreadManagerObserver* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void MockDBusThreadManager::RemoveObserver(
    DBusThreadManagerObserver* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

}  // namespace chromeos
