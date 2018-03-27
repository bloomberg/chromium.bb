// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/system_tray_client.h"

#include "ash/ash_view_ids.h"
#include "ash/public/interfaces/constants.mojom.h"
#include "ash/public/interfaces/system_tray_test_api.mojom.h"
#include "base/i18n/time_formatting.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/ui/user_adding_screen.h"
#include "chrome/browser/chromeos/policy/device_policy_cros_browser_test.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/upgrade_detector_chromeos.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/upgrade_detector.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_update_engine_client.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/account_id/account_id.h"
#include "components/user_manager/user_manager.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/test/test_utils.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/views/controls/label.h"

using chromeos::ProfileHelper;
using user_manager::UserManager;

class SystemTrayClientTest : public InProcessBrowserTest {
 public:
  SystemTrayClientTest() = default;
  ~SystemTrayClientTest() override = default;

  void SetUp() override {
    std::unique_ptr<chromeos::DBusThreadManagerSetter> dbus_setter =
        chromeos::DBusThreadManager::GetSetterForTesting();
    fake_update_engine_client_ = new chromeos::FakeUpdateEngineClient();
    dbus_setter->SetUpdateEngineClient(
        std::unique_ptr<chromeos::UpdateEngineClient>(
            fake_update_engine_client_));
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    // Connect to the ash test interface.
    content::ServiceManagerConnection::GetForProcess()
        ->GetConnector()
        ->BindInterface(ash::mojom::kServiceName, &tray_test_api_);
  }

 protected:
  chromeos::FakeUpdateEngineClient* fake_update_engine_client_ = nullptr;
  ash::mojom::SystemTrayTestApiPtr tray_test_api_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SystemTrayClientTest);
};

// Test that a chrome update shows the update icon in the system menu.
IN_PROC_BROWSER_TEST_F(SystemTrayClientTest, UpdateTrayIcon) {
  ash::mojom::SystemTrayTestApiAsyncWaiter wait_for(tray_test_api_.get());

  // When no update is pending, the icon isn't visible.
  bool visible = false;
  wait_for.IsTrayViewVisible(ash::VIEW_ID_TRAY_UPDATE_ICON, &visible);
  EXPECT_FALSE(visible);

  // Simulate an upgrade. This sends a mojo message to ash.
  UpgradeDetector* detector = UpgradeDetector::GetInstance();
  detector->set_upgrade_notification_stage(
      UpgradeDetector::UPGRADE_ANNOYANCE_LOW);
  detector->NotifyUpgrade();
  content::RunAllPendingInMessageLoop();

  // Tray icon is now visible.
  wait_for.IsTrayViewVisible(ash::VIEW_ID_TRAY_UPDATE_ICON, &visible);
  EXPECT_TRUE(visible);
}

// Tests that the update icon becomes visible after an update is detected
// available for downloading over cellular connection. The update icon hides
// after user's one time permission on the update is set successfully in Update
// Engine.
IN_PROC_BROWSER_TEST_F(SystemTrayClientTest, UpdateOverCellularTrayIcon) {
  ash::mojom::SystemTrayTestApiAsyncWaiter wait_for(tray_test_api_.get());
  wait_for.DisableAnimations();

  // When no update is pending, the icon isn't visible.
  bool visible = false;
  wait_for.IsTrayViewVisible(ash::VIEW_ID_TRAY_UPDATE_ICON, &visible);
  EXPECT_FALSE(visible);

  chromeos::UpdateEngineClient::Status status;
  status.status =
      chromeos::UpdateEngineClient::UPDATE_STATUS_NEED_PERMISSION_TO_UPDATE;
  fake_update_engine_client_->set_default_status(status);
  fake_update_engine_client_->NotifyObserversThatStatusChanged(status);
  content::RunAllPendingInMessageLoop();

  // When an update is available over cellular networks, the icon is visible.
  wait_for.IsTrayViewVisible(ash::VIEW_ID_TRAY_UPDATE_ICON, &visible);
  EXPECT_TRUE(visible);

  wait_for.ShowBubble();
  base::string16 label;
  wait_for.GetBubbleLabelText(ash::VIEW_ID_TRAY_UPDATE_MENU_LABEL, &label);
  EXPECT_EQ("Click to view update details", base::UTF16ToUTF8(label));

  // Notifies that the user's one time permission on update over cellular
  // connection is granted.
  fake_update_engine_client_
      ->NotifyUpdateOverCellularOneTimePermissionGranted();
  content::RunAllPendingInMessageLoop();

  // When user permission is granted, the icon becomes invisible.
  wait_for.IsTrayViewVisible(ash::VIEW_ID_TRAY_UPDATE_ICON, &visible);
  EXPECT_FALSE(visible);
}

