// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/command_line.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/test/integration/apps_helper.h"
#include "chrome/browser/sync/test/integration/sync_app_list_helper.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service_factory.h"
#include "ui/app_list/app_list_switches.h"

using sync_integration_test_util::AwaitCommitActivityCompletion;

namespace {

const size_t kNumDefaultApps = 2;

bool AllProfilesHaveSameAppListAsVerifier() {
  return SyncAppListHelper::GetInstance()->
      AllProfilesHaveSameAppListAsVerifier();
}

}  // namespace

class SingleClientAppListSyncTest : public SyncTest {
 public:
  SingleClientAppListSyncTest() : SyncTest(SINGLE_CLIENT) {}

  virtual ~SingleClientAppListSyncTest() {}

  // SyncTest
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    SyncTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(app_list::switches::kEnableSyncAppList);
  }

  virtual bool SetupClients() OVERRIDE {
    if (!SyncTest::SetupClients())
      return false;

    // Init SyncAppListHelper to ensure that the extension system is initialized
    // for each Profile.
    SyncAppListHelper::GetInstance();
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SingleClientAppListSyncTest);
};

IN_PROC_BROWSER_TEST_F(SingleClientAppListSyncTest, AppListEmpty) {
  ASSERT_TRUE(SetupSync());

  ASSERT_TRUE(AllProfilesHaveSameAppListAsVerifier());
}

IN_PROC_BROWSER_TEST_F(SingleClientAppListSyncTest, AppListSomeApps) {
  ASSERT_TRUE(SetupSync());

  const size_t kNumApps = 5;
  for (int i = 0; i < static_cast<int>(kNumApps); ++i) {
    apps_helper::InstallApp(GetProfile(0), i);
    apps_helper::InstallApp(verifier(), i);
  }

  app_list::AppListSyncableService* service =
      app_list::AppListSyncableServiceFactory::GetForProfile(verifier());
  ASSERT_EQ(kNumApps + kNumDefaultApps, service->GetNumSyncItemsForTest());

  ASSERT_TRUE(AwaitCommitActivityCompletion(GetSyncService((0))));
  ASSERT_TRUE(AllProfilesHaveSameAppListAsVerifier());

}
