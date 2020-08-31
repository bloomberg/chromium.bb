// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/driver/profile_sync_service.h"
#include "components/sync/driver/sync_user_settings.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/sync/test/integration/os_sync_test.h"
#include "chromeos/constants/chromeos_features.h"
#endif

using syncer::UserSelectableType;
using syncer::UserSelectableTypeSet;

namespace {

#if defined(OS_CHROMEOS)
// Chrome OS syncs apps as an OS type.
class SingleClientAppSettingsOsSyncTest : public OsSyncTest {
 public:
  SingleClientAppSettingsOsSyncTest() : OsSyncTest(SINGLE_CLIENT) {}
  ~SingleClientAppSettingsOsSyncTest() override = default;
};

IN_PROC_BROWSER_TEST_F(SingleClientAppSettingsOsSyncTest,
                       DisablingOsSyncFeatureDisablesDataType) {
  ASSERT_TRUE(chromeos::features::IsSplitSettingsSyncEnabled());
  ASSERT_TRUE(SetupSync());
  syncer::ProfileSyncService* service = GetSyncService(0);
  syncer::SyncUserSettings* settings = service->GetUserSettings();

  EXPECT_TRUE(settings->IsOsSyncFeatureEnabled());
  EXPECT_TRUE(service->GetActiveDataTypes().Has(syncer::APP_SETTINGS));

  settings->SetOsSyncFeatureEnabled(false);
  EXPECT_FALSE(settings->IsOsSyncFeatureEnabled());
  EXPECT_FALSE(service->GetActiveDataTypes().Has(syncer::APP_SETTINGS));
}

#else   // !defined(OS_CHROMEOS)

// See also TwoClientExtensionSettingsAndAppSettingsSyncTest.
class SingleClientAppSettingsSyncTest : public SyncTest {
 public:
  SingleClientAppSettingsSyncTest() : SyncTest(SINGLE_CLIENT) {}
  ~SingleClientAppSettingsSyncTest() override = default;
};

IN_PROC_BROWSER_TEST_F(SingleClientAppSettingsSyncTest, Basics) {
  ASSERT_TRUE(SetupSync());
  syncer::ProfileSyncService* service = GetSyncService(0);
  syncer::SyncUserSettings* settings = service->GetUserSettings();
  EXPECT_TRUE(settings->GetSelectedTypes().Has(UserSelectableType::kApps));
  EXPECT_TRUE(service->GetActiveDataTypes().Has(syncer::APP_SETTINGS));

  settings->SetSelectedTypes(false, UserSelectableTypeSet());
  EXPECT_FALSE(settings->GetSelectedTypes().Has(UserSelectableType::kApps));
  EXPECT_FALSE(service->GetActiveDataTypes().Has(syncer::APP_SETTINGS));
}
#endif  // defined(OS_CHROMEOS)

}  // namespace