// Test that a flash update causes the update UI to show in the system menu.
IN_PROC_BROWSER_TEST_F(SystemTrayClientTest, FlashUpdateTrayIcon) {
  ash::mojom::SystemTrayTestApiAsyncWaiter wait_for(tray_test_api_.get());

  // When no update is pending, the icon isn't visible.
  bool visible = false;
  wait_for.IsTrayViewVisible(ash::VIEW_ID_TRAY_UPDATE_ICON, &visible);
  EXPECT_FALSE(visible);

  // Simulate a Flash update. This sends a mojo message to ash.
  SystemTrayClient::Get()->SetFlashUpdateAvailable();
  content::RunAllPendingInMessageLoop();

  // Tray icon is now visible.
  wait_for.IsTrayViewVisible(ash::VIEW_ID_TRAY_UPDATE_ICON, &visible);
  EXPECT_TRUE(visible);
}

using SystemTrayClientEnterpriseTest = policy::DevicePolicyCrosBrowserTest;

IN_PROC_BROWSER_TEST_F(SystemTrayClientEnterpriseTest, TrayEnterprise) {
  // Mark the device as enterprise managed.
  policy::DevicePolicyCrosTestHelper::MarkAsEnterpriseOwnedBy("example.com");
  content::RunAllPendingInMessageLoop();

  // Connect to ash.
  ash::mojom::SystemTrayTestApiPtr tray_test_api;
  content::ServiceManagerConnection::GetForProcess()
      ->GetConnector()
      ->BindInterface(ash::mojom::kServiceName, &tray_test_api);

  // Open the system tray menu.
  ash::mojom::SystemTrayTestApiAsyncWaiter wait_for(tray_test_api.get());
  wait_for.ShowBubble();

  // Managed devices show an item in the menu.
  bool view_visible = false;
  wait_for.IsBubbleViewVisible(ash::VIEW_ID_TRAY_ENTERPRISE, &view_visible);
  EXPECT_TRUE(view_visible);
}

class SystemTrayClientClockTest : public chromeos::LoginManagerTest {
 public:
  SystemTrayClientClockTest()
      : LoginManagerTest(false /* should_launch_browser */),
        // Use consumer emails to avoid having to fake a policy fetch.
        account_id1_(
            AccountId::FromUserEmailGaiaId("user1@gmail.com", "1111111111")),
        account_id2_(
            AccountId::FromUserEmailGaiaId("user2@gmail.com", "2222222222")) {}

  ~SystemTrayClientClockTest() override = default;

  void SetupUserProfile(const AccountId& account_id, bool use_24_hour_clock) {
    const user_manager::User* user = UserManager::Get()->FindUser(account_id);
    Profile* profile = ProfileHelper::Get()->GetProfileByUser(user);
    profile->GetPrefs()->SetBoolean(prefs::kUse24HourClock, use_24_hour_clock);
    // Allow clock setting to be sent to ash over mojo.
    content::RunAllPendingInMessageLoop();
  }

 protected:
  const AccountId account_id1_;
  const AccountId account_id2_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SystemTrayClientClockTest);
};

IN_PROC_BROWSER_TEST_F(SystemTrayClientClockTest,
                       PRE_TestMultiProfile24HourClock) {
  RegisterUser(account_id1_);
  RegisterUser(account_id2_);
  chromeos::StartupUtils::MarkOobeCompleted();
}

// Test that clock type is taken from user profile for current active user.
IN_PROC_BROWSER_TEST_F(SystemTrayClientClockTest, TestMultiProfile24HourClock) {
  // Connect to ash.
  ash::mojom::SystemTrayTestApiPtr tray_test_api;
  content::ServiceManagerConnection::GetForProcess()
      ->GetConnector()
      ->BindInterface(ash::mojom::kServiceName, &tray_test_api);
  ash::mojom::SystemTrayTestApiAsyncWaiter wait_for(tray_test_api.get());

  // Login a user with a 24-hour clock.
  LoginUser(account_id1_);
  SetupUserProfile(account_id1_, true /* use_24_hour_clock */);
  bool is_24_hour = false;
  wait_for.Is24HourClock(&is_24_hour);
  EXPECT_TRUE(is_24_hour);

  // Add a user with a 12-hour clock.
  chromeos::UserAddingScreen::Get()->Start();
  content::RunAllPendingInMessageLoop();
  AddUser(account_id2_);
  SetupUserProfile(account_id2_, false /* use_24_hour_clock */);
  wait_for.Is24HourClock(&is_24_hour);
  EXPECT_FALSE(is_24_hour);

  // Switch back to the user with the 24-hour clock.
  UserManager::Get()->SwitchActiveUser(account_id1_);
  // Allow clock setting to be sent to ash over mojo.
  content::RunAllPendingInMessageLoop();
  wait_for.Is24HourClock(&is_24_hour);
  EXPECT_TRUE(is_24_hour);
}
