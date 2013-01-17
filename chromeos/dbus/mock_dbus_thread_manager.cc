// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/mock_dbus_thread_manager.h"

#include "chromeos/dbus/dbus_thread_manager_observer.h"
#include "chromeos/dbus/mock_bluetooth_adapter_client.h"
#include "chromeos/dbus/mock_bluetooth_device_client.h"
#include "chromeos/dbus/mock_bluetooth_input_client.h"
#include "chromeos/dbus/mock_bluetooth_manager_client.h"
#include "chromeos/dbus/mock_bluetooth_node_client.h"
#include "chromeos/dbus/mock_bluetooth_out_of_band_client.h"
#include "chromeos/dbus/mock_cros_disks_client.h"
#include "chromeos/dbus/mock_cryptohome_client.h"
#include "chromeos/dbus/mock_debug_daemon_client.h"
#include "chromeos/dbus/mock_shill_device_client.h"
#include "chromeos/dbus/mock_shill_ipconfig_client.h"
#include "chromeos/dbus/mock_shill_manager_client.h"
#include "chromeos/dbus/mock_shill_profile_client.h"
#include "chromeos/dbus/mock_shill_service_client.h"
#include "chromeos/dbus/mock_gsm_sms_client.h"
#include "chromeos/dbus/mock_image_burner_client.h"
#include "chromeos/dbus/mock_introspectable_client.h"
#include "chromeos/dbus/mock_modem_messaging_client.h"
#include "chromeos/dbus/mock_permission_broker_client.h"
#include "chromeos/dbus/mock_power_manager_client.h"
#include "chromeos/dbus/mock_session_manager_client.h"
#include "chromeos/dbus/mock_sms_client.h"
#include "chromeos/dbus/mock_speech_synthesizer_client.h"
#include "chromeos/dbus/mock_update_engine_client.h"

using ::testing::AnyNumber;
using ::testing::Return;
using ::testing::ReturnNull;
using ::testing::_;

