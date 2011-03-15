// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/cros_mock.h"

#include "base/message_loop.h"
#include "base/ref_counted.h"
#include "base/time.h"
#include "chrome/browser/chromeos/cros/mock_cryptohome_library.h"
#include "chrome/browser/chromeos/cros/mock_input_method_library.h"
#include "chrome/browser/chromeos/cros/mock_keyboard_library.h"
#include "chrome/browser/chromeos/cros/mock_library_loader.h"
#include "chrome/browser/chromeos/cros/mock_network_library.h"
#include "chrome/browser/chromeos/cros/mock_power_library.h"
#include "chrome/browser/chromeos/cros/mock_screen_lock_library.h"
#include "chrome/browser/chromeos/cros/mock_speech_synthesis_library.h"
#include "chrome/browser/chromeos/cros/mock_system_library.h"
#include "chrome/browser/chromeos/cros/mock_touchpad_library.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/login/wizard_screen.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
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
      mock_keyboard_library_(NULL),
      mock_input_method_library_(NULL),
      mock_network_library_(NULL),
      mock_power_library_(NULL),
      mock_screen_lock_library_(NULL),
      mock_speech_synthesis_library_(NULL),
      mock_system_library_(NULL),
      mock_touchpad_library_(NULL),
      current_input_method_("", "", "", ""),
      previous_input_method_("", "", "", "") {
  current_input_method_ =
      input_method::GetFallbackInputMethodDescriptor();
}

CrosMock::~CrosMock() {
}

chromeos::CrosLibrary::TestApi* CrosMock::test_api() {
  return chromeos::CrosLibrary::Get()->GetTestApi();
}

void CrosMock::InitStatusAreaMocks() {
  InitMockKeyboardLibrary();
  InitMockInputMethodLibrary();
  InitMockNetworkLibrary();
  InitMockPowerLibrary();
  InitMockTouchpadLibrary();
  InitMockSystemLibrary();
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

void CrosMock::InitMockKeyboardLibrary() {
  InitMockLibraryLoader();
  if (mock_keyboard_library_)
    return;
  mock_keyboard_library_ = new StrictMock<MockKeyboardLibrary>();
  test_api()->SetKeyboardLibrary(mock_keyboard_library_, true);
}

void CrosMock::InitMockInputMethodLibrary() {
  InitMockLibraryLoader();
  if (mock_input_method_library_)
    return;
  mock_input_method_library_ = new StrictMock<MockInputMethodLibrary>();
  test_api()->SetInputMethodLibrary(mock_input_method_library_, true);
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

void CrosMock::InitMockTouchpadLibrary() {
  InitMockLibraryLoader();
  if (mock_touchpad_library_)
    return;
  mock_touchpad_library_ = new StrictMock<MockTouchpadLibrary>();
  test_api()->SetTouchpadLibrary(mock_touchpad_library_, true);
}

void CrosMock::InitMockSystemLibrary() {
  InitMockLibraryLoader();
  if (mock_system_library_)
    return;
  mock_system_library_ = new StrictMock<MockSystemLibrary>();
  test_api()->SetSystemLibrary(mock_system_library_, true);
}

// Initialization of mocks.
MockCryptohomeLibrary* CrosMock::mock_cryptohome_library() {
  return mock_cryptohome_library_;
}

MockKeyboardLibrary* CrosMock::mock_keyboard_library() {
  return mock_keyboard_library_;
}

MockInputMethodLibrary* CrosMock::mock_input_method_library() {
  return mock_input_method_library_;
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

MockSystemLibrary* CrosMock::mock_system_library() {
  return mock_system_library_;
}

MockTouchpadLibrary* CrosMock::mock_touchpad_library() {
  return mock_touchpad_library_;
}

void CrosMock::SetStatusAreaMocksExpectations() {
  SetKeyboardLibraryStatusAreaExpectations();
  SetInputMethodLibraryStatusAreaExpectations();
  SetNetworkLibraryStatusAreaExpectations();
  SetPowerLibraryStatusAreaExpectations();
  SetPowerLibraryExpectations();
  SetTouchpadLibraryExpectations();
  SetSystemLibraryStatusAreaExpectations();
}

void CrosMock::SetKeyboardLibraryStatusAreaExpectations() {
  EXPECT_CALL(*mock_keyboard_library_, SetCurrentKeyboardLayoutByName(_))
      .Times(AnyNumber())
      .WillRepeatedly((Return(true)))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_keyboard_library_, RemapModifierKeys(_))
      .Times(AnyNumber())
      .WillRepeatedly((Return(true)))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_keyboard_library_, SetAutoRepeatEnabled(_))
      .Times(AnyNumber())
      .WillRepeatedly((Return(true)))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_keyboard_library_, SetAutoRepeatRate(_))
      .Times(AnyNumber())
      .WillRepeatedly((Return(true)))
      .RetiresOnSaturation();
}

