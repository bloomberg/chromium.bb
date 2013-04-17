// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/prefs/pref_service.h"
#include "chrome/browser/managed_mode/managed_user_service.h"
#include "chrome/browser/managed_mode/managed_user_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/common/pref_names.h"

class SingleClientManagedUserSettingsSyncTest : public SyncTest {
 public:
  SingleClientManagedUserSettingsSyncTest() : SyncTest(SINGLE_CLIENT) {}

  virtual ~SingleClientManagedUserSettingsSyncTest() {}
};

IN_PROC_BROWSER_TEST_F(SingleClientManagedUserSettingsSyncTest, Sanity) {
  ASSERT_TRUE(SetupClients());
  for (int i = 0; i < num_clients(); ++i) {
    Profile* profile = GetProfile(i);
    profile->GetPrefs()->SetBoolean(prefs::kProfileIsManaged, true);
    ManagedUserServiceFactory::GetForProfile(profile)->Init();
  }
  ASSERT_TRUE(SetupSync());
}
