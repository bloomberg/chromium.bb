// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/string16.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/mock_login_library.h"
#include "chrome/browser/chromeos/cros/mock_network_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/login/mock_screen_observer.h"
#include "chrome/browser/chromeos/login/network_screen.h"
#include "chrome/browser/chromeos/login/network_selection_view.h"
#include "chrome/browser/chromeos/login/view_screen.h"
#include "chrome/browser/chromeos/login/views_network_screen_actor.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/login/wizard_in_process_browser_test.h"
#include "chrome/browser/chromeos/login/wizard_screen.h"
#include "chrome/test/base/ui_test_utils.h"
#include "grit/generated_resources.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "views/controls/button/text_button.h"

namespace chromeos {
using ::testing::AnyNumber;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::_;
using ::testing::A;
using views::Button;

class DummyButtonListener : public views::ButtonListener {
 public:
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) {}
};

class NetworkScreenTest : public WizardInProcessBrowserTest {
 public:
  NetworkScreenTest(): WizardInProcessBrowserTest("network"),
                       mock_login_library_(NULL),
                       mock_network_library_(NULL) {
  }

 protected:
  virtual void SetUpInProcessBrowserTestFixture() {
    cros_mock_->InitStatusAreaMocks();
    mock_network_library_ = cros_mock_->mock_network_library();
    mock_login_library_ = new MockLoginLibrary();
    cellular_.reset(new NetworkDevice("cellular"));
    cros_mock_->test_api()->SetLoginLibrary(mock_login_library_, true);
    EXPECT_CALL(*mock_login_library_, EmitLoginPromptReady())
        .Times(1);
    EXPECT_CALL(*mock_login_library_,RequestRetrievePolicy(_,_))
        .Times(AnyNumber());

    // Minimal set of expectations needed on NetworkScreen initialization.
    // Status bar expectations are defined with RetiresOnSaturation() so
    // these mocks will be active once status bar is initialized.
    EXPECT_CALL(*mock_network_library_, ethernet_connected())
        .Times(2)
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*mock_network_library_, ethernet_connecting())
        .Times(2)
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*mock_network_library_, wifi_connected())
        .Times(1)
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*mock_network_library_, FindWifiDevice())
        .Times(AnyNumber());
    EXPECT_CALL(*mock_network_library_, FindEthernetDevice())
        .Times(AnyNumber());

    cros_mock_->SetStatusAreaMocksExpectations();

    // Override these return values, but do not set specific expectation:
    EXPECT_CALL(*mock_network_library_, wifi_available())
        .Times(AnyNumber())
        .WillRepeatedly((Return(true)));
    EXPECT_CALL(*mock_network_library_, wifi_enabled())
        .Times(AnyNumber())
        .WillRepeatedly((Return(true)));
    EXPECT_CALL(*mock_network_library_, wifi_connecting())
        .Times(AnyNumber())
        .WillRepeatedly((Return(false)));
    EXPECT_CALL(*mock_network_library_, wifi_scanning())
        .Times(AnyNumber())
        .WillRepeatedly((Return(false)));
    EXPECT_CALL(*mock_network_library_, cellular_available())
        .Times(AnyNumber())
        .WillRepeatedly((Return(true)));
    EXPECT_CALL(*mock_network_library_, cellular_enabled())
        .Times(AnyNumber())
        .WillRepeatedly((Return(true)));
    EXPECT_CALL(*mock_network_library_, cellular_connecting())
        .Times(AnyNumber())
        .WillRepeatedly((Return(false)));

    EXPECT_CALL(*mock_network_library_, FindCellularDevice())
        .Times(AnyNumber())
        .WillRepeatedly((Return(cellular_.get())));
  }

  virtual void SetUpOnMainThread() {
    mock_screen_observer_.reset(new MockScreenObserver());
    ASSERT_TRUE(controller() != NULL);
    network_screen_ = controller()->GetNetworkScreen();
    ASSERT_TRUE(network_screen_ != NULL);
    ASSERT_EQ(controller()->current_screen(), network_screen_);
    network_screen_->screen_observer_ = mock_screen_observer_.get();
    // We need a couple of methods defined only in views version.
    // TODO(avayvod): Refactor this when WebUI implementation is finished.
    actor_ = static_cast<ViewsNetworkScreenActor*>(network_screen_->actor());
    ASSERT_TRUE(actor_ != NULL);
  }

  virtual void TearDownInProcessBrowserTestFixture() {
    network_screen_->screen_observer_ = controller();
    CrosInProcessBrowserTest::TearDownInProcessBrowserTestFixture();
    cros_mock_->test_api()->SetLoginLibrary(NULL, false);
  }

  void EmulateContinueButtonExit(NetworkScreen* network_screen) {
    EXPECT_CALL(*mock_screen_observer_,
                OnExit(ScreenObserver::NETWORK_CONNECTED))
        .Times(1);
    EXPECT_CALL(*mock_network_library_, Connected())
        .WillOnce(Return(true));
    network_screen->OnContinuePressed();
    ui_test_utils::RunAllPendingInMessageLoop();
  }

  scoped_ptr<MockScreenObserver> mock_screen_observer_;
  MockLoginLibrary* mock_login_library_;
  MockNetworkLibrary* mock_network_library_;
  scoped_ptr<NetworkDevice> cellular_;
  NetworkScreen* network_screen_;
  ViewsNetworkScreenActor* actor_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkScreenTest);
};

