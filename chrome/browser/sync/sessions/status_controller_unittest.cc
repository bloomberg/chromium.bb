// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sessions/sync_session.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_sync {
namespace sessions {

typedef testing::Test StatusControllerTest;

TEST_F(StatusControllerTest, GetsDirty) {
  StatusController status;
  status.set_num_conflicting_commits(1);
  EXPECT_TRUE(status.TestAndClearIsDirty());
  EXPECT_FALSE(status.TestAndClearIsDirty());  // Test that it actually resets.
  status.set_num_conflicting_commits(1);
  EXPECT_FALSE(status.TestAndClearIsDirty());
  status.set_num_conflicting_commits(0);
  EXPECT_TRUE(status.TestAndClearIsDirty());

  status.set_num_consecutive_problem_get_updates(1);
  EXPECT_TRUE(status.TestAndClearIsDirty());

  status.increment_num_consecutive_problem_get_updates();
  EXPECT_TRUE(status.TestAndClearIsDirty());

  status.set_num_consecutive_problem_commits(1);
  EXPECT_TRUE(status.TestAndClearIsDirty());

  status.increment_num_consecutive_problem_commits();
  EXPECT_TRUE(status.TestAndClearIsDirty());

  status.set_num_consecutive_transient_error_commits(1);
  EXPECT_TRUE(status.TestAndClearIsDirty());

  status.increment_num_consecutive_transient_error_commits_by(1);
  EXPECT_TRUE(status.TestAndClearIsDirty());
  status.increment_num_consecutive_transient_error_commits_by(0);
  EXPECT_FALSE(status.TestAndClearIsDirty());

  status.set_num_consecutive_errors(10);
  EXPECT_TRUE(status.TestAndClearIsDirty());
  status.set_num_consecutive_errors(10);
  EXPECT_FALSE(status.TestAndClearIsDirty());  // Only dirty if value changed.
  status.increment_num_consecutive_errors();
  EXPECT_TRUE(status.TestAndClearIsDirty());
  status.increment_num_consecutive_errors_by(1);
  EXPECT_TRUE(status.TestAndClearIsDirty());
  status.increment_num_consecutive_errors_by(0);
  EXPECT_FALSE(status.TestAndClearIsDirty());

  status.set_current_sync_timestamp(100);
  EXPECT_TRUE(status.TestAndClearIsDirty());

  status.set_num_server_changes_remaining(30);
  EXPECT_TRUE(status.TestAndClearIsDirty());

  status.set_over_quota(true);
  EXPECT_TRUE(status.TestAndClearIsDirty());
  status.set_over_quota(false);
  EXPECT_TRUE(status.TestAndClearIsDirty());

  status.set_invalid_store(true);
  EXPECT_TRUE(status.TestAndClearIsDirty());
  status.set_invalid_store(false);
  EXPECT_TRUE(status.TestAndClearIsDirty());

  status.set_syncer_stuck(true);
  EXPECT_TRUE(status.TestAndClearIsDirty());
  status.set_syncer_stuck(false);
  EXPECT_TRUE(status.TestAndClearIsDirty());

  status.set_syncing(true);
  EXPECT_TRUE(status.TestAndClearIsDirty());
  status.set_syncing(false);
  EXPECT_TRUE(status.TestAndClearIsDirty());

  status.set_num_successful_commits(1);
  EXPECT_TRUE(status.TestAndClearIsDirty());
  status.increment_num_successful_commits();
  EXPECT_TRUE(status.TestAndClearIsDirty());

  std::vector<int64> v;
  v.push_back(1);
  status.set_unsynced_handles(v);
  EXPECT_TRUE(status.TestAndClearIsDirty());
  std::vector<int64> v2;
  v2.push_back(1);
  status.set_unsynced_handles(v2);
  EXPECT_FALSE(status.TestAndClearIsDirty());  // Test for deep comparison.
}

TEST_F(StatusControllerTest, StaysClean) {
  StatusController status;
  status.set_conflict_sets_built(true);
  EXPECT_FALSE(status.TestAndClearIsDirty());
  status.set_conflicts_resolved(true);
  EXPECT_FALSE(status.TestAndClearIsDirty());
  status.set_timestamp_dirty(true);
  EXPECT_FALSE(status.TestAndClearIsDirty());
  status.set_items_committed(true);
  EXPECT_FALSE(status.TestAndClearIsDirty());

  std::vector<syncable::Id> v;
  v.push_back(syncable::Id());
  status.set_commit_ids(v);
  EXPECT_FALSE(status.TestAndClearIsDirty());
}

// This test is useful, as simple as it sounds, due to the copy-paste prone
// nature of status_controller.cc (we have had bugs in the past where a set_foo
// method was actually setting |bar_| instead!).
TEST_F(StatusControllerTest, ReadYourWrites) {
  StatusController status;
  status.set_num_conflicting_commits(1);
  EXPECT_EQ(1, status.error_counters().num_conflicting_commits);

  status.set_num_consecutive_problem_get_updates(2);
  EXPECT_EQ(2, status.error_counters().consecutive_problem_get_updates);
  status.increment_num_consecutive_problem_get_updates();
  EXPECT_EQ(3, status.error_counters().consecutive_problem_get_updates);

  status.set_num_consecutive_problem_commits(4);
  EXPECT_EQ(4, status.error_counters().consecutive_problem_commits);
  status.increment_num_consecutive_problem_commits();
  EXPECT_EQ(5, status.error_counters().consecutive_problem_commits);

  status.set_num_consecutive_transient_error_commits(6);
  EXPECT_EQ(6, status.error_counters().consecutive_transient_error_commits);
  status.increment_num_consecutive_transient_error_commits_by(1);
  EXPECT_EQ(7, status.error_counters().consecutive_transient_error_commits);
  status.increment_num_consecutive_transient_error_commits_by(0);
  EXPECT_EQ(7, status.error_counters().consecutive_transient_error_commits);

  status.set_num_consecutive_errors(8);
  EXPECT_EQ(8, status.error_counters().consecutive_errors);
  status.increment_num_consecutive_errors();
  EXPECT_EQ(9, status.error_counters().consecutive_errors);
  status.increment_num_consecutive_errors_by(2);
  EXPECT_EQ(11, status.error_counters().consecutive_errors);

  status.set_current_sync_timestamp(12);
  EXPECT_EQ(12, status.change_progress().current_sync_timestamp);

  status.set_num_server_changes_remaining(13);
  EXPECT_EQ(13, status.change_progress().num_server_changes_remaining);

  EXPECT_FALSE(status.syncer_status().over_quota);
  status.set_over_quota(true);
  EXPECT_TRUE(status.syncer_status().over_quota);

  EXPECT_FALSE(status.syncer_status().invalid_store);
  status.set_invalid_store(true);
  EXPECT_TRUE(status.syncer_status().invalid_store);

  EXPECT_FALSE(status.syncer_status().syncer_stuck);
  status.set_syncer_stuck(true);
  EXPECT_TRUE(status.syncer_status().syncer_stuck);

  EXPECT_FALSE(status.syncer_status().syncing);
  status.set_syncing(true);
  EXPECT_TRUE(status.syncer_status().syncing);

  status.set_num_successful_commits(14);
  EXPECT_EQ(14, status.syncer_status().num_successful_commits);
  status.increment_num_successful_commits();
  EXPECT_EQ(15, status.syncer_status().num_successful_commits);

  std::vector<int64> v;
  v.push_back(16);
  status.set_unsynced_handles(v);
  EXPECT_EQ(16, v[0]);
}

TEST_F(StatusControllerTest, ResetTransientState) {
  StatusController status;
  status.set_conflict_sets_built(true);
  EXPECT_TRUE(status.conflict_sets_built());
  status.set_timestamp_dirty(true);
  status.set_items_committed(true);
  status.set_conflicts_resolved(true);
  sync_pb::SyncEntity entity;
  status.mutable_update_progress()->AddVerifyResult(VERIFY_SUCCESS, entity);
  status.mutable_commit_message()->mutable_commit();  // Lazy initialization.
  ASSERT_TRUE(status.mutable_commit_message()->has_commit());
  status.mutable_commit_response()->mutable_commit();
  ASSERT_TRUE(status.commit_response().has_commit());
  status.mutable_updates_response()->mutable_get_updates();
  ASSERT_TRUE(status.updates_response().has_get_updates());

  std::vector<int64> v;
  v.push_back(1);
  status.set_unsynced_handles(v);

  std::vector<syncable::Id> v2;
  v2.push_back(syncable::Id());
  status.set_commit_ids(v2);

  EXPECT_TRUE(status.TestAndClearIsDirty());
  status.ResetTransientState();
  EXPECT_FALSE(status.TestAndClearIsDirty());

  EXPECT_FALSE(status.conflict_sets_built());
  EXPECT_FALSE(status.timestamp_dirty());
  EXPECT_FALSE(status.did_commit_items());
  EXPECT_FALSE(status.conflicts_resolved());
  EXPECT_EQ(0, status.update_progress().VerifiedUpdatesSize());
  EXPECT_FALSE(status.mutable_commit_message()->has_commit());
  EXPECT_FALSE(status.commit_response().has_commit());
  EXPECT_FALSE(status.updates_response().has_get_updates());
}

TEST_F(StatusControllerTest, HasConflictingUpdates) {
  StatusController status;
  EXPECT_FALSE(status.update_progress().HasConflictingUpdates());
  status.mutable_update_progress()->AddAppliedUpdate(SUCCESS, syncable::Id());
  status.mutable_update_progress()->AddAppliedUpdate(CONFLICT, syncable::Id());
  EXPECT_TRUE(status.update_progress().HasConflictingUpdates());
}

TEST_F(StatusControllerTest, CountUpdates) {
  StatusController status;
  EXPECT_EQ(0, status.CountUpdates());
  EXPECT_TRUE(status.got_zero_updates());
  ClientToServerResponse* response(status.mutable_updates_response());
  sync_pb::SyncEntity* entity1 = response->mutable_get_updates()->add_entries();
  sync_pb::SyncEntity* entity2 = response->mutable_get_updates()->add_entries();
  ASSERT_TRUE(entity1 != NULL && entity2 != NULL);
  EXPECT_EQ(2, status.CountUpdates());
  EXPECT_FALSE(status.got_zero_updates());
}

}  // namespace sessions
}  // namespace browser_sync
