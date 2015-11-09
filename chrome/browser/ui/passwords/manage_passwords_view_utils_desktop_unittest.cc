// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/manage_passwords_view_utils_desktop.h"

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_entropy_provider.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/profile_sync_service_mock.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "components/browser_sync/browser/profile_sync_service.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/range/range.h"
#include "url/gurl.h"

using ::testing::Return;

namespace {

enum UserType { SMART_LOCK_USER, NON_SMART_LOCK_USER };

const char kPasswordManagerSettingMigrationFieldTrialName[] =
    "PasswordManagerSettingsMigration";
const char kEnabledPasswordManagerSettingsMigrationGroupName[] = "Enable";
const char kDisablePasswordManagerSettingsMigrationGroupName[] = "Disable";

const char kPasswordManagerSettingsBehaviourChangeFieldTrialName[] =
    "PasswordManagerSettingsBehaviourChange";
const char kPasswordManagerSettingsBehaviourChangeEnabledGroupName[] =
    "PasswordManagerSettingsBehaviourChange.Active";
const char kPasswordManagerSettingsBehaviourChangeDisabledGroupName[] =
    "PasswordManagerSettingsBehaviourChange.NotActive";

const char kBrandingExperimentName[] = "PasswordBranding";
const char kSmartLockBrandingGroupName[] = "SmartLockBranding";
const char kSmartLockNoBrandingGroupName[] = "NoSmartLockBranding";

}  // namespace

class ManagePasswordsViewUtilDesktopTest : public testing::Test {
 public:
  ManagePasswordsViewUtilDesktopTest() {}
  ~ManagePasswordsViewUtilDesktopTest() override {}

  void SetUp() override {
    ProfileSyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        &profile_, &ProfileSyncServiceMock::BuildMockProfileSyncService);
  };

  Profile* profile() { return &profile_; }

  void EnforcePasswordManagerSettingMigrationExperiment(const char* name) {
    settings_migration_ = base::FieldTrialList::CreateFieldTrial(
        kPasswordManagerSettingMigrationFieldTrialName, name);
  }

  void EnforcePasswordManagerSettingsBehaviourChangeExperiment(
      const char* name) {
    settings_behaviour_change_ = base::FieldTrialList::CreateFieldTrial(
        kPasswordManagerSettingsBehaviourChangeFieldTrialName, name);
  }

  void EnforceSmartLockBrandingExperiment(const char* name) {
    smart_lock_branding_ =
        base::FieldTrialList::CreateFieldTrial(kBrandingExperimentName, name);
  }

  ProfileSyncService* GetSyncServiceForSmartLockUser() {
    ProfileSyncServiceMock* sync_service = static_cast<ProfileSyncServiceMock*>(
        ProfileSyncServiceFactory::GetInstance()->GetForProfile(&profile_));
    EXPECT_CALL(*sync_service, IsSyncActive()).WillRepeatedly(Return(true));
    EXPECT_CALL(*sync_service, HasSyncSetupCompleted())
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*sync_service, GetActiveDataTypes())
        .WillRepeatedly(Return(syncer::UserSelectableTypes()));
    EXPECT_CALL(*sync_service, IsUsingSecondaryPassphrase())
        .WillRepeatedly(Return(false));
    return sync_service;
  }

  ProfileSyncService* GetSyncServiceForNonSmartLockUser() {
    ProfileSyncServiceMock* sync_service = static_cast<ProfileSyncServiceMock*>(
        ProfileSyncServiceFactory::GetInstance()->GetForProfile(&profile_));
    EXPECT_CALL(*sync_service, IsSyncActive()).WillRepeatedly(Return(false));
    return sync_service;
  }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;
  scoped_refptr<base::FieldTrial> smart_lock_branding_;
  scoped_refptr<base::FieldTrial> settings_migration_;
  scoped_refptr<base::FieldTrial> settings_behaviour_change_;
};

