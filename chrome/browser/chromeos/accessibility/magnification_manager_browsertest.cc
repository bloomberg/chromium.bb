// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/accessibility/magnification_manager.h"
#include "chrome/browser/chromeos/cros/cros_in_process_browser_test.h"
#include "chrome/browser/chromeos/cros/cros_mock.h"
#include "chrome/browser/chromeos/cros/mock_network_library.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/login/user_manager_impl.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/mock_cros_disks_client.h"
#include "chromeos/dbus/mock_dbus_thread_manager.h"
#include "chromeos/dbus/mock_power_manager_client.h"
#include "chromeos/dbus/mock_session_manager_client.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Return;

namespace chromeos {

class MagnificationManagerTest : public CrosInProcessBrowserTest {
 protected:
  MagnificationManagerTest() {}
  virtual ~MagnificationManagerTest() {}

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    MockDBusThreadManager* mock_dbus_thread_manager =
        new MockDBusThreadManager;
    EXPECT_CALL(*mock_dbus_thread_manager, GetSystemBus())
        .WillRepeatedly(Return(reinterpret_cast<dbus::Bus*>(NULL)));

    DBusThreadManager::InitializeForTesting(mock_dbus_thread_manager);
    CrosInProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    cros_mock_->InitStatusAreaMocks();
    cros_mock_->SetStatusAreaMocksExpectations();

    MockNetworkLibrary* mock_network_library_ =
        cros_mock_->mock_network_library();
    EXPECT_CALL(*mock_network_library_, AddUserActionObserver(_))
        .Times(AnyNumber());
    EXPECT_CALL(*mock_network_library_, LoadOncNetworks(_, _, _, _, _))
        .WillRepeatedly(Return(true));

    MockSessionManagerClient* mock_session_manager_client =
        mock_dbus_thread_manager->mock_session_manager_client();
    EXPECT_CALL(*mock_session_manager_client, RetrieveDevicePolicy(_))
        .Times(AnyNumber());
    EXPECT_CALL(*mock_session_manager_client, HasObserver(_))
        .WillRepeatedly(Return(false));

    MockPowerManagerClient* mock_update_engine_client =
        mock_dbus_thread_manager->mock_power_manager_client();
    EXPECT_CALL(*mock_update_engine_client, HasObserver(_))
        .WillRepeatedly(Return(false));

    MockCrosDisksClient* mock_cros_disks_client =
        mock_dbus_thread_manager->mock_cros_disks_client();
    EXPECT_CALL(*mock_cros_disks_client, EnumerateAutoMountableDevices(_, _))
        .Times(AnyNumber());
    EXPECT_CALL(*mock_cros_disks_client, SetUpConnections(_, _))
        .Times(AnyNumber());
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(switches::kLoginManager);
    command_line->AppendSwitchASCII(switches::kLoginProfile,
                                    TestingProfile::kTestUserProfileDir);
  }

  Profile* profile() {
    Profile* profile = ProfileManager::GetDefaultProfileOrOffTheRecord();
    DCHECK(profile);
    return profile;
  }

  PrefServiceBase* prefs() {
    return PrefServiceBase::FromBrowserContext(profile());
  }

  DISALLOW_COPY_AND_ASSIGN(MagnificationManagerTest);
};

IN_PROC_BROWSER_TEST_F(MagnificationManagerTest, Login) {
  // Confirms that magnifier is enabled on the login screen.
  EXPECT_TRUE(MagnificationManager::GetInstance()->IsEnabled());

  // Logs in.
  UserManager::Get()->UserLoggedIn("owner@invalid.domain", true);
  UserManager::Get()->SessionStarted();

  // Confirms that magnifier is disabled just after login.
  EXPECT_FALSE(MagnificationManager::GetInstance()->IsEnabled());

  // Enables magnifier.
  MagnificationManager::GetInstance()->SetEnabled(true);

  // Confirms that magnifier is enabled.
  EXPECT_TRUE(MagnificationManager::GetInstance()->IsEnabled());
}

IN_PROC_BROWSER_TEST_F(MagnificationManagerTest, WorkingWithPref) {
  // Logs in
  UserManager::Get()->UserLoggedIn("owner@invalid.domain", true);
  UserManager::Get()->SessionStarted();

  // Confirms that magnifier is disabled just after login.
  EXPECT_FALSE(MagnificationManager::GetInstance()->IsEnabled());

  // Sets the pref as true to enable magnifier.
  prefs()->SetBoolean(prefs::kScreenMagnifierEnabled, true);

  // Confirms that magnifier is enabled.
  EXPECT_TRUE(MagnificationManager::GetInstance()->IsEnabled());

  // Sets the pref as false to disabled magnifier.
  prefs()->SetBoolean(prefs::kScreenMagnifierEnabled, false);

  // Confirms that magnifier is disabled.
  EXPECT_FALSE(MagnificationManager::GetInstance()->IsEnabled());

  // Sets the pref as true to enable magnifier again.
  prefs()->SetBoolean(prefs::kScreenMagnifierEnabled, true);

  // Confirms that magnifier is enabled.
  EXPECT_TRUE(MagnificationManager::GetInstance()->IsEnabled());
}

IN_PROC_BROWSER_TEST_F(MagnificationManagerTest, ResumeSavedPref) {
  // Loads the profile of the user.
  UserManager::Get()->UserLoggedIn("owner@invalid.domain", true);

  // Sets the pref as true to enable magnifier before login.
  prefs()->SetBoolean(prefs::kScreenMagnifierEnabled, true);

  // Logs in.
  UserManager::Get()->SessionStarted();

  // Confirms that magnifier is enabled just after login.
  EXPECT_TRUE(MagnificationManager::GetInstance()->IsEnabled());
}

}  // namespace chromeos
