// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/manage_passwords_view_utils_desktop.h"

#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_entropy_provider.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/profile_sync_test_util.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "components/browser_sync/profile_sync_service.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/range/range.h"
#include "url/gurl.h"

using ::testing::Return;

namespace {

enum UserType { SMART_LOCK_USER, NON_SMART_LOCK_USER };

}  // namespace

class ManagePasswordsViewUtilDesktopTest : public testing::Test {
 public:
  ManagePasswordsViewUtilDesktopTest() {}
  ~ManagePasswordsViewUtilDesktopTest() override {}

  void SetUp() override {
    ProfileSyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        &profile_, &BuildMockProfileSyncService);
  };

  Profile* profile() { return &profile_; }

  browser_sync::ProfileSyncService* GetSyncServiceForSmartLockUser() {
    browser_sync::ProfileSyncServiceMock* sync_service =
        static_cast<browser_sync::ProfileSyncServiceMock*>(
            ProfileSyncServiceFactory::GetInstance()->GetForProfile(&profile_));
    EXPECT_CALL(*sync_service, IsSyncActive()).WillRepeatedly(Return(true));
    EXPECT_CALL(*sync_service, IsFirstSetupComplete())
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*sync_service, GetActiveDataTypes())
        .WillRepeatedly(Return(syncer::UserSelectableTypes()));
    EXPECT_CALL(*sync_service, IsUsingSecondaryPassphrase())
        .WillRepeatedly(Return(false));
    return sync_service;
  }

  browser_sync::ProfileSyncService* GetSyncServiceForNonSmartLockUser() {
    browser_sync::ProfileSyncServiceMock* sync_service =
        static_cast<browser_sync::ProfileSyncServiceMock*>(
            ProfileSyncServiceFactory::GetInstance()->GetForProfile(&profile_));
    EXPECT_CALL(*sync_service, IsSyncActive()).WillRepeatedly(Return(false));
    return sync_service;
  }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;
  scoped_refptr<base::FieldTrial> smart_lock_branding_;
  scoped_refptr<base::FieldTrial> settings_migration_;
};

TEST_F(ManagePasswordsViewUtilDesktopTest, GetPasswordManagerSettingsStringId) {
  const struct {
    UserType user_type;
    int expected_setting_description_id;
  } kTestData[] = {
      {SMART_LOCK_USER, IDS_OPTIONS_PASSWORD_MANAGER_SMART_LOCK_ENABLE},
      {NON_SMART_LOCK_USER, IDS_OPTIONS_PASSWORD_MANAGER_ENABLE},
  };

  for (const auto& test_case : kTestData) {
    browser_sync::ProfileSyncService* sync_service;
    if (test_case.user_type == SMART_LOCK_USER)
      sync_service = GetSyncServiceForSmartLockUser();
    else
      sync_service = GetSyncServiceForNonSmartLockUser();
    EXPECT_EQ(
        l10n_util::GetStringUTF16(test_case.expected_setting_description_id),
        l10n_util::GetStringUTF16(
            GetPasswordManagerSettingsStringId(sync_service)));
  }
}
