// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/message_loop.h"
#include "base/scoped_ptr.h"
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
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/login/wizard_in_process_browser_test.h"
#include "chrome/browser/chromeos/login/wizard_screen.h"
#include "chrome/test/ui_test_utils.h"
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
    cros_mock_->test_api()->SetLoginLibrary(mock_login_library_, true);
    EXPECT_CALL(*mock_login_library_, EmitLoginPromptReady())
        .Times(1);
    EXPECT_CALL(*mock_login_library_,RetrieveProperty(_,_,_))
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
    EXPECT_CALL(*mock_network_library_, cellular_connected())
        .Times(1)
        .WillRepeatedly(Return(false));

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
  }

  virtual void TearDownInProcessBrowserTestFixture() {
    CrosInProcessBrowserTest::TearDownInProcessBrowserTestFixture();
    cros_mock_->test_api()->SetLoginLibrary(NULL, false);
  }

  void EmulateContinueButtonExit(NetworkScreen* network_screen) {
    scoped_ptr<MockScreenObserver>
        mock_screen_observer(new MockScreenObserver());
    EXPECT_CALL(*mock_screen_observer,
                OnExit(ScreenObserver::NETWORK_CONNECTED))
        .Times(1);
    EXPECT_CALL(*mock_network_library_, Connected())
        .WillOnce(Return(true));
    controller()->set_observer(mock_screen_observer.get());
    DummyButtonListener button_listener;
    views::TextButton button(&button_listener, L"Button");
    views::MouseEvent event(views::Event::ET_MOUSE_RELEASED,
                            0, 0,
                            views::Event::EF_LEFT_BUTTON_DOWN);
    network_screen->ButtonPressed(&button, event);
    ui_test_utils::RunAllPendingInMessageLoop();
    controller()->set_observer(NULL);
  }

  MockLoginLibrary* mock_login_library_;
  MockNetworkLibrary* mock_network_library_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkScreenTest);
};

IN_PROC_BROWSER_TEST_F(NetworkScreenTest, Ethernet) {
  ASSERT_TRUE(controller());
  NetworkScreen* network_screen = controller()->GetNetworkScreen();
  ASSERT_TRUE(network_screen != NULL);
  ASSERT_EQ(network_screen, controller()->current_screen());

  NetworkSelectionView* network_view = network_screen->view();
  ASSERT_TRUE(network_view != NULL);

  EXPECT_CALL(*mock_network_library_, ethernet_connected())
      .WillOnce((Return(false)));
  EXPECT_CALL(*mock_network_library_, wifi_connected())
      .WillOnce((Return(false)));
  EXPECT_CALL(*mock_network_library_, cellular_connected())
      .WillOnce((Return(false)));
  EXPECT_CALL(*mock_network_library_, ethernet_connecting())
      .WillOnce((Return(true)));
  EXPECT_FALSE(network_view->IsContinueEnabled());
  network_screen->OnNetworkManagerChanged(mock_network_library_);

  EXPECT_CALL(*mock_network_library_, ethernet_connected())
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_network_library_, Connected())
      .Times(2)
      .WillRepeatedly(Return(true));
  EXPECT_FALSE(network_view->IsContinueEnabled());
  EXPECT_FALSE(network_view->IsConnecting());
  network_screen->OnNetworkManagerChanged(mock_network_library_);

  EXPECT_TRUE(network_view->IsContinueEnabled());
  EmulateContinueButtonExit(network_screen);
}

IN_PROC_BROWSER_TEST_F(NetworkScreenTest, Wifi) {
  ASSERT_TRUE(controller());
  NetworkScreen* network_screen = controller()->GetNetworkScreen();
  ASSERT_TRUE(network_screen != NULL);
  ASSERT_EQ(network_screen, controller()->current_screen());

  NetworkSelectionView* network_view = network_screen->view();
  ASSERT_TRUE(network_view != NULL);

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
  WifiNetwork wifi;
  WifiNetworkVector wifi_networks;
  wifi_networks.push_back(&wifi);
  EXPECT_CALL(*mock_network_library_, wifi_network())
      .WillRepeatedly(Return(&wifi));
  EXPECT_CALL(*mock_network_library_, wifi_networks())
      .WillRepeatedly(ReturnRef(wifi_networks));
  EXPECT_FALSE(network_view->IsContinueEnabled());
  network_screen->OnNetworkManagerChanged(mock_network_library_);

  EXPECT_CALL(*mock_network_library_, ethernet_connected())
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_network_library_, Connected())
        .Times(2)
        .WillRepeatedly(Return(true));
  EXPECT_FALSE(network_view->IsContinueEnabled());
  EXPECT_FALSE(network_view->IsConnecting());
  network_screen->OnNetworkManagerChanged(mock_network_library_);

  EXPECT_TRUE(network_view->IsContinueEnabled());
  EmulateContinueButtonExit(network_screen);
}

IN_PROC_BROWSER_TEST_F(NetworkScreenTest, Cellular) {
  ASSERT_TRUE(controller());
  NetworkScreen* network_screen = controller()->GetNetworkScreen();
  ASSERT_TRUE(network_screen != NULL);
  ASSERT_EQ(network_screen, controller()->current_screen());

  NetworkSelectionView* network_view = network_screen->view();
  ASSERT_TRUE(network_view != NULL);

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
  scoped_ptr<CellularNetwork> cellular(new CellularNetwork());
  EXPECT_CALL(*mock_network_library_, cellular_network())
      .WillOnce(Return(cellular.get()));
  EXPECT_FALSE(network_view->IsContinueEnabled());
  network_screen->OnNetworkManagerChanged(mock_network_library_);

  EXPECT_CALL(*mock_network_library_, ethernet_connected())
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_network_library_, Connected())
      .Times(2)
      .WillRepeatedly(Return(true));
  EXPECT_FALSE(network_view->IsContinueEnabled());
  EXPECT_FALSE(network_view->IsConnecting());
  network_screen->OnNetworkManagerChanged(mock_network_library_);

  EXPECT_TRUE(network_view->IsContinueEnabled());
  EmulateContinueButtonExit(network_screen);
}

IN_PROC_BROWSER_TEST_F(NetworkScreenTest, Timeout) {
  ASSERT_TRUE(controller());
  NetworkScreen* network_screen = controller()->GetNetworkScreen();
  ASSERT_TRUE(network_screen != NULL);
  ASSERT_EQ(network_screen, controller()->current_screen());

  NetworkSelectionView* network_view = network_screen->view();
  ASSERT_TRUE(network_view != NULL);

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
  scoped_ptr<WifiNetwork> wifi(new WifiNetwork());
  EXPECT_CALL(*mock_network_library_, wifi_network())
      .WillOnce(Return(wifi.get()));
  EXPECT_FALSE(network_view->IsContinueEnabled());
  network_screen->OnNetworkManagerChanged(mock_network_library_);

  EXPECT_CALL(*mock_network_library_, Connected())
      .Times(2)
      .WillRepeatedly(Return(false));
  EXPECT_FALSE(network_view->IsContinueEnabled());
  EXPECT_FALSE(network_view->IsConnecting());
  network_screen->OnConnectionTimeout();

  // Close infobubble with error message - it makes the test stable.
  EXPECT_FALSE(network_view->IsContinueEnabled());
  EXPECT_FALSE(network_view->IsConnecting());
  network_screen->ClearErrors();
}

}  // namespace chromeos
