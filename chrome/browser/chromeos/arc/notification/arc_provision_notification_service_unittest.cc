// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/notification/arc_provision_notification_service.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/scoped_temp_dir.h"
#include "base/values.h"
#include "chrome/browser/chromeos/arc/arc_auth_notification.h"
#include "chrome/browser/chromeos/arc/arc_optin_uma.h"
#include "chrome/browser/chromeos/arc/arc_session_manager.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_session_manager_client.h"
#include "components/arc/arc_prefs.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/arc_util.h"
#include "components/arc/test/fake_arc_session.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::AnyNumber;
using testing::AtLeast;
using testing::Mock;
using testing::StrictMock;

namespace arc {

namespace {

class MockArcProvisionNotificationServiceDelegate
    : public ArcProvisionNotificationService::Delegate {
 public:
  MOCK_METHOD0(ShowManagedProvisionNotification, void());
  MOCK_METHOD0(RemoveManagedProvisionNotification, void());
};

class ArcProvisionNotificationServiceTest : public testing::Test {
 protected:
  ArcProvisionNotificationServiceTest()
      : user_manager_enabler_(
            std::make_unique<chromeos::FakeChromeUserManager>()) {}

  void SetUp() override {
    chromeos::DBusThreadManager::GetSetterForTesting()->SetSessionManagerClient(
        std::make_unique<chromeos::FakeSessionManagerClient>());
    chromeos::DBusThreadManager::Initialize();

    SetArcAvailableCommandLineForTesting(
        base::CommandLine::ForCurrentProcess());
    ArcSessionManager::DisableUIForTesting();
    ArcAuthNotification::DisableForTesting();

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    TestingProfile::Builder profile_builder;
    profile_builder.SetProfileName("user@gmail.com");
    profile_builder.SetPath(temp_dir_.GetPath().AppendASCII("TestArcProfile"));
    profile_ = profile_builder.Build();

    arc_service_manager_ = std::make_unique<ArcServiceManager>();
    arc_session_manager_ = std::make_unique<ArcSessionManager>(
        std::make_unique<ArcSessionRunner>(base::Bind(FakeArcSession::Create)));
    std::unique_ptr<MockArcProvisionNotificationServiceDelegate>
        mock_arc_provision_notification_service_delegate = std::make_unique<
            StrictMock<MockArcProvisionNotificationServiceDelegate>>();
    arc_provision_notification_service_delegate_ =
        mock_arc_provision_notification_service_delegate.get();
    arc_provision_notification_service_ =
        ArcProvisionNotificationService::CreateForTesting(
            profile_.get(),
            std::move(mock_arc_provision_notification_service_delegate));

    const AccountId account_id(AccountId::FromUserEmailGaiaId(
        profile()->GetProfileUserName(), "1234567890"));
    GetFakeUserManager()->AddUser(account_id);
    GetFakeUserManager()->LoginUser(account_id);
  }

  void TearDown() override {
    EXPECT_CALL(*arc_provision_notification_service_delegate(),
                RemoveManagedProvisionNotification())
        .Times(AtLeast(1));
    arc_provision_notification_service_.reset();
    arc_provision_notification_service_delegate_ = nullptr;
    arc_session_manager_.reset();
    arc_service_manager_.reset();
    profile_.reset();
    chromeos::DBusThreadManager::Shutdown();
  }

  TestingProfile* profile() { return profile_.get(); }
  ArcServiceManager* arc_service_manager() {
    return arc_service_manager_.get();
  }
  ArcSessionManager* arc_session_manager() {
    return arc_session_manager_.get();
  }
  MockArcProvisionNotificationServiceDelegate*
  arc_provision_notification_service_delegate() {
    return arc_provision_notification_service_delegate_;
  }
  ArcProvisionNotificationService* arc_provision_notification_service() {
    return arc_provision_notification_service_.get();
  }
  chromeos::FakeChromeUserManager* GetFakeUserManager() {
    return static_cast<chromeos::FakeChromeUserManager*>(
        user_manager::UserManager::Get());
  }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  user_manager::ScopedUserManager user_manager_enabler_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<ArcServiceManager> arc_service_manager_;
  std::unique_ptr<ArcSessionManager> arc_session_manager_;
  std::unique_ptr<ArcProvisionNotificationService>
      arc_provision_notification_service_;
  MockArcProvisionNotificationServiceDelegate*
      arc_provision_notification_service_delegate_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ArcProvisionNotificationServiceTest);
};

}  // namespace

// The managed provision notification is displayed from the beginning of the
// silent opt-in till its successful finish.
TEST_F(ArcProvisionNotificationServiceTest,
       ManagedProvisionNotification_Basic) {
  // Set up managed ARC and assign managed values to all opt-in prefs.
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kArcEnabled, std::make_unique<base::Value>(true));
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kArcBackupRestoreEnabled, std::make_unique<base::Value>(false));
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kArcLocationServiceEnabled, std::make_unique<base::Value>(false));

  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();

  // Trigger opt-in flow. The notification gets shown.
  EXPECT_CALL(*arc_provision_notification_service_delegate(),
              ShowManagedProvisionNotification());
  arc_session_manager()->RequestEnable();
  Mock::VerifyAndClearExpectations(
      arc_provision_notification_service_delegate());
  EXPECT_EQ(ArcSessionManager::State::CHECKING_ANDROID_MANAGEMENT,
            arc_session_manager()->state());
  arc_session_manager()->StartArcForTesting();
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());

  // Emulate successful provisioning. The notification gets removed.
  EXPECT_CALL(*arc_provision_notification_service_delegate(),
              RemoveManagedProvisionNotification())
      .Times(AtLeast(1));
  arc_session_manager()->OnProvisioningFinished(ProvisioningResult::SUCCESS);
  Mock::VerifyAndClearExpectations(
      arc_provision_notification_service_delegate());
}

