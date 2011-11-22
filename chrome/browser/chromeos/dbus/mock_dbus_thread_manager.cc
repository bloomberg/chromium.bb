// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dbus/mock_dbus_thread_manager.h"

#include "chrome/browser/chromeos/dbus/mock_bluetooth_adapter_client.h"
#include "chrome/browser/chromeos/dbus/mock_bluetooth_manager_client.h"
#include "chrome/browser/chromeos/dbus/mock_cros_disks_client.h"
#include "chrome/browser/chromeos/dbus/mock_power_manager_client.h"
#include "chrome/browser/chromeos/dbus/mock_sensors_client.h"
#include "chrome/browser/chromeos/dbus/mock_session_manager_client.h"
#include "chrome/browser/chromeos/dbus/mock_speech_synthesizer_client.h"
#include "chrome/browser/chromeos/dbus/mock_update_engine_client.h"

using ::testing::AnyNumber;
using ::testing::Return;
using ::testing::_;

namespace chromeos {

MockDBusThreadManager::MockDBusThreadManager()
    : mock_bluetooth_adapter_client_(new MockBluetoothAdapterClient),
      mock_bluetooth_manager_client_(new MockBluetoothManagerClient),
      mock_cros_disks_client_(new MockCrosDisksClient),
      mock_power_manager_client_(new MockPowerManagerClient),
      mock_sensors_client_(new MockSensorsClient),
      mock_session_manager_client_(new MockSessionManagerClient),
      mock_speech_synthesizer_client_(new MockSpeechSynthesizerClient),
      mock_update_engine_client_(new MockUpdateEngineClient) {
  EXPECT_CALL(*this, GetBluetoothAdapterClient())
      .WillRepeatedly(Return(mock_bluetooth_adapter_client_.get()));
  EXPECT_CALL(*this, GetBluetoothManagerClient())
      .WillRepeatedly(Return(mock_bluetooth_manager_client()));
  EXPECT_CALL(*this, GetCrosDisksClient())
      .WillRepeatedly(Return(mock_cros_disks_client()));
  EXPECT_CALL(*this, GetPowerManagerClient())
      .WillRepeatedly(Return(mock_power_manager_client_.get()));
  EXPECT_CALL(*this, GetSensorsClient())
      .WillRepeatedly(Return(mock_sensors_client_.get()));
  EXPECT_CALL(*this, GetSessionManagerClient())
      .WillRepeatedly(Return(mock_session_manager_client_.get()));
  EXPECT_CALL(*this, GetSpeechSynthesizerClient())
      .WillRepeatedly(Return(mock_speech_synthesizer_client_.get()));
  EXPECT_CALL(*this, GetUpdateEngineClient())
      .WillRepeatedly(Return(mock_update_engine_client_.get()));

  // These observers calls are used in ChromeBrowserMainPartsChromeos.
  EXPECT_CALL(*mock_power_manager_client_.get(), AddObserver(_))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_power_manager_client_.get(), RemoveObserver(_))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_session_manager_client_.get(), AddObserver(_))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_session_manager_client_.get(), RemoveObserver(_))
      .Times(AnyNumber());
}

MockDBusThreadManager::~MockDBusThreadManager() {}

}  // namespace chromeos
