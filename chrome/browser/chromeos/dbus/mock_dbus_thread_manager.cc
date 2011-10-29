// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dbus/mock_dbus_thread_manager.h"

#include "chrome/browser/chromeos/dbus/mock_bluetooth_adapter_client.h"
#include "chrome/browser/chromeos/dbus/mock_bluetooth_manager_client.h"
#include "chrome/browser/chromeos/dbus/mock_power_manager_client.h"
#include "chrome/browser/chromeos/dbus/mock_sensors_client.h"
#include "chrome/browser/chromeos/dbus/mock_session_manager_client.h"
#include "chrome/browser/chromeos/dbus/mock_speech_synthesizer_client.h"

using ::testing::Return;

namespace chromeos {

MockDBusThreadManager::MockDBusThreadManager()
    : mock_bluetooth_adapter_client_(new MockBluetoothAdapterClient),
      mock_bluetooth_manager_client_(new MockBluetoothManagerClient),
      mock_power_manager_client_(new MockPowerManagerClient),
      mock_sensors_client_(new MockSensorsClient),
      mock_session_manager_client_(new MockSessionManagerClient),
      mock_speech_synthesizer_client_(new MockSpeechSynthesizerClient) {
  EXPECT_CALL(*this, bluetooth_adapter_client())
      .WillRepeatedly(Return(mock_bluetooth_adapter_client_.get()));
  EXPECT_CALL(*this, bluetooth_manager_client())
      .WillRepeatedly(Return(mock_bluetooth_manager_client()));
  EXPECT_CALL(*this, power_manager_client())
      .WillRepeatedly(Return(mock_power_manager_client_.get()));
  EXPECT_CALL(*this, sensors_client())
      .WillRepeatedly(Return(mock_sensors_client_.get()));
  EXPECT_CALL(*this, session_manager_client())
      .WillRepeatedly(Return(mock_session_manager_client_.get()));
  EXPECT_CALL(*this, speech_synthesizer_client())
      .WillRepeatedly(Return(mock_speech_synthesizer_client_.get()));
}

MockDBusThreadManager::~MockDBusThreadManager() {}

}  // namespace chromeos