void CrosMock::SetInputMethodLibraryStatusAreaExpectations() {
  EXPECT_CALL(*mock_input_method_library_, AddObserver(_))
      .Times(AnyNumber())
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_input_method_library_, GetActiveInputMethods())
      .Times(AnyNumber())
      .WillRepeatedly(InvokeWithoutArgs(CreateInputMethodDescriptors))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_input_method_library_, GetSupportedInputMethods())
      .Times(AnyNumber())
      .WillRepeatedly(InvokeWithoutArgs(CreateInputMethodDescriptors))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_input_method_library_, current_input_method())
      .Times(AnyNumber())
      .WillRepeatedly((ReturnRef(current_input_method_)))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_input_method_library_, previous_input_method())
      .Times(AnyNumber())
      .WillRepeatedly((ReturnRef(previous_input_method_)))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_input_method_library_, current_ime_properties())
      .Times(AnyNumber())
      .WillRepeatedly((ReturnRef(ime_properties_)))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_input_method_library_, GetNumActiveInputMethods())
      .Times(AnyNumber())
      .WillRepeatedly((Return(1)))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_input_method_library_, SetImeConfig(_, _, _))
      .Times(AnyNumber())
      .WillRepeatedly((Return(true)))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_input_method_library_, RemoveObserver(_))
      .Times(AnyNumber())
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_input_method_library_, SetDeferImeStartup(_))
      .Times(AnyNumber())
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_input_method_library_, StopInputMethodDaemon())
      .Times(AnyNumber())
      .RetiresOnSaturation();
}

void CrosMock::SetNetworkLibraryStatusAreaExpectations() {
  // We don't care how often these are called, just set their return values:
  EXPECT_CALL(*mock_network_library_, AddNetworkManagerObserver(_))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_network_library_, AddCellularDataPlanObserver(_))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_network_library_, RemoveNetworkManagerObserver(_))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_network_library_, RemoveObserverForAllNetworks(_))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_network_library_, RemoveCellularDataPlanObserver(_))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_network_library_, IsLocked())
      .Times(AnyNumber())
      .WillRepeatedly((Return(false)));
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
      .Times(2)
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
      .Times(2)
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
  EXPECT_CALL(*mock_speech_synthesis_library_, Speak(_))
      .WillOnce(Return(true))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_speech_synthesis_library_, IsSpeaking())
      .Times(AnyNumber())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_speech_synthesis_library_, StopSpeaking())
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

void CrosMock::SetSystemLibraryStatusAreaExpectations() {
  EXPECT_CALL(*mock_system_library_, AddObserver(_))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_system_library_, RemoveObserver(_))
      .Times(1)
      .RetiresOnSaturation();
}

void CrosMock::SetTouchpadLibraryExpectations() {
  EXPECT_CALL(*mock_touchpad_library_, SetSensitivity(_))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_touchpad_library_, SetTapToClick(_))
      .Times(AnyNumber());
}

void CrosMock::SetSystemLibraryExpectations() {
  EXPECT_CALL(*mock_system_library_, GetTimezone())
      .Times(AnyNumber());
  EXPECT_CALL(*mock_system_library_, SetTimezone(_))
      .Times(AnyNumber());
}

void CrosMock::TearDownMocks() {
  // Prevent bogus gMock leak check from firing.
  if (loader_)
    test_api()->SetLibraryLoader(NULL, false);
  if (mock_cryptohome_library_)
    test_api()->SetCryptohomeLibrary(NULL, false);
  if (mock_keyboard_library_)
    test_api()->SetKeyboardLibrary(NULL, false);
  if (mock_input_method_library_)
    test_api()->SetInputMethodLibrary(NULL, false);
  if (mock_network_library_)
    test_api()->SetNetworkLibrary(NULL, false);
  if (mock_power_library_)
    test_api()->SetPowerLibrary(NULL, false);
  if (mock_screen_lock_library_)
    test_api()->SetScreenLockLibrary(NULL, false);
  if (mock_speech_synthesis_library_)
    test_api()->SetSpeechSynthesisLibrary(NULL, false);
  if (mock_system_library_)
    test_api()->SetSystemLibrary(NULL, false);
  if (mock_touchpad_library_)
    test_api()->SetTouchpadLibrary(NULL, false);
}

InputMethodDescriptors* CrosMock::CreateInputMethodDescriptors() {
  InputMethodDescriptors* descriptors = new InputMethodDescriptors;
  descriptors->push_back(
      input_method::GetFallbackInputMethodDescriptor());
  return descriptors;
}

}  // namespace chromeos
