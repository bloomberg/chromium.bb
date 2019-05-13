// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/chromeos/login/login_wizard.h"
#include "chrome/browser/chromeos/login/screens/error_screen.h"
#include "chrome/browser/chromeos/login/test/js_checker.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/ui/webui/chromeos/login/error_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/welcome_screen_handler.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/dbus/shill/shill_profile_client.h"
#include "chromeos/network/network_state_test_helper.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace chromeos {

namespace {

constexpr char kWifiServiceName[] = "stub_wifi";
constexpr char kWifiNetworkName[] = "wifi-test-network";

ErrorScreen* GetScreen() {
  return static_cast<ErrorScreen*>(
      WizardController::default_controller()->GetScreen(
          ErrorScreenView::kScreenId));
}

}  // namespace

class NetworkErrorScreenTest : public InProcessBrowserTest {
 public:
  NetworkErrorScreenTest() = default;
  ~NetworkErrorScreenTest() override = default;

  void SetUpOnMainThread() override {
    network_helper_ = std::make_unique<NetworkStateTestHelper>(
        /*use_default_devices_and_services=*/false);
    InProcessBrowserTest::SetUpOnMainThread();

    ShowLoginWizard(WelcomeView::kScreenId);
    OobeScreenWaiter(WelcomeView::kScreenId).Wait();
  }

  void ShowErrorScreenWithNetworkList() {
    // The only reason we set UI state to UI_STATE_UPDATE is to show a list
    // of networks on the error screen. There are other UI states that show
    // the network list, picked one arbitrarily.
    GetScreen()->SetUIState(NetworkError::UI_STATE_UPDATE);

    GetScreen()->Show();

    // Wait until network list adds the wifi test network.
    test::OobeJS()
        .CreateWaiter(WifiElementSelector(kWifiNetworkName) + " != null")
        ->Wait();
  }

  void TearDownOnMainThread() override {
    network_helper_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

 protected:
  std::string WifiElementSelector(const std::string& wifi_network_name) {
    return base::StrCat(
        {"$('offline-network-control').$$('#networkSelect')"
         ".getNetworkListForTest()"
         ".querySelector('cr-network-list-item[aria-label=\"",
         wifi_network_name, "\"]')"});
  }

  void ClickOnWifiNetwork(const std::string& wifi_network_name) {
    test::OobeJS().Evaluate(WifiElementSelector(wifi_network_name) +
                            ".click()");
  }

  void SetUpDisconnectedWifiNetwork() {
    network_helper_->device_test()->ClearDevices();
    network_helper_->service_test()->ClearServices();

    network_helper_->device_test()->AddDevice(
        "/device/stub_wifi_device", shill::kTypeWifi, "stub_wifi_device");
    network_helper_->service_test()->AddService(
        kWifiServiceName, "wifi_guid", kWifiNetworkName, shill::kTypeWifi,
        shill::kStateDisconnect, true);
    network_helper_->service_test()->SetServiceProperty(
        kWifiServiceName, shill::kConnectableProperty, base::Value(true));
    network_helper_->profile_test()->AddService(
        ShillProfileClient::GetSharedProfilePath(), kWifiServiceName);

    // Network modification notifications are posted asynchronously. Wait until
    // idle to ensure observers are notified.
    base::RunLoop().RunUntilIdle();
  }

 private:
  std::unique_ptr<NetworkStateTestHelper> network_helper_;

  DISALLOW_COPY_AND_ASSIGN(NetworkErrorScreenTest);
};

// Test that the network list contains the fake wifi network.
IN_PROC_BROWSER_TEST_F(NetworkErrorScreenTest, ShowsNetwork) {
  SetUpDisconnectedWifiNetwork();
  ShowErrorScreenWithNetworkList();

  test::OobeJS()
      .CreateWaiter(WifiElementSelector(kWifiNetworkName) + ".hidden == false")
      ->Wait();
}

// Test that error screen hides when a network is connected and that showing and
//  hiding the error screen does not modify WizardController's current_screen.
IN_PROC_BROWSER_TEST_F(NetworkErrorScreenTest, SelectNetwork) {
  SetUpDisconnectedWifiNetwork();
  EXPECT_EQ(
      WizardController::default_controller()->current_screen()->screen_id(),
      WelcomeView::kScreenId);

  ShowErrorScreenWithNetworkList();
  EXPECT_EQ(
      WizardController::default_controller()->current_screen()->screen_id(),
      WelcomeView::kScreenId);

  // Go back to welcome screen after hiding the error screen.
  GetScreen()->SetParentScreen(WelcomeView::kScreenId);
  ClickOnWifiNetwork(kWifiNetworkName);

  OobeScreenWaiter welecome_screen_waiter(WelcomeView::kScreenId);
  welecome_screen_waiter.set_assert_next_screen();
  welecome_screen_waiter.Wait();

  EXPECT_EQ(
      WizardController::default_controller()->current_screen()->screen_id(),
      WelcomeView::kScreenId);
}

// Test ConnectRequestCallback is called when connecting to a network.
IN_PROC_BROWSER_TEST_F(NetworkErrorScreenTest, ConnectRequestCallback) {
  SetUpDisconnectedWifiNetwork();

  bool callback_called = false;
  auto subscription = GetScreen()->RegisterConnectRequestCallback(
      base::BindLambdaForTesting([&]() { callback_called = true; }));

  ShowErrorScreenWithNetworkList();
  ClickOnWifiNetwork(kWifiNetworkName);

  EXPECT_TRUE(callback_called);
}

// Test HideCallback is called after screen hides.
IN_PROC_BROWSER_TEST_F(NetworkErrorScreenTest, HideCallback) {
  bool callback_called = false;
  GetScreen()->SetHideCallback(
      base::BindLambdaForTesting([&]() { callback_called = true; }));

  GetScreen()->Show();
  OobeScreenWaiter(ErrorScreenView::kScreenId).Wait();
  GetScreen()->Hide();

  EXPECT_TRUE(callback_called);
}

}  // namespace chromeos
