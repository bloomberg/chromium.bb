// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/cros_in_process_browser_test.h"

#include "base/message_loop.h"
#include "base/ref_counted.h"
#include "base/time.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/login/wizard_screen.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gmock/include/gmock/gmock.h"

static const char kDefaultXKBId[] = "USA";
static const char kDefaultXKBDisplayName[] = "US";

namespace chromeos {
using ::testing::AnyNumber;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::_;

CrosInProcessBrowserTest::CrosInProcessBrowserTest()
    : loader_(NULL),
      mock_language_library_(NULL),
      mock_network_library_(NULL),
      mock_power_library_(NULL),
      mock_synaptics_library_(NULL) {}

CrosInProcessBrowserTest::~CrosInProcessBrowserTest() {
}

chromeos::CrosLibrary::TestApi* CrosInProcessBrowserTest::test_api() {
  return chromeos::CrosLibrary::Get()->GetTestApi();
}

void CrosInProcessBrowserTest::InitStatusAreaMocks() {
  InitMockLanguageLibrary();
  InitMockNetworkLibrary();
  InitMockPowerLibrary();
  InitMockSynapticsLibrary();
}

void CrosInProcessBrowserTest::InitMockLibraryLoader() {
  if (loader_)
    return;
  loader_ = new MockLibraryLoader();
  EXPECT_CALL(*loader_, Load(_))
      .Times(AnyNumber())
      .WillRepeatedly(Return(true));
  test_api()->SetLibraryLoader(loader_);
}

void CrosInProcessBrowserTest::InitMockLanguageLibrary() {
  InitMockLibraryLoader();
  if (mock_language_library_)
    return;
  mock_language_library_ = new MockLanguageLibrary();
  test_api()->SetLanguageLibrary(mock_language_library_);
}

void CrosInProcessBrowserTest::InitMockNetworkLibrary() {
  InitMockLibraryLoader();
  if (mock_network_library_)
    return;
  mock_network_library_ = new MockNetworkLibrary();
  test_api()->SetNetworkLibrary(mock_network_library_);
}

void CrosInProcessBrowserTest::InitMockPowerLibrary() {
  InitMockLibraryLoader();
  if (mock_power_library_)
    return;
  mock_power_library_ = new MockPowerLibrary();
  test_api()->SetPowerLibrary(mock_power_library_);
}

void CrosInProcessBrowserTest::InitMockSynapticsLibrary() {
  InitMockLibraryLoader();
  if (mock_synaptics_library_)
    return;
  mock_synaptics_library_ = new MockSynapticsLibrary();
  test_api()->SetSynapticsLibrary(mock_synaptics_library_);
}

void CrosInProcessBrowserTest::SetStatusAreaMocksExpectations() {
  SetLanguageLibraryStatusAreaExpectations();
  SetNetworkLibraryStatusAreaExpectations();
  SetPowerLibraryStatusAreaExpectations();
  SetSynapticsLibraryExpectations();
}

void CrosInProcessBrowserTest::SetLanguageLibraryStatusAreaExpectations() {
  EXPECT_CALL(*mock_language_library_, AddObserver(_))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_language_library_, GetActiveLanguages())
      .Times(1)
      .WillOnce(Return(CreateFallbackInputLanguageList()))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_language_library_, current_ime_properties())
      .Times(1)
      .WillOnce((ReturnRef(ime_properties_)))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_language_library_, current_language())
      .Times(1)
      .WillOnce((ReturnRef(language_)))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_language_library_, RemoveObserver(_))
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

void CrosInProcessBrowserTest::SetSynapticsLibraryExpectations() {
  EXPECT_CALL(*mock_synaptics_library_, SetBoolParameter(_, _))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_synaptics_library_, SetRangeParameter(_, _))
      .Times(AnyNumber());
}

void CrosInProcessBrowserTest::TearDownInProcessBrowserTestFixture() {
  // Prevent bogus gMock leak check from firing.
  if (loader_)
    test_api()->SetLibraryLoader(NULL);
  if (mock_language_library_)
    test_api()->SetLanguageLibrary(NULL);
  if (mock_network_library_)
    test_api()->SetNetworkLibrary(NULL);
  if (mock_power_library_)
    test_api()->SetPowerLibrary(NULL);
  if (mock_synaptics_library_)
    test_api()->SetSynapticsLibrary(NULL);
}

}  // namespace chromeos