IN_PROC_BROWSER_TEST_F(NetworkScreenTest, Ethernet) {
  EXPECT_CALL(*mock_network_library_, ethernet_connected())
      .WillOnce((Return(false)));
  EXPECT_CALL(*mock_network_library_, wifi_connected())
      .WillOnce((Return(false)));
  EXPECT_CALL(*mock_network_library_, cellular_connected())
      .WillOnce((Return(false)));
  EXPECT_CALL(*mock_network_library_, ethernet_connecting())
      .WillOnce((Return(true)));
  EXPECT_FALSE(actor_->IsContinueEnabled());
  network_screen_->OnNetworkManagerChanged(mock_network_library_);

  EXPECT_CALL(*mock_network_library_, ethernet_connected())
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_network_library_, Connected())
      .Times(3)
      .WillRepeatedly(Return(true));
  EXPECT_FALSE(actor_->IsContinueEnabled());
  EXPECT_FALSE(actor_->IsConnecting());
  network_screen_->OnNetworkManagerChanged(mock_network_library_);

  EXPECT_TRUE(actor_->IsContinueEnabled());
  EmulateContinueButtonExit(network_screen_);
}

IN_PROC_BROWSER_TEST_F(NetworkScreenTest, Wifi) {
  EXPECT_CALL(*mock_network_library_, ethernet_connected())
      .WillOnce((Return(false)));
  EXPECT_CALL(*mock_network_library_, wifi_connected())
      .WillOnce((Return(false)));
  EXPECT_CALL(*mock_network_library_, cellular_connected())
      .WillOnce((Return(false)));
  EXPECT_CALL(*mock_network_library_, ethernet_connecting())
      .WillOnce((Return(false)));
  EXPECT_CALL(*mock_network_library_, wifi_connecting())
      .WillOnce((Return(true)));
  scoped_ptr<WifiNetwork> wifi(new WifiNetwork("wifi"));
  WifiNetworkVector wifi_networks;
  wifi_networks.push_back(wifi.get());
  EXPECT_CALL(*mock_network_library_, wifi_network())
      .WillRepeatedly(Return(wifi.get()));
  EXPECT_CALL(*mock_network_library_, wifi_networks())
      .WillRepeatedly(ReturnRef(wifi_networks));
  EXPECT_FALSE(actor_->IsContinueEnabled());
  network_screen_->OnNetworkManagerChanged(mock_network_library_);

  EXPECT_CALL(*mock_network_library_, ethernet_connected())
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_network_library_, Connected())
        .Times(3)
        .WillRepeatedly(Return(true));
  EXPECT_FALSE(actor_->IsContinueEnabled());
  EXPECT_FALSE(actor_->IsConnecting());
  network_screen_->OnNetworkManagerChanged(mock_network_library_);

  EXPECT_TRUE(actor_->IsContinueEnabled());
  EmulateContinueButtonExit(network_screen_);
}

