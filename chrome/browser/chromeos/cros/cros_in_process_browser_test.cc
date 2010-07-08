// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/cros_in_process_browser_test.h"

#include "base/message_loop.h"
#include "base/ref_counted.h"
#include "base/time.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/cros/mock_cryptohome_library.h"
#include "chrome/browser/chromeos/cros/mock_keyboard_library.h"
#include "chrome/browser/chromeos/cros/mock_input_method_library.h"
#include "chrome/browser/chromeos/cros/mock_library_loader.h"
#include "chrome/browser/chromeos/cros/mock_network_library.h"
#include "chrome/browser/chromeos/cros/mock_power_library.h"
#include "chrome/browser/chromeos/cros/mock_screen_lock_library.h"
#include "chrome/browser/chromeos/cros/mock_synaptics_library.h"
#include "chrome/browser/chromeos/cros/mock_system_library.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/login/wizard_screen.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {
using ::testing::AnyNumber;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::StrictMock;
using ::testing::_;

CrosInProcessBrowserTest::CrosInProcessBrowserTest()
    : loader_(NULL),
      mock_cryptohome_library_(NULL),
      mock_keyboard_library_(NULL),
      mock_input_method_library_(NULL),
      mock_network_library_(NULL),
      mock_power_library_(NULL),
      mock_screen_lock_library_(NULL),
      mock_synaptics_library_(NULL),
      mock_system_library_(NULL) {}

CrosInProcessBrowserTest::~CrosInProcessBrowserTest() {
}

chromeos::CrosLibrary::TestApi* CrosInProcessBrowserTest::test_api() {
  return chromeos::CrosLibrary::Get()->GetTestApi();
}

void CrosInProcessBrowserTest::InitStatusAreaMocks() {
  InitMockKeyboardLibrary();
  InitMockInputMethodLibrary();
  InitMockNetworkLibrary();
  InitMockPowerLibrary();
  InitMockSynapticsLibrary();
  InitMockSystemLibrary();
}

void CrosInProcessBrowserTest::InitMockLibraryLoader() {
  if (loader_)
    return;
  loader_ = new StrictMock<MockLibraryLoader>();
  EXPECT_CALL(*loader_, Load(_))
      .Times(AnyNumber())
      .WillRepeatedly(Return(true));
  test_api()->SetLibraryLoader(loader_, true);
}

void CrosInProcessBrowserTest::InitMockCryptohomeLibrary() {
  InitMockLibraryLoader();
  if (mock_cryptohome_library_)
    return;
  mock_cryptohome_library_ = new StrictMock<MockCryptohomeLibrary>();
  test_api()->SetCryptohomeLibrary(mock_cryptohome_library_, true);
}

void CrosInProcessBrowserTest::InitMockKeyboardLibrary() {
  InitMockLibraryLoader();
  if (mock_keyboard_library_)
    return;
  mock_keyboard_library_ = new StrictMock<MockKeyboardLibrary>();
  test_api()->SetKeyboardLibrary(mock_keyboard_library_, true);
}

void CrosInProcessBrowserTest::InitMockInputMethodLibrary() {
  InitMockLibraryLoader();
  if (mock_input_method_library_)
    return;
  mock_input_method_library_ = new StrictMock<MockInputMethodLibrary>();
  test_api()->SetInputMethodLibrary(mock_input_method_library_, true);
}

void CrosInProcessBrowserTest::InitMockNetworkLibrary() {
  InitMockLibraryLoader();
  if (mock_network_library_)
    return;
  mock_network_library_ = new StrictMock<MockNetworkLibrary>();
  test_api()->SetNetworkLibrary(mock_network_library_, true);
}

void CrosInProcessBrowserTest::InitMockPowerLibrary() {
  InitMockLibraryLoader();
  if (mock_power_library_)
    return;
  mock_power_library_ = new StrictMock<MockPowerLibrary>();
  test_api()->SetPowerLibrary(mock_power_library_, true);
}

void CrosInProcessBrowserTest::InitMockScreenLockLibrary() {
  InitMockLibraryLoader();
  if (mock_screen_lock_library_)
    return;
  mock_screen_lock_library_ = new StrictMock<MockScreenLockLibrary>();
  test_api()->SetScreenLockLibrary(mock_screen_lock_library_, true);
}

void CrosInProcessBrowserTest::InitMockSynapticsLibrary() {
  InitMockLibraryLoader();
  if (mock_synaptics_library_)
    return;
  mock_synaptics_library_ = new StrictMock<MockSynapticsLibrary>();
  test_api()->SetSynapticsLibrary(mock_synaptics_library_, true);
}

void CrosInProcessBrowserTest::InitMockSystemLibrary() {
  InitMockLibraryLoader();
  if (mock_system_library_)
    return;
  mock_system_library_ = new StrictMock<MockSystemLibrary>();
  test_api()->SetSystemLibrary(mock_system_library_, true);
}

