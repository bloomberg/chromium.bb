// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/run_loop.h"
#include "build/build_config.h"

#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/session_hierarchy_match_checker.h"
#include "chrome/browser/sync/test/integration/sessions_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/common/webui_url_constants.h"
#include "components/sync/base/sync_prefs.h"
#include "components/sync/engine/polling_constants.h"
#include "components/sync/protocol/client_commands.pb.h"
#include "components/sync/test/fake_server/sessions_hierarchy.h"

#include "components/sync/protocol/client_commands.pb.h"

using sessions_helper::CheckInitialState;
using sessions_helper::OpenTab;
using syncer::SyncPrefs;

namespace {

class SyncClientCommandTest : public SyncTest {
 public:
  SyncClientCommandTest() : SyncTest(SINGLE_CLIENT) {}
  ~SyncClientCommandTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SyncClientCommandTest);
};

IN_PROC_BROWSER_TEST_F(SyncClientCommandTest, ShouldPersistPollIntervals) {
  // Setup clients and verify no poll intervals are present yet.
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  std::unique_ptr<SyncPrefs> sync_prefs =
      std::make_unique<SyncPrefs>(GetProfile(0)->GetPrefs());
  EXPECT_TRUE(sync_prefs->GetShortPollInterval().is_zero());
  EXPECT_TRUE(sync_prefs->GetLongPollInterval().is_zero());

  // Execute a sync cycle and verify the client set up (and persisted) the
  // default values.
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  EXPECT_EQ(sync_prefs->GetShortPollInterval().InSeconds(),
            syncer::kDefaultShortPollIntervalSeconds);
  EXPECT_EQ(sync_prefs->GetLongPollInterval().InSeconds(),
            syncer::kDefaultLongPollIntervalSeconds);

  // Simulate server-provided poll intervals and make sure they get persisted.
  sync_pb::ClientCommand client_command;
  client_command.set_set_sync_poll_interval(67);
  client_command.set_set_sync_long_poll_interval(199);
  GetFakeServer()->SetClientCommand(client_command);

  // Trigger a sync-cycle.
  ASSERT_TRUE(CheckInitialState(0));
  ASSERT_TRUE(OpenTab(0, GURL(chrome::kChromeUIHistoryURL)));
  SessionHierarchyMatchChecker checker(
      fake_server::SessionsHierarchy(
          {{GURL(chrome::kChromeUIHistoryURL).spec()}}),
      GetSyncService(0), GetFakeServer());
  EXPECT_TRUE(checker.Wait());

  EXPECT_EQ(sync_prefs->GetShortPollInterval().InSeconds(), 67);
  EXPECT_EQ(sync_prefs->GetLongPollInterval().InSeconds(), 199);
}

IN_PROC_BROWSER_TEST_F(SyncClientCommandTest, ShouldUsePollIntervalsFromPrefs) {
  // Setup clients and provide new poll intervals via prefs.
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  std::unique_ptr<SyncPrefs> sync_prefs =
      std::make_unique<SyncPrefs>(GetProfile(0)->GetPrefs());
  sync_prefs->SetShortPollInterval(base::TimeDelta::FromSeconds(123));
  sync_prefs->SetLongPollInterval(base::TimeDelta::FromSeconds(1234));

  // Execute a sync cycle and verify this cycle used those intervals.
  // This test assumes the SyncScheduler reads the actual intervals from the
  // context. This is covered in the SyncSchedulerImpl's unittest.
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  EXPECT_EQ(
      123,
      GetClient(0)->GetLastCycleSnapshot().short_poll_interval().InSeconds());
  EXPECT_EQ(
      1234,
      GetClient(0)->GetLastCycleSnapshot().long_poll_interval().InSeconds());
}

}  // namespace