IN_PROC_BROWSER_TEST_F(NetworkScreenTest, Cellular) {
  EXPECT_CALL(*mock_network_library_, ethernet_connected())
      .WillOnce((Return(false)));
  EXPECT_CALL(*mock_network_library_, wifi_connected())
      .WillOnce((Return(false)));
  EXPECT_CALL(*mock_network_library_, cellular_connected())
      .WillOnce((Return(false)));
  EXPECT_CALL(*mock_network_library_, ethernet_connecting())
      .WillOnce((Return(false)));
  EXPECT_CALL(*mock_network_library_, wifi_connecting())
      .WillOnce((Return(false)));
  EXPECT_CALL(*mock_network_library_, cellular_connecting())
      .WillOnce((Return(true)));
  scoped_ptr<CellularNetwork> cellular(new CellularNetwork("cellular"));
  EXPECT_CALL(*mock_network_library_, cellular_network())
      .WillOnce(Return(cellular.get()));
  EXPECT_FALSE(actor_->IsContinueEnabled());
  network_screen_->OnNetworkManagerChanged(mock_network_library_);

  EXPECT_CALL(*mock_network_library_, ethernet_connected())
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_network_library_, Connected())
      .Times(3)
      .WillRepeatedly(Return(true));
  EXPECT_FALSE(actor_->IsContinueEnabled());
  EXPECT_FALSE(actor_->IsConnecting());
  network_screen_->OnNetworkManagerChanged(mock_network_library_);

  EXPECT_TRUE(actor_->IsContinueEnabled());
  EmulateContinueButtonExit(network_screen_);
}

// See crbug.com/89392
#if defined(OS_LINUX)
#define MAYBE_Timeout DISABLED_Timeout
#else
#define MAYBE_Timeout Timeout
#endif
IN_PROC_BROWSER_TEST_F(NetworkScreenTest, MAYBE_Timeout) {
  EXPECT_CALL(*mock_network_library_, ethernet_connected())
      .WillOnce((Return(false)));
  EXPECT_CALL(*mock_network_library_, wifi_connected())
      .WillOnce((Return(false)));
  EXPECT_CALL(*mock_network_library_, cellular_connected())
      .WillOnce((Return(false)));
  EXPECT_CALL(*mock_network_library_, ethernet_connecting())
      .WillOnce((Return(false)));
  EXPECT_CALL(*mock_network_library_, wifi_connecting())
      .WillOnce((Return(true)));
  scoped_ptr<WifiNetwork> wifi(new WifiNetwork("wifi"));
  EXPECT_CALL(*mock_network_library_, wifi_network())
      .WillOnce(Return(wifi.get()));
  EXPECT_FALSE(actor_->IsContinueEnabled());
  network_screen_->OnNetworkManagerChanged(mock_network_library_);

  EXPECT_CALL(*mock_network_library_, Connected())
      .Times(2)
      .WillRepeatedly(Return(false));
  EXPECT_FALSE(actor_->IsContinueEnabled());
  EXPECT_FALSE(actor_->IsConnecting());
  network_screen_->OnConnectionTimeout();

  // Close infobubble with error message - it makes the test stable.
  EXPECT_FALSE(actor_->IsContinueEnabled());
  EXPECT_FALSE(actor_->IsConnecting());
  actor_->ClearErrors();
}

}  // namespace chromeos