void CrosInProcessBrowserTest::SetStatusAreaMocksExpectations() {
  SetKeyboardLibraryStatusAreaExpectations();
  SetInputMethodLibraryStatusAreaExpectations();
  SetNetworkLibraryStatusAreaExpectations();
  SetPowerLibraryStatusAreaExpectations();
  SetSynapticsLibraryExpectations();
  SetSystemLibraryStatusAreaExpectations();
}

void CrosInProcessBrowserTest::SetKeyboardLibraryStatusAreaExpectations() {
  EXPECT_CALL(*mock_keyboard_library_, GetCurrentKeyboardLayoutName())
      .Times(AnyNumber())
      .WillRepeatedly((Return("us")))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_keyboard_library_, SetCurrentKeyboardLayoutByName(_))
      .Times(AnyNumber())
      .WillRepeatedly((Return(true)))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_keyboard_library_, SetKeyboardLayoutPerWindow(_))
      .Times(AnyNumber())
      .WillRepeatedly((Return(true)))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_keyboard_library_, GetKeyboardLayoutPerWindow(_))
      .Times(AnyNumber())
      .WillRepeatedly((Return(true)))
      .RetiresOnSaturation();
}

void CrosInProcessBrowserTest::SetInputMethodLibraryStatusAreaExpectations() {
  EXPECT_CALL(*mock_input_method_library_, AddObserver(_))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_input_method_library_, GetActiveInputMethods())
      .Times(AnyNumber())
      .WillRepeatedly(InvokeWithoutArgs(CreateFallbackInputMethodDescriptors))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_input_method_library_, GetSupportedInputMethods())
      .Times(AnyNumber())
      .WillRepeatedly(InvokeWithoutArgs(CreateFallbackInputMethodDescriptors))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_input_method_library_, current_ime_properties())
      .Times(1)
      .WillOnce((ReturnRef(ime_properties_)))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_input_method_library_, SetImeConfig(_, _, _))
      .Times(AnyNumber())
      .WillRepeatedly((Return(true)))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_input_method_library_, RemoveObserver(_))
      .Times(1)
      .RetiresOnSaturation();
}

void CrosInProcessBrowserTest::SetNetworkLibraryStatusAreaExpectations() {
  EXPECT_CALL(*mock_network_library_, AddObserver(_))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_network_library_, wifi_connecting())
      .Times(1)
      .WillRepeatedly((Return(false)))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_network_library_, wifi_connected())
      .Times(1)
      .WillRepeatedly((Return(false)))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_network_library_, cellular_connecting())
      .Times(1)
      .WillRepeatedly((Return(false)))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_network_library_, cellular_connected())
      .Times(1)
      .WillRepeatedly((Return(false)))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_network_library_, ethernet_connected())
      .Times(1)
      .WillRepeatedly((Return(false)))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_network_library_, Connected())
      .Times(1)
      .WillRepeatedly((Return(false)))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_network_library_, Connecting())
      .Times(1)
      .WillRepeatedly((Return(false)))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_network_library_, RemoveObserver(_))
      .Times(1)
      .RetiresOnSaturation();
}

void CrosInProcessBrowserTest::SetPowerLibraryStatusAreaExpectations() {
  EXPECT_CALL(*mock_power_library_, AddObserver(_))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_power_library_, battery_fully_charged())
      .Times(3)
      .WillRepeatedly((Return(false)))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_power_library_, battery_is_present())
      .Times(1)
      .WillOnce((Return(true)))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_power_library_, battery_percentage())
      .Times(2)
      .WillRepeatedly((Return(42.0)))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_power_library_, line_power_on())
      .Times(4)
      .WillRepeatedly((Return(false)))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_power_library_, battery_time_to_empty())
      .Times(1)
      .WillOnce((Return(base::TimeDelta::FromMinutes(42))))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_power_library_, RemoveObserver(_))
      .Times(1)
      .RetiresOnSaturation();
}

void CrosInProcessBrowserTest::SetSystemLibraryStatusAreaExpectations() {
  EXPECT_CALL(*mock_system_library_, AddObserver(_))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_system_library_, RemoveObserver(_))
      .Times(1)
      .RetiresOnSaturation();
}

void CrosInProcessBrowserTest::SetSynapticsLibraryExpectations() {
  EXPECT_CALL(*mock_synaptics_library_, SetBoolParameter(_, _))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_synaptics_library_, SetRangeParameter(_, _))
      .Times(AnyNumber());
}

void CrosInProcessBrowserTest::SetSystemLibraryExpectations() {
  EXPECT_CALL(*mock_system_library_, GetTimezone())
      .Times(AnyNumber());
  EXPECT_CALL(*mock_system_library_, SetTimezone(_))
      .Times(AnyNumber());
}

void CrosInProcessBrowserTest::TearDownInProcessBrowserTestFixture() {
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
  if (mock_synaptics_library_)
    test_api()->SetSynapticsLibrary(NULL, false);
  if (mock_system_library_)
    test_api()->SetSystemLibrary(NULL, false);
}

}  // namespace chromeos