namespace chromeos {

MockDBusThreadManager::MockDBusThreadManager()
    : mock_bluetooth_adapter_client_(new MockBluetoothAdapterClient),
      mock_bluetooth_device_client_(new MockBluetoothDeviceClient),
      mock_bluetooth_input_client_(new MockBluetoothInputClient),
      mock_bluetooth_manager_client_(new MockBluetoothManagerClient),
      mock_bluetooth_node_client_(new MockBluetoothNodeClient),
      mock_bluetooth_out_of_band_client_(new MockBluetoothOutOfBandClient),
      mock_cros_disks_client_(new MockCrosDisksClient),
      mock_cryptohome_client_(new MockCryptohomeClient),
      mock_debugdaemon_client_(new MockDebugDaemonClient),
      mock_shill_device_client_(new MockShillDeviceClient),
      mock_shill_ipconfig_client_(new MockShillIPConfigClient),
      mock_shill_manager_client_(new MockShillManagerClient),
      mock_shill_profile_client_(new MockShillProfileClient),
      mock_shill_service_client_(new MockShillServiceClient),
      mock_gsm_sms_client_(new MockGsmSMSClient),
      mock_image_burner_client_(new MockImageBurnerClient),
      mock_introspectable_client_(new MockIntrospectableClient),
      mock_modem_messaging_client_(new MockModemMessagingClient),
      mock_permission_broker_client_(new MockPermissionBrokerClient),
      mock_power_manager_client_(new MockPowerManagerClient),
      mock_session_manager_client_(new MockSessionManagerClient),
      mock_sms_client_(new MockSMSClient),
      mock_speech_synthesizer_client_(new MockSpeechSynthesizerClient),
      mock_update_engine_client_(new MockUpdateEngineClient) {
  EXPECT_CALL(*this, GetBluetoothAdapterClient())
      .WillRepeatedly(Return(mock_bluetooth_adapter_client_.get()));
  EXPECT_CALL(*this, GetBluetoothDeviceClient())
      .WillRepeatedly(Return(mock_bluetooth_device_client_.get()));
  EXPECT_CALL(*this, GetBluetoothInputClient())
      .WillRepeatedly(Return(mock_bluetooth_input_client_.get()));
  EXPECT_CALL(*this, GetBluetoothManagerClient())
      .WillRepeatedly(Return(mock_bluetooth_manager_client()));
  EXPECT_CALL(*this, GetBluetoothNodeClient())
      .WillRepeatedly(Return(mock_bluetooth_node_client_.get()));
  EXPECT_CALL(*this, GetBluetoothOutOfBandClient())
      .WillRepeatedly(Return(mock_bluetooth_out_of_band_client_.get()));
  EXPECT_CALL(*this, GetCrosDisksClient())
      .WillRepeatedly(Return(mock_cros_disks_client()));
  EXPECT_CALL(*this, GetCryptohomeClient())
      .WillRepeatedly(Return(mock_cryptohome_client()));
  EXPECT_CALL(*this, GetDebugDaemonClient())
      .WillRepeatedly(Return(mock_debugdaemon_client()));
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
  EXPECT_CALL(*this, GetImageBurnerClient())
      .WillRepeatedly(Return(mock_image_burner_client()));
  EXPECT_CALL(*this, GetIntrospectableClient())
      .WillRepeatedly(Return(mock_introspectable_client()));
  EXPECT_CALL(*this, GetModemMessagingClient())
      .WillRepeatedly(Return(mock_modem_messaging_client()));
  EXPECT_CALL(*this, GetPowerManagerClient())
      .WillRepeatedly(Return(mock_power_manager_client_.get()));
  EXPECT_CALL(*this, GetSessionManagerClient())
      .WillRepeatedly(Return(mock_session_manager_client_.get()));
  EXPECT_CALL(*this, GetSMSClient())
      .WillRepeatedly(Return(mock_sms_client_.get()));
  EXPECT_CALL(*this, GetSpeechSynthesizerClient())
      .WillRepeatedly(Return(mock_speech_synthesizer_client_.get()));
  EXPECT_CALL(*this, GetUpdateEngineClient())
      .WillRepeatedly(Return(mock_update_engine_client_.get()));

  EXPECT_CALL(*this, GetIBusBus())
      .WillRepeatedly(ReturnNull());

  // These observers calls are used in ChromeBrowserMainPartsChromeos.
  EXPECT_CALL(*mock_power_manager_client_.get(), AddObserver(_))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_power_manager_client_.get(), RemoveObserver(_))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_session_manager_client_.get(), AddObserver(_))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_session_manager_client_.get(), RemoveObserver(_))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_update_engine_client_.get(), AddObserver(_))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_update_engine_client_.get(), RemoveObserver(_))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_bluetooth_manager_client_.get(), AddObserver(_))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_bluetooth_manager_client_.get(), RemoveObserver(_))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_bluetooth_adapter_client_.get(), AddObserver(_))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_bluetooth_adapter_client_.get(), RemoveObserver(_))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_bluetooth_device_client_.get(), AddObserver(_))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_bluetooth_device_client_.get(), RemoveObserver(_))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_bluetooth_input_client_.get(), AddObserver(_))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_bluetooth_input_client_.get(), RemoveObserver(_))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_bluetooth_node_client_.get(), AddObserver(_))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_bluetooth_node_client_.get(), RemoveObserver(_))
      .Times(AnyNumber());

  // Called from PowerMenuButton ctor.
  EXPECT_CALL(*mock_power_manager_client_.get(), RequestStatusUpdate(_))
      .Times(AnyNumber());

  // Called from DiskMountManager::Initialize(), ChromeBrowserMainPartsChromeos.
  EXPECT_CALL(*mock_cros_disks_client_.get(), SetUpConnections(_, _))
      .Times(AnyNumber());

  // Called from BluetoothManagerImpl ctor.
  EXPECT_CALL(*mock_bluetooth_manager_client_.get(), DefaultAdapter(_))
      .Times(AnyNumber());

  // Called from AsyncMethodCaller ctor and dtor.
  EXPECT_CALL(*mock_cryptohome_client_.get(), SetAsyncCallStatusHandlers(_, _))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_cryptohome_client_.get(), ResetAsyncCallStatusHandlers())
      .Times(AnyNumber());

  // Called from BrightnessController::GetBrightnessPercent as part of ash tray
  // initialization.
  EXPECT_CALL(*mock_power_manager_client_.get(), GetScreenBrightnessPercent(_))
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
