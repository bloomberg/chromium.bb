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

namespace chromeos {
using ::testing::AnyNumber;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::_;

CrosInProcessBrowserTest::CrosInProcessBrowserTest() {
}

CrosInProcessBrowserTest::~CrosInProcessBrowserTest() {
}

void CrosInProcessBrowserTest::SetUpInProcessBrowserTestFixture() {
  chromeos::CrosLibrary::TestApi* test_api =
      chromeos::CrosLibrary::Get()->GetTestApi();

  loader_ = new MockLibraryLoader();
  EXPECT_CALL(*loader_, Load(_))
      .Times(AnyNumber())
      .WillRepeatedly(Return(true));

  test_api->SetLibraryLoader(loader_);

  // Create minimal mocks for status bar.
  mock_language_library_ = new MockLanguageLibrary();
  test_api->SetLanguageLibrary(mock_language_library_);
  EXPECT_CALL(*mock_language_library_, AddObserver(_))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_language_library_, GetActiveLanguages())
      .Times(AnyNumber())
      .WillRepeatedly(Return(CreateFallbackInputLanguageList()));
  EXPECT_CALL(*mock_language_library_, current_ime_properties())
      .Times(AnyNumber())
      .WillRepeatedly((ReturnRef(ime_properties_)));
  EXPECT_CALL(*mock_language_library_, current_language())
      .Times(AnyNumber())
      .WillRepeatedly((ReturnRef(language_)));
  EXPECT_CALL(*mock_language_library_, RemoveObserver(_))
      .Times(AnyNumber());

  mock_network_library_ = new MockNetworkLibrary();
  test_api->SetNetworkLibrary(mock_network_library_);
  EXPECT_CALL(*mock_network_library_, AddObserver(_))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_network_library_, wifi_connecting())
      .Times(AnyNumber())
      .WillRepeatedly((Return(false)));
  EXPECT_CALL(*mock_network_library_, wifi_connected())
      .Times(AnyNumber())
      .WillRepeatedly((Return(false)));
  EXPECT_CALL(*mock_network_library_, cellular_connecting())
      .Times(AnyNumber())
      .WillRepeatedly((Return(false)));
  EXPECT_CALL(*mock_network_library_, cellular_connected())
      .Times(AnyNumber())
      .WillRepeatedly((Return(false)));
  EXPECT_CALL(*mock_network_library_, ethernet_connected())
      .Times(AnyNumber())
      .WillRepeatedly((Return(false)));
  EXPECT_CALL(*mock_network_library_, Connected())
      .Times(AnyNumber())
      .WillRepeatedly((Return(false)));
  EXPECT_CALL(*mock_network_library_, Connecting())
      .Times(AnyNumber())
      .WillRepeatedly((Return(false)));
  /*EXPECT_CALL(*mock_network_library_, wifi_networks())
      .Times(AnyNumber())
      .WillRepeatedly((ReturnRef(wifi_networks_)));
  EXPECT_CALL(*mock_network_library_, cellular_networks())
      .Times(AnyNumber())
      .WillRepeatedly((ReturnRef(cellular_networks_)));*/
  EXPECT_CALL(*mock_network_library_, RemoveObserver(_))
      .Times(AnyNumber());

  mock_power_library_ = new MockPowerLibrary();
  test_api->SetPowerLibrary(mock_power_library_);
  EXPECT_CALL(*mock_power_library_, AddObserver(_))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_power_library_, battery_fully_charged())
      .Times(AnyNumber())
      .WillRepeatedly((Return(false)));
  EXPECT_CALL(*mock_power_library_, battery_is_present())
      .Times(AnyNumber())
      .WillRepeatedly((Return(true)));
  EXPECT_CALL(*mock_power_library_, battery_percentage())
      .Times(AnyNumber())
      .WillRepeatedly((Return(42.0)));
  EXPECT_CALL(*mock_power_library_, line_power_on())
      .Times(AnyNumber())
      .WillRepeatedly((Return(false)));
  EXPECT_CALL(*mock_power_library_, battery_time_to_empty())
      .Times(AnyNumber())
      .WillRepeatedly((Return(base::TimeDelta::FromMinutes(42))));
  EXPECT_CALL(*mock_power_library_, RemoveObserver(_))
      .Times(AnyNumber());

  mock_synaptics_library_ = new MockSynapticsLibrary();
  test_api->SetSynapticsLibrary(mock_synaptics_library_);
  EXPECT_CALL(*mock_synaptics_library_, SetBoolParameter(_, _))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_synaptics_library_, SetRangeParameter(_, _))
      .Times(AnyNumber());
}

void CrosInProcessBrowserTest::TearDownInProcessBrowserTestFixture() {
  // Prevent bogus gMock leak check from firing.
  chromeos::CrosLibrary::TestApi* test_api =
      chromeos::CrosLibrary::Get()->GetTestApi();
  test_api->SetLibraryLoader(NULL);
  test_api->SetLanguageLibrary(NULL);
  test_api->SetNetworkLibrary(NULL);
  test_api->SetPowerLibrary(NULL);
  test_api->SetSynapticsLibrary(NULL);
}

}  // namespace chromeos