TEST_F(ManagePasswordsViewUtilDesktopTest, GetPasswordManagerSettingsStringId) {
  const struct {
    const char* description;
    const char* smart_lock_branding_experiment_group;
    const char* settings_migration_experiment_group;
    const char* settings_behaviour_change_experiment_group;
    UserType user_type;
    int expected_setting_description_id;
  } kTestData[] = {
      {"Smart Lock User, branding, behavioral change, migration active.",
       kSmartLockBrandingGroupName,
       kEnabledPasswordManagerSettingsMigrationGroupName,
       kPasswordManagerSettingsBehaviourChangeEnabledGroupName, SMART_LOCK_USER,
       IDS_OPTIONS_PASSWORD_MANAGER_SMART_LOCK_ENABLE},
      {"Smart Lock User no branding, behavioral change, migration active.",
       kSmartLockNoBrandingGroupName,
       kEnabledPasswordManagerSettingsMigrationGroupName,
       kPasswordManagerSettingsBehaviourChangeEnabledGroupName, SMART_LOCK_USER,
       IDS_OPTIONS_PASSWORD_MANAGER_SMART_LOCK_ENABLE},
      {"Smart Lock User no branding, no behavioral change, migration active.",
       kSmartLockNoBrandingGroupName,
       kEnabledPasswordManagerSettingsMigrationGroupName,
       kPasswordManagerSettingsBehaviourChangeDisabledGroupName,
       SMART_LOCK_USER, IDS_OPTIONS_PASSWORD_MANAGER_SMART_LOCK_ENABLE},
      {"Smart Lock User no branding, no behavioral change, no migration",
       kSmartLockNoBrandingGroupName,
       kDisablePasswordManagerSettingsMigrationGroupName,
       kPasswordManagerSettingsBehaviourChangeDisabledGroupName,
       SMART_LOCK_USER, IDS_OPTIONS_PASSWORD_MANAGER_ENABLE},
      {"Smart Lock User branding, no behavioral change, no migration",
       kSmartLockBrandingGroupName,
       kDisablePasswordManagerSettingsMigrationGroupName,
       kPasswordManagerSettingsBehaviourChangeDisabledGroupName,
       SMART_LOCK_USER, IDS_OPTIONS_PASSWORD_MANAGER_SMART_LOCK_ENABLE},
      {"Non Smart Lock User, no behavioral change, no migration",
       kSmartLockNoBrandingGroupName,
       kDisablePasswordManagerSettingsMigrationGroupName,
       kPasswordManagerSettingsBehaviourChangeDisabledGroupName,
       NON_SMART_LOCK_USER, IDS_OPTIONS_PASSWORD_MANAGER_ENABLE},
      {"Non Smart Lock User, no behavioral change, migration",
       kSmartLockNoBrandingGroupName,
       kEnabledPasswordManagerSettingsMigrationGroupName,
       kPasswordManagerSettingsBehaviourChangeDisabledGroupName,
       NON_SMART_LOCK_USER, IDS_OPTIONS_PASSWORD_MANAGER_ENABLE},
      {"Non Smart Lock User, behavioral change, migration",
       kSmartLockNoBrandingGroupName,
       kEnabledPasswordManagerSettingsMigrationGroupName,
       kPasswordManagerSettingsBehaviourChangeEnabledGroupName,
       NON_SMART_LOCK_USER, IDS_OPTIONS_PASSWORD_MANAGER_ENABLE},
  };

  for (const auto& test_case : kTestData) {
    base::FieldTrialList field_trial_list(new base::MockEntropyProvider());
    SCOPED_TRACE(testing::Message(test_case.description));
    ProfileSyncService* sync_service;
    if (test_case.user_type == SMART_LOCK_USER)
      sync_service = GetSyncServiceForSmartLockUser();
    else
      sync_service = GetSyncServiceForNonSmartLockUser();
    EnforceSmartLockBrandingExperiment(
        test_case.smart_lock_branding_experiment_group);
    EnforcePasswordManagerSettingMigrationExperiment(
        test_case.settings_migration_experiment_group);
    EnforcePasswordManagerSettingsBehaviourChangeExperiment(
        test_case.settings_behaviour_change_experiment_group);
    EXPECT_EQ(
        l10n_util::GetStringUTF16(test_case.expected_setting_description_id),
        l10n_util::GetStringUTF16(
            GetPasswordManagerSettingsStringId(sync_service)));
  }
}