// The managed provision notification is not displayed after the restart if the
// provisioning was successful.
TEST_F(ArcProvisionNotificationServiceTest,
       ManagedProvisionNotification_Restart) {
  // No notifications are expected to be shown in this test, but the delegate
  // method that removes the notification is allowed to be called.
  EXPECT_CALL(*arc_provision_notification_service_delegate(),
              RemoveManagedProvisionNotification())
      .Times(AnyNumber());

  // Set up managed ARC and assign managed values to all opt-in prefs.
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kArcEnabled, std::make_unique<base::Value>(true));
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kArcBackupRestoreEnabled, std::make_unique<base::Value>(false));
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kArcLocationServiceEnabled, std::make_unique<base::Value>(false));
  // Set the pref that indicates that signing into ARC has already been
  // performed.
  profile()->GetPrefs()->SetBoolean(prefs::kArcSignedIn, true);

  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();

  // Enable ARC. The opt-in flow doesn't take place, and no notification is
  // shown.
  arc_session_manager()->RequestEnable();
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());
  arc_session_manager()->OnProvisioningFinished(ProvisioningResult::SUCCESS);
}

// The managed provision notification is displayed from the beginning of the
// silent opt-in till the failure of the provision.
TEST_F(ArcProvisionNotificationServiceTest,
       ManagedProvisionNotification_Failure) {
  // Set up managed ARC and assign managed values to all opt-in prefs.
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kArcEnabled, std::make_unique<base::Value>(true));
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kArcBackupRestoreEnabled, std::make_unique<base::Value>(false));
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kArcLocationServiceEnabled, std::make_unique<base::Value>(false));

  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();

  // Trigger opt-in flow. The notification gets shown.
  EXPECT_CALL(*arc_provision_notification_service_delegate(),
              ShowManagedProvisionNotification());
  arc_session_manager()->RequestEnable();
  Mock::VerifyAndClearExpectations(
      arc_provision_notification_service_delegate());
  EXPECT_EQ(ArcSessionManager::State::CHECKING_ANDROID_MANAGEMENT,
            arc_session_manager()->state());
  arc_session_manager()->StartArcForTesting();
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());

  // Emulate provisioning failure that leads to stopping ARC. The notification
  // gets removed.
  EXPECT_CALL(*arc_provision_notification_service_delegate(),
              RemoveManagedProvisionNotification())
      .Times(AtLeast(1));
  arc_session_manager()->OnProvisioningFinished(
      ProvisioningResult::CHROME_SERVER_COMMUNICATION_ERROR);
  Mock::VerifyAndClearExpectations(
      arc_provision_notification_service_delegate());
}

// The managed provision notification is displayed from the beginning of the
// silent opt-in till the failure of the provision.
TEST_F(ArcProvisionNotificationServiceTest,
       ManagedProvisionNotification_FailureNotStopping) {
  // Set up managed ARC and assign managed values to all opt-in prefs.
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kArcEnabled, std::make_unique<base::Value>(true));
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kArcBackupRestoreEnabled, std::make_unique<base::Value>(false));
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kArcLocationServiceEnabled, std::make_unique<base::Value>(false));

  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();

  // Trigger opt-in flow. The notification gets shown.
  EXPECT_CALL(*arc_provision_notification_service_delegate(),
              ShowManagedProvisionNotification());
  arc_session_manager()->RequestEnable();
  Mock::VerifyAndClearExpectations(
      arc_provision_notification_service_delegate());
  EXPECT_EQ(ArcSessionManager::State::CHECKING_ANDROID_MANAGEMENT,
            arc_session_manager()->state());
  arc_session_manager()->StartArcForTesting();
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());

  // Emulate provisioning failure that leads to showing an error screen without
  // shutting ARC down. The notification gets removed.
  EXPECT_CALL(*arc_provision_notification_service_delegate(),
              RemoveManagedProvisionNotification())
      .Times(AtLeast(1));
  arc_session_manager()->OnProvisioningFinished(
      ProvisioningResult::NO_NETWORK_CONNECTION);
  Mock::VerifyAndClearExpectations(
      arc_provision_notification_service_delegate());
}

// The managed provision notification is not displayed when opt-in prefs are not
// managed.
TEST_F(ArcProvisionNotificationServiceTest,
       ManagedProvisionNotification_NotSilent) {
  // No notifications are expected to be shown in this test, but the delegate
  // method that removes the notification is allowed to be called.
  EXPECT_CALL(*arc_provision_notification_service_delegate(),
              RemoveManagedProvisionNotification())
      .Times(AnyNumber());

  // Set ARC to be managed.
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kArcEnabled, std::make_unique<base::Value>(true));

  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();

  // Trigger opt-in flow. The notification is not shown.
  arc_session_manager()->RequestEnable();
  EXPECT_EQ(ArcSessionManager::State::NEGOTIATING_TERMS_OF_SERVICE,
            arc_session_manager()->state());

  // Emulate accepting the terms of service.
  arc_session_manager()->OnTermsOfServiceNegotiatedForTesting(true);
  arc_session_manager()->StartArcForTesting();
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());

  // Emulate successful provisioning.
  arc_session_manager()->OnProvisioningFinished(ProvisioningResult::SUCCESS);
}

}  // namespace arc
