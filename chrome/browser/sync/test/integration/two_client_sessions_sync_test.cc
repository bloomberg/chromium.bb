// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/guid.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sync/test/integration/encryption_helper.h"
#include "chrome/browser/sync/test/integration/passwords_helper.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sessions_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/sync/engine/cycle/sync_cycle_snapshot.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using sessions_helper::CheckInitialState;
using sessions_helper::CloseTab;
using sessions_helper::DeleteForeignSession;
using sessions_helper::ForeignSessionsMatchChecker;
using sessions_helper::GetLocalWindows;
using sessions_helper::GetSessionData;
using sessions_helper::NavigateTab;
using sessions_helper::OpenTab;
using sessions_helper::OpenTabAtIndex;
using sessions_helper::ScopedWindowMap;
using sessions_helper::SessionWindowMap;
using sessions_helper::SyncedSessionVector;
using sessions_helper::WindowsMatch;
using testing::IsEmpty;

class TwoClientSessionsSyncTest : public SyncTest {
 public:
  TwoClientSessionsSyncTest() : SyncTest(TWO_CLIENT) {}
  ~TwoClientSessionsSyncTest() override {}

  bool WaitForForeignSessionsToSync(int local_index, int non_local_index) {
    return ForeignSessionsMatchChecker(non_local_index, local_index).Wait();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TwoClientSessionsSyncTest);
};

static const char* kURL1 = "data:text/html,<html><title>Test</title></html>";
static const char* kURL2 = "data:text/html,<html><title>Test2</title></html>";
static const char* kURL3 = "data:text/html,<html><title>Test3</title></html>";
static const char* kURL4 = "data:text/html,<html><title>Test4</title></html>";
static const char* kURLTemplate =
    "data:text/html,<html><title>Test%s</title></html>";

// TODO(zea): Test each individual session command we care about separately.
// (as well as multi-window). We're currently only checking basic single-window/
// single-tab functionality.

IN_PROC_BROWSER_TEST_F(TwoClientSessionsSyncTest,
                       E2E_ENABLED(SingleClientChanged)) {
  ResetSyncForPrimaryAccount();
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Open tab and access a url on client 0
  ScopedWindowMap client0_windows;
  std::string url =
      base::StringPrintf(kURLTemplate, base::GenerateGUID().c_str());

  ASSERT_TRUE(OpenTab(0, GURL(url)));
  EXPECT_TRUE(WaitForForeignSessionsToSync(0, 1));
}

IN_PROC_BROWSER_TEST_F(TwoClientSessionsSyncTest, SingleClientClosed) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Open two tabs on client 0.
  OpenTab(0, GURL(kURL1));
  OpenTab(0, GURL(kURL2));
  ASSERT_TRUE(WaitForForeignSessionsToSync(0, 1));

  // Close one of the two tabs. We also issue another navigation to make sure
  // association logic kicks in.
  CloseTab(/*index=*/0, /*tab_index=*/1);
  NavigateTab(0, GURL(kURL3));
  EXPECT_TRUE(WaitForForeignSessionsToSync(0, 1));

  std::vector<sync_pb::SyncEntity> entities =
      GetFakeServer()->GetSyncEntitiesByModelType(syncer::SESSIONS);
  // Two header entities and one tab entity (the other one has been deleted).
  EXPECT_EQ(3U, entities.size());
}

IN_PROC_BROWSER_TEST_F(TwoClientSessionsSyncTest, E2E_ENABLED(AllChanged)) {
  ResetSyncForPrimaryAccount();
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Open tabs on all clients and retain window information.
  for (int i = 0; i < num_clients(); ++i) {
    ScopedWindowMap windows;
    std::string url =
        base::StringPrintf(kURLTemplate, base::GenerateGUID().c_str());
    ASSERT_TRUE(OpenTab(i, GURL(url)));
  }

  // Get foreign session data from all clients and check it against all
  // local sessions.
  for (int i = 0; i < num_clients(); ++i) {
    for (int j = 0; j < num_clients(); ++j) {
      if (i == j) {
        continue;
      }
      EXPECT_TRUE(WaitForForeignSessionsToSync(i, j));
    }
  }
}

