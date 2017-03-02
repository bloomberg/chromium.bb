// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/status/data_promo_notification.h"

#include "base/command_line.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/login/users/scoped_user_manager_enabler.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_device_client.h"
#include "chromeos/dbus/shill_service_client.h"
#include "chromeos/login/login_state.h"
#include "chromeos/network/network_connect.h"
#include "chromeos/network/network_state_handler.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/platform_test.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/message_center/message_center.h"

namespace {

const char kCellularDevicePath[] = "/device/stub_cellular_device1";
const char kCellularServicePath[] = "/service/cellular1";
const char kCellularGuid[] = "cellular1_guid";
const char kNotificationId[] = "chrome://settings/internet/data_saver";
const char kTestUserName[] = "test-user@example.com";

class NetworkConnectTestDelegate : public chromeos::NetworkConnect::Delegate {
 public:
  NetworkConnectTestDelegate() {}
  ~NetworkConnectTestDelegate() override {}

  void ShowNetworkConfigure(const std::string& network_id) override {}
  void ShowNetworkSettings(const std::string& network_id) override {}
  bool ShowEnrollNetwork(const std::string& network_id) override {
    return false;
  }
  void ShowMobileSimDialog() override {}
  void ShowMobileSetupDialog(const std::string& network_id) override {}
  void ShowNetworkConnectError(const std::string& error_name,
                               const std::string& network_id) override {}
  void ShowMobileActivationError(const std::string& network_id) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkConnectTestDelegate);
};

}  // namespace

namespace chromeos {
namespace test {

class DataPromoNotificationTest : public testing::Test {
 public:
  DataPromoNotificationTest() {}
  ~DataPromoNotificationTest() override {}

  void SetUp() override {
    testing::Test::SetUp();
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableDataSaverPrompt);
    DBusThreadManager::Initialize();
    NetworkHandler::Initialize();
    data_promo_notification_.reset(new DataPromoNotification);
    SetupUser();
    SetupNetworkShillState();
    message_center::MessageCenter::Initialize();
    base::RunLoop().RunUntilIdle();
    network_connect_delegate_.reset(new NetworkConnectTestDelegate);
    chromeos::NetworkConnect::Initialize(network_connect_delegate_.get());
  }

  void TearDown() override {
    chromeos::NetworkConnect::Shutdown();
    network_connect_delegate_.reset();
    message_center::MessageCenter::Shutdown();
    LoginState::Shutdown();
    profile_manager_.reset();
    user_manager_enabler_.reset();
    data_promo_notification_.reset();
    NetworkHandler::Shutdown();
    DBusThreadManager::Shutdown();
    testing::Test::TearDown();
  }

 protected:
  void SetupUser() {
    std::unique_ptr<FakeChromeUserManager> user_manager(
        new FakeChromeUserManager());
    const AccountId test_account_id(AccountId::FromUserEmail(kTestUserName));
    user_manager->AddUser(test_account_id);
    user_manager->LoginUser(test_account_id);
    user_manager_enabler_.reset(
        new ScopedUserManagerEnabler(user_manager.release()));

    profile_manager_.reset(
        new TestingProfileManager(TestingBrowserProcess::GetGlobal()));
    ASSERT_TRUE(profile_manager_->SetUp());
    profile_manager_->SetLoggedIn(true);
    ASSERT_TRUE(user_manager::UserManager::Get()->GetPrimaryUser());

    LoginState::Initialize();
    LoginState::Get()->SetLoggedInState(LoginState::LOGGED_IN_ACTIVE,
                                        LoginState::LOGGED_IN_USER_REGULAR);
  }

  void SetupNetworkShillState() {
    base::RunLoop().RunUntilIdle();

    // Create a cellular device with provider.
    ShillDeviceClient::TestInterface* device_test =
        DBusThreadManager::Get()->GetShillDeviceClient()->GetTestInterface();
    device_test->ClearDevices();
    device_test->AddDevice(kCellularDevicePath, shill::kTypeCellular,
                           "stub_cellular_device1");
    base::DictionaryValue home_provider;
    home_provider.SetString("name", "Cellular1_Provider");
    home_provider.SetString("country", "us");
    device_test->SetDeviceProperty(kCellularDevicePath,
                                   shill::kHomeProviderProperty, home_provider);

    // Create a cellular network and activate it.
    ShillServiceClient::TestInterface* service_test =
        DBusThreadManager::Get()->GetShillServiceClient()->GetTestInterface();
    service_test->ClearServices();
    service_test->AddService(kCellularServicePath, kCellularGuid,
                             "cellular1" /* name */, shill::kTypeCellular,
                             "activated", true /* visible */);
    service_test->SetServiceProperty(
        kCellularServicePath, shill::kActivationStateProperty,
        base::StringValue(shill::kActivationStateActivated));
    service_test->SetServiceProperty(
        kCellularServicePath, shill::kConnectableProperty, base::Value(true));
  }

  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<DataPromoNotification> data_promo_notification_;
  std::unique_ptr<NetworkConnectTestDelegate> network_connect_delegate_;
  std::unique_ptr<ScopedUserManagerEnabler> user_manager_enabler_;
  std::unique_ptr<TestingProfileManager> profile_manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DataPromoNotificationTest);
};

TEST_F(DataPromoNotificationTest, DataSaverNotification) {
  message_center::MessageCenter* message_center =
      message_center::MessageCenter::Get();

  // Network setup shouldn't be enough to activate notification.
  EXPECT_FALSE(message_center->FindVisibleNotificationById(kNotificationId));

  chromeos::NetworkConnect::Get()->ConnectToNetworkId(kCellularGuid);
  base::RunLoop().RunUntilIdle();
  // Connecting to cellular network (which here makes it the default network)
  // should trigger the Data Saver notification.
  EXPECT_TRUE(message_center->FindVisibleNotificationById(kNotificationId));
}

}  // namespace test
}  // namespace chromeos
