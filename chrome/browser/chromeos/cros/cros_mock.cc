// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/cros_mock.h"

#include "base/memory/ref_counted.h"
#include "base/message_loop.h"
#include "base/time.h"
#include "chrome/browser/chromeos/cros/mock_cryptohome_library.h"
#include "chrome/browser/chromeos/cros/mock_library_loader.h"
#include "chrome/browser/chromeos/cros/mock_network_library.h"
#include "chrome/browser/chromeos/cros/mock_power_library.h"
#include "chrome/browser/chromeos/cros/mock_screen_lock_library.h"
#include "chrome/browser/chromeos/cros/mock_speech_synthesis_library.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/login/wizard_screen.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

using ::testing::AnyNumber;
using ::testing::AtMost;
using ::testing::InSequence;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::StrictMock;
using ::testing::_;

CrosMock::CrosMock()
    : loader_(NULL),
      mock_cryptohome_library_(NULL),
      mock_network_library_(NULL),
      mock_power_library_(NULL),
      mock_screen_lock_library_(NULL),
      mock_speech_synthesis_library_(NULL) {
}

CrosMock::~CrosMock() {
}

chromeos::CrosLibrary::TestApi* CrosMock::test_api() {
  return chromeos::CrosLibrary::Get()->GetTestApi();
}

void CrosMock::InitStatusAreaMocks() {
  InitMockNetworkLibrary();
  InitMockPowerLibrary();
}

void CrosMock::InitMockLibraryLoader() {
  if (loader_)
    return;
  loader_ = new StrictMock<MockLibraryLoader>();
  EXPECT_CALL(*loader_, Load(_))
      .Times(AnyNumber())
      .WillRepeatedly(Return(true));
  test_api()->SetLibraryLoader(loader_, true);
}

void CrosMock::InitMockCryptohomeLibrary() {
  InitMockLibraryLoader();
  if (mock_cryptohome_library_)
    return;
  mock_cryptohome_library_ = new StrictMock<MockCryptohomeLibrary>();
  test_api()->SetCryptohomeLibrary(mock_cryptohome_library_, true);
}

void CrosMock::InitMockNetworkLibrary() {
  InitMockLibraryLoader();
  if (mock_network_library_)
    return;
  mock_network_library_ = new StrictMock<MockNetworkLibrary>();
  test_api()->SetNetworkLibrary(mock_network_library_, true);
}

void CrosMock::InitMockPowerLibrary() {
  InitMockLibraryLoader();
  if (mock_power_library_)
    return;
  mock_power_library_ = new StrictMock<MockPowerLibrary>();
  test_api()->SetPowerLibrary(mock_power_library_, true);
}

void CrosMock::InitMockScreenLockLibrary() {
  InitMockLibraryLoader();
  if (mock_screen_lock_library_)
    return;
  mock_screen_lock_library_ = new StrictMock<MockScreenLockLibrary>();
  test_api()->SetScreenLockLibrary(mock_screen_lock_library_, true);
}

void CrosMock::InitMockSpeechSynthesisLibrary() {
  InitMockLibraryLoader();
  if (mock_speech_synthesis_library_)
    return;
  mock_speech_synthesis_library_ =
      new StrictMock<MockSpeechSynthesisLibrary>();
  test_api()->SetSpeechSynthesisLibrary(mock_speech_synthesis_library_, true);
}

// Initialization of mocks.
MockCryptohomeLibrary* CrosMock::mock_cryptohome_library() {
  return mock_cryptohome_library_;
}

MockNetworkLibrary* CrosMock::mock_network_library() {
  return mock_network_library_;
}

MockPowerLibrary* CrosMock::mock_power_library() {
  return mock_power_library_;
}

MockScreenLockLibrary* CrosMock::mock_screen_lock_library() {
  return mock_screen_lock_library_;
}

MockSpeechSynthesisLibrary* CrosMock::mock_speech_synthesis_library() {
  return mock_speech_synthesis_library_;
}

void CrosMock::SetStatusAreaMocksExpectations() {
  SetNetworkLibraryStatusAreaExpectations();
  SetPowerLibraryStatusAreaExpectations();
  SetPowerLibraryExpectations();
}

