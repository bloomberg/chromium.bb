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
#include "components/browser_sync/profile_sync_service.h"
#include "components/sync/base/sync_prefs.h"
#include "components/sync/engine/polling_constants.h"
#include "components/sync/protocol/client_commands.pb.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::Eq;
using sessions_helper::CheckInitialState;
using sessions_helper::OpenTab;
using syncer::SyncPrefs;

namespace {

class SingleClientPollingSyncTest : public SyncTest {
 public:
  SingleClientPollingSyncTest() : SyncTest(SINGLE_CLIENT) {}
  ~SingleClientPollingSyncTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SingleClientPollingSyncTest);
};

// This test verifies that the poll intervals in prefs get initialized if no
// data is available yet.
IN_PROC_BROWSER_TEST_F(SingleClientPollingSyncTest, ShouldInitializePollPrefs) {
  // Setup clients and verify no poll intervals are present yet.
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  SyncPrefs sync_prefs(GetProfile(0)->GetPrefs());
  EXPECT_TRUE(sync_prefs.GetShortPollInterval().is_zero());
  EXPECT_TRUE(sync_prefs.GetLongPollInterval().is_zero());
  ASSERT_TRUE(sync_prefs.GetLastPollTime().is_null());

  // Execute a sync cycle and verify the client set up (and persisted) the
  // default values.
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  EXPECT_THAT(sync_prefs.GetShortPollInterval().InSeconds(),
              Eq(syncer::kDefaultShortPollIntervalSeconds));
  EXPECT_THAT(sync_prefs.GetLongPollInterval().InSeconds(),
              Eq(syncer::kDefaultLongPollIntervalSeconds));
}

// This test verifies that updates of the poll intervals get persisted
// That's important make sure clients with short live times will eventually poll
// (e.g. Android).
IN_PROC_BROWSER_TEST_F(SingleClientPollingSyncTest, ShouldUpdatePollPrefs) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

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
  ASSERT_TRUE(checker.Wait());

  SyncPrefs sync_prefs(GetProfile(0)->GetPrefs());
  EXPECT_THAT(sync_prefs.GetShortPollInterval().InSeconds(), Eq(67));
  EXPECT_THAT(sync_prefs.GetLongPollInterval().InSeconds(), Eq(199));
}

IN_PROC_BROWSER_TEST_F(SingleClientPollingSyncTest,
                       ShouldUsePollIntervalsFromPrefs) {
  // Setup clients and provide new poll intervals via prefs.
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  SyncPrefs sync_prefs(GetProfile(0)->GetPrefs());
  sync_prefs.SetShortPollInterval(base::TimeDelta::FromSeconds(123));
  sync_prefs.SetLongPollInterval(base::TimeDelta::FromSeconds(1234));

  // Execute a sync cycle and verify this cycle used those intervals.
  // This test assumes the SyncScheduler reads the actual intervals from the
  // context. This is covered in the SyncSchedulerImpl's unittest.
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  EXPECT_THAT(
      GetClient(0)->GetLastCycleSnapshot().short_poll_interval().InSeconds(),
      Eq(123));
  EXPECT_THAT(
      GetClient(0)->GetLastCycleSnapshot().long_poll_interval().InSeconds(),
      Eq(1234));
}

// TODO(tschumann): Add a test that tests a poll on start-up in a simpler set
// up: Bring up a single client, let it sync a local mode and do a fake restart.
// After that, GetLastCycleSnapshot() should be an empty snapshot, so the we can
// use the UpdatedProgressMarker checker to verify a poll has taken place. This
// might require changing
// UpdatedProgressMarkerChecker::IsExitConditionSatisfied() to require non-empty
// progress markers.
// This test could also verify that the last poll time gets not simply set
// to the start-up time but kept if not yet expired.

}  // namespace