IN_PROC_BROWSER_TEST_F(TwoClientSessionsSyncTest, BothChanged) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  ASSERT_TRUE(CheckInitialState(0));
  ASSERT_TRUE(CheckInitialState(1));

  ASSERT_TRUE(OpenTab(0, GURL(kURL1)));
  ASSERT_TRUE(OpenTab(1, GURL(kURL2)));

  EXPECT_TRUE(WaitForForeignSessionsToSync(0, 1));
  EXPECT_TRUE(WaitForForeignSessionsToSync(1, 0));

  // Check that a navigation in client 0 is reflected on client 1.
  NavigateTab(0, GURL(kURL3));
  EXPECT_TRUE(WaitForForeignSessionsToSync(0, 1));
}

IN_PROC_BROWSER_TEST_F(TwoClientSessionsSyncTest, DeleteIdleSession) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  ASSERT_TRUE(CheckInitialState(0));
  ASSERT_TRUE(CheckInitialState(1));

  // Client 0 opened some tabs then went idle.
  ASSERT_TRUE(OpenTab(0, GURL(kURL1)));
  ASSERT_TRUE(WaitForForeignSessionsToSync(0, 1));

  // Get foreign session data from client 1.
  SyncedSessionVector sessions1;
  ASSERT_TRUE(GetSessionData(1, &sessions1));

  // Client 1 now deletes client 0's tabs. This frees the memory of sessions1.
  DeleteForeignSession(1, sessions1[0]->session_tag);
  ASSERT_TRUE(GetClient(1)->AwaitMutualSyncCycleCompletion(GetClient(0)));
  EXPECT_FALSE(GetSessionData(1, &sessions1));
}

IN_PROC_BROWSER_TEST_F(TwoClientSessionsSyncTest, DeleteActiveSession) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  ASSERT_TRUE(CheckInitialState(0));
  ASSERT_TRUE(CheckInitialState(1));

  // Client 0 opened some tabs then went idle.
  ASSERT_TRUE(OpenTab(0, GURL(kURL1)));
  ASSERT_TRUE(WaitForForeignSessionsToSync(0, 1));

  SyncedSessionVector sessions1;
  ASSERT_TRUE(GetSessionData(1, &sessions1));
  ASSERT_EQ(1U, sessions1.size());

  // Client 1 now deletes client 0's tabs. This frees the memory of sessions1.
  DeleteForeignSession(1, sessions1[0]->session_tag);
  ASSERT_TRUE(GetClient(1)->AwaitMutualSyncCycleCompletion(GetClient(0)));
  ASSERT_FALSE(GetSessionData(1, &sessions1));

  // Client 0 becomes active again with a new tab.
  ASSERT_TRUE(OpenTab(0, GURL(kURL2)));
  EXPECT_TRUE(WaitForForeignSessionsToSync(0, 1));
  EXPECT_TRUE(GetSessionData(1, &sessions1));
}

IN_PROC_BROWSER_TEST_F(TwoClientSessionsSyncTest, MultipleWindowsMultipleTabs) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  ASSERT_TRUE(CheckInitialState(0));
  ASSERT_TRUE(CheckInitialState(1));

  EXPECT_TRUE(OpenTab(0, GURL(kURL1)));
  EXPECT_TRUE(OpenTabAtIndex(0, 1, GURL(kURL2)));

  // Add a second browser for profile 0. This browser ends up in index 2.
  AddBrowser(0);
  EXPECT_TRUE(OpenTab(2, GURL(kURL3)));
  EXPECT_TRUE(OpenTabAtIndex(2, 1, GURL(kURL4)));

  EXPECT_TRUE(WaitForForeignSessionsToSync(0, 1));
}

IN_PROC_BROWSER_TEST_F(TwoClientSessionsSyncTest,
                       DISABLED_NoHistoryIfEncryptionEnabled) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  ASSERT_TRUE(CheckInitialState(0));
  ASSERT_TRUE(CheckInitialState(1));

  GetSyncService(0)->GetUserSettings()->SetEncryptionPassphrase("passphrase");
  ASSERT_TRUE(
      PassphraseRequiredStateChecker(GetSyncService(1), /*desired_state=*/true)
          .Wait());
  ASSERT_TRUE(GetSyncService(1)->GetUserSettings()->SetDecryptionPassphrase(
      "passphrase"));

  EXPECT_TRUE(OpenTab(0, GURL(kURL1)));
  EXPECT_TRUE(WaitForForeignSessionsToSync(0, 1));

  EXPECT_THAT(GetFakeServer()->GetCommittedHistoryURLs(), IsEmpty());
}

}  // namespace