void CrosMock::SetNetworkLibraryStatusAreaExpectations() {
  // We don't care how often these are called, just set their return values:
  EXPECT_CALL(*mock_network_library_, AddNetworkManagerObserver(_))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_network_library_, AddNetworkDeviceObserver(_, _))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_network_library_, AddCellularDataPlanObserver(_))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_network_library_, RemoveNetworkManagerObserver(_))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_network_library_, RemoveNetworkDeviceObserver(_, _))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_network_library_, RemoveObserverForAllNetworks(_))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_network_library_, RemoveCellularDataPlanObserver(_))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_network_library_, IsLocked())
      .Times(AnyNumber())
      .WillRepeatedly((Return(false)));
  EXPECT_CALL(*mock_network_library_, FindCellularDevice())
      .Times(AnyNumber())
      .WillRepeatedly((Return((const NetworkDevice*)(NULL))));
  EXPECT_CALL(*mock_network_library_, ethernet_available())
      .Times(AnyNumber())
      .WillRepeatedly((Return(true)));
  EXPECT_CALL(*mock_network_library_, wifi_available())
      .Times(AnyNumber())
      .WillRepeatedly((Return(false)));
  EXPECT_CALL(*mock_network_library_, cellular_available())
      .Times(AnyNumber())
      .WillRepeatedly((Return(false)));
  EXPECT_CALL(*mock_network_library_, ethernet_enabled())
      .Times(AnyNumber())
      .WillRepeatedly((Return(true)));
  EXPECT_CALL(*mock_network_library_, wifi_enabled())
      .Times(AnyNumber())
      .WillRepeatedly((Return(false)));
  EXPECT_CALL(*mock_network_library_, cellular_enabled())
      .Times(AnyNumber())
      .WillRepeatedly((Return(false)));
  EXPECT_CALL(*mock_network_library_, active_network())
      .Times(AnyNumber())
      .WillRepeatedly((Return((const Network*)(NULL))));
  EXPECT_CALL(*mock_network_library_, ethernet_network())
      .Times(AnyNumber())
      .WillRepeatedly((Return((const EthernetNetwork*)(NULL))));
  EXPECT_CALL(*mock_network_library_, wifi_network())
      .Times(AnyNumber())
      .WillRepeatedly((Return((const WifiNetwork*)(NULL))));
  EXPECT_CALL(*mock_network_library_, cellular_network())
      .Times(AnyNumber())
      .WillRepeatedly((Return((const CellularNetwork*)(NULL))));
  EXPECT_CALL(*mock_network_library_, virtual_network())
      .Times(AnyNumber())
      .WillRepeatedly((Return((const VirtualNetwork*)(NULL))));
  EXPECT_CALL(*mock_network_library_, wifi_networks())
      .Times(AnyNumber())
      .WillRepeatedly((ReturnRef(wifi_networks_)));
  EXPECT_CALL(*mock_network_library_, cellular_networks())
      .Times(AnyNumber())
      .WillRepeatedly((ReturnRef(cellular_networks_)));
  EXPECT_CALL(*mock_network_library_, virtual_networks())
      .Times(AnyNumber())
      .WillRepeatedly((ReturnRef(virtual_networks_)));
  EXPECT_CALL(*mock_network_library_, connected_network())
      .Times(AnyNumber())
      .WillRepeatedly((Return((const Network*)(NULL))));
  EXPECT_CALL(*mock_network_library_, connecting_network())
      .Times(AnyNumber())
      .WillRepeatedly((Return((const Network*)(NULL))));

  // Set specific expectations for interesting functions:

  // NetworkMenuButton::OnNetworkChanged() calls:
  EXPECT_CALL(*mock_network_library_, Connected())
      .Times(AnyNumber())
      .WillRepeatedly((Return(false)))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_network_library_, Connecting())
      .Times(AnyNumber())
      .WillRepeatedly((Return(false)))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_network_library_, cellular_connected())
      .Times(AnyNumber())
      .WillRepeatedly((Return(false)))
      .RetiresOnSaturation();

  // NetworkMenu::InitMenuItems() calls:
  EXPECT_CALL(*mock_network_library_, IsLocked())
      .Times(AnyNumber())
      .WillRepeatedly((Return(false)))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_network_library_, ethernet_connected())
      .Times(1)
      .WillRepeatedly((Return(false)))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_network_library_, ethernet_connecting())
      .Times(1)
      .WillRepeatedly((Return(false)))
      .RetiresOnSaturation();
}

void CrosMock::SetPowerLibraryStatusAreaExpectations() {
  EXPECT_CALL(*mock_power_library_, AddObserver(_))
      .Times(3)
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_power_library_, battery_fully_charged())
      .Times(1)
      .WillRepeatedly((Return(false)))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_power_library_, battery_is_present())
      .Times(1)
      .WillOnce((Return(true)))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_power_library_, battery_percentage())
      .Times(1)
      .WillRepeatedly((Return(42.0)))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_power_library_, line_power_on())
      .Times(1)
      .WillRepeatedly((Return(false)))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_power_library_, battery_time_to_empty())
      .Times(1)
      .WillRepeatedly((Return(base::TimeDelta::FromMinutes(42))))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_power_library_, battery_time_to_full())
      .Times(1)
      .WillRepeatedly((Return(base::TimeDelta::FromMinutes(24))))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_power_library_, RemoveObserver(_))
      .Times(3)
      .RetiresOnSaturation();
}

void CrosMock::SetPowerLibraryExpectations() {
  // EnableScreenLock is currently bounded with a prefs value and thus is
  // always called when loading
  EXPECT_CALL(*mock_power_library_, EnableScreenLock(_))
      .Times(AnyNumber());
}

void CrosMock::SetSpeechSynthesisLibraryExpectations() {
  InSequence s;
  EXPECT_CALL(*mock_speech_synthesis_library_, StopSpeaking())
      .WillOnce(Return(true))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_speech_synthesis_library_, SetSpeakProperties(_))
      .WillOnce(Return(true))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_speech_synthesis_library_, Speak(_))
      .WillOnce(Return(true))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_speech_synthesis_library_, IsSpeaking())
      .WillOnce(Return(true))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_speech_synthesis_library_, StopSpeaking())
      .WillOnce(Return(true))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_speech_synthesis_library_, SetSpeakProperties(_))
      .WillOnce(Return(true))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_speech_synthesis_library_, Speak(_))
      .WillOnce(Return(true))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_speech_synthesis_library_, IsSpeaking())
      .WillOnce(Return(true))
      .WillOnce(Return(true))
      .WillOnce(Return(false))
      .RetiresOnSaturation();
}

void CrosMock::TearDownMocks() {
  // Prevent bogus gMock leak check from firing.
  if (loader_)
    test_api()->SetLibraryLoader(NULL, false);
  if (mock_cryptohome_library_)
    test_api()->SetCryptohomeLibrary(NULL, false);
  if (mock_network_library_)
    test_api()->SetNetworkLibrary(NULL, false);
  if (mock_power_library_)
    test_api()->SetPowerLibrary(NULL, false);
  if (mock_screen_lock_library_)
    test_api()->SetScreenLockLibrary(NULL, false);
  if (mock_speech_synthesis_library_)
    test_api()->SetSpeechSynthesisLibrary(NULL, false);
}

}  // namespace chromeos
