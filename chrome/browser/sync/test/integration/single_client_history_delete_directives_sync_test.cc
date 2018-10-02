// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/macros.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/web_history_service_factory.h"
#include "chrome/browser/sync/test/integration/feature_toggler.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync/test/fake_server/fake_server.h"

namespace {

using sync_pb::HistoryDeleteDirectiveSpecifics;

// Allows to wait until the number of server-side entities is equal to a
// expected number.
class HistoryDeleteDirectivesEqualityChecker
    : public SingleClientStatusChangeChecker {
 public:
  HistoryDeleteDirectivesEqualityChecker(
      browser_sync::ProfileSyncService* service,
      fake_server::FakeServer* fake_server,
      size_t num_expected_directives)
      : SingleClientStatusChangeChecker(service),
        fake_server_(fake_server),
        num_expected_directives_(num_expected_directives) {}

  bool IsExitConditionSatisfied() override {
    const std::vector<sync_pb::SyncEntity> entities =
        fake_server_->GetSyncEntitiesByModelType(
            syncer::HISTORY_DELETE_DIRECTIVES);

    if (entities.size() == num_expected_directives_) {
      return true;
    }

    // |entities.size()| is only going to grow, if |entities.size()| ever
    // becomes bigger then all hope is lost of passing, stop now.
    EXPECT_LT(entities.size(), num_expected_directives_)
        << "Entity set will never become equal";
    return false;
  }

  std::string GetDebugMessage() const override {
    return "Waiting server side HISTORY_DELETE_DIRECTIVES to match expected.";
  }

 private:
  fake_server::FakeServer* const fake_server_;
  const size_t num_expected_directives_;

  DISALLOW_COPY_AND_ASSIGN(HistoryDeleteDirectivesEqualityChecker);
};

class SingleClientHistoryDeleteDirectivesSyncTest : public FeatureToggler,
                                                    public SyncTest {
 public:
  SingleClientHistoryDeleteDirectivesSyncTest()
      : FeatureToggler(switches::kSyncPseudoUSSHistoryDeleteDirectives),
        SyncTest(SINGLE_CLIENT) {}
  ~SingleClientHistoryDeleteDirectivesSyncTest() override {}

  bool WaitForHistoryDeleteDirectives(size_t num_expected_directives) {
    return HistoryDeleteDirectivesEqualityChecker(
               GetSyncService(0), GetFakeServer(), num_expected_directives)
        .Wait();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SingleClientHistoryDeleteDirectivesSyncTest);
};

IN_PROC_BROWSER_TEST_P(SingleClientHistoryDeleteDirectivesSyncTest,
                       ShouldCommitDeleteDirective) {
  const GURL kPageUrl = GURL("http://foo.com");
  const base::Time kHistoryEntryTime = base::Time::Now();
  base::CancelableTaskTracker task_tracker;

  ASSERT_TRUE(SetupSync());

  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfileWithoutCreating(GetProfile(0));
  history_service->AddPage(kPageUrl, kHistoryEntryTime,
                           history::SOURCE_BROWSED);

  history_service->ExpireLocalAndRemoteHistoryBetween(
      WebHistoryServiceFactory::GetForProfile(GetProfile(0)), std::set<GURL>(),
      /*begin_time=*/base::Time(), /*end_time=*/base::Time(), base::DoNothing(),
      &task_tracker);

  EXPECT_TRUE(WaitForHistoryDeleteDirectives(1));
}

INSTANTIATE_TEST_CASE_P(USS,
                        SingleClientHistoryDeleteDirectivesSyncTest,
                        ::testing::Values(false, true));

}  // namespace
