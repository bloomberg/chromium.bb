// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sessions/sync_session.h"
#include "chrome/browser/sync/test/engine/test_id_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_sync {
namespace sessions {

class StatusControllerTest : public testing::Test {
 public:
  virtual void SetUp() {
    routes_[syncable::BOOKMARKS] = GROUP_UI;
  }
 protected:
  ModelSafeRoutingInfo routes_;
};

TEST_F(StatusControllerTest, GetsDirty) {
  StatusController status(routes_);
  status.increment_num_conflicting_commits_by(1);
  EXPECT_TRUE(status.TestAndClearIsDirty());
  EXPECT_FALSE(status.TestAndClearIsDirty());  // Test that it actually resets.
  status.increment_num_conflicting_commits_by(0);
  EXPECT_FALSE(status.TestAndClearIsDirty());
  status.increment_num_conflicting_commits_by(1);
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

  status.set_num_server_changes_remaining(30);
  EXPECT_TRUE(status.TestAndClearIsDirty());

  status.set_invalid_store(true);
  EXPECT_TRUE(status.TestAndClearIsDirty());
  status.set_invalid_store(false);
  EXPECT_TRUE(status.TestAndClearIsDirty());

  status.set_syncer_stuck(true);
  EXPECT_TRUE(status.TestAndClearIsDirty());
  status.set_syncer_stuck(false);
  EXPECT_TRUE(status.TestAndClearIsDirty());

  status.SetSyncInProgressAndUpdateStartTime(true);
  EXPECT_TRUE(status.TestAndClearIsDirty());
  status.SetSyncInProgressAndUpdateStartTime(false);
  EXPECT_TRUE(status.TestAndClearIsDirty());

  status.increment_num_successful_commits();
  EXPECT_TRUE(status.TestAndClearIsDirty());
  status.increment_num_successful_commits();
  EXPECT_TRUE(status.TestAndClearIsDirty());

  {
    ScopedModelSafeGroupRestriction r(&status, GROUP_UI);
    status.mutable_conflict_progress()->AddConflictingItemById(syncable::Id());
  }
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
  StatusController status(routes_);
  status.update_conflict_sets_built(true);
  EXPECT_FALSE(status.TestAndClearIsDirty());
  status.update_conflicts_resolved(true);
  EXPECT_FALSE(status.TestAndClearIsDirty());

  status.set_items_committed();
  EXPECT_FALSE(status.TestAndClearIsDirty());

  OrderedCommitSet commits(routes_);
  commits.AddCommitItem(0, syncable::Id(), syncable::BOOKMARKS);
  status.set_commit_set(commits);
  EXPECT_FALSE(status.TestAndClearIsDirty());
}

// This test is useful, as simple as it sounds, due to the copy-paste prone
// nature of status_controller.cc (we have had bugs in the past where a set_foo
// method was actually setting |bar_| instead!).
TEST_F(StatusControllerTest, ReadYourWrites) {
  StatusController status(routes_);
  status.increment_num_conflicting_commits_by(1);
  EXPECT_EQ(1, status.error().num_conflicting_commits);

  status.set_num_consecutive_transient_error_commits(6);
  EXPECT_EQ(6, status.error().consecutive_transient_error_commits);
  status.increment_num_consecutive_transient_error_commits_by(1);
  EXPECT_EQ(7, status.error().consecutive_transient_error_commits);
  status.increment_num_consecutive_transient_error_commits_by(0);
  EXPECT_EQ(7, status.error().consecutive_transient_error_commits);

  status.set_num_consecutive_errors(8);
  EXPECT_EQ(8, status.error().consecutive_errors);
  status.increment_num_consecutive_errors();
  EXPECT_EQ(9, status.error().consecutive_errors);
  status.increment_num_consecutive_errors_by(2);
  EXPECT_EQ(11, status.error().consecutive_errors);

  status.set_num_server_changes_remaining(13);
  EXPECT_EQ(13, status.num_server_changes_remaining());

  EXPECT_FALSE(status.syncer_status().invalid_store);
  status.set_invalid_store(true);
  EXPECT_TRUE(status.syncer_status().invalid_store);

  EXPECT_FALSE(status.syncer_status().syncer_stuck);
  status.set_syncer_stuck(true);
  EXPECT_TRUE(status.syncer_status().syncer_stuck);

  EXPECT_FALSE(status.syncer_status().sync_in_progress);
  status.SetSyncInProgressAndUpdateStartTime(true);
  EXPECT_TRUE(status.syncer_status().sync_in_progress);

  for (int i = 0; i < 14; i++)
    status.increment_num_successful_commits();
  EXPECT_EQ(14, status.syncer_status().num_successful_commits);

  std::vector<int64> v;
  v.push_back(16);
  status.set_unsynced_handles(v);
  EXPECT_EQ(16, v[0]);
}

TEST_F(StatusControllerTest, HasConflictingUpdates) {
  StatusController status(routes_);
  EXPECT_FALSE(status.HasConflictingUpdates());
  {
    ScopedModelSafeGroupRestriction r(&status, GROUP_UI);
    EXPECT_FALSE(status.update_progress());
    status.mutable_update_progress()->AddAppliedUpdate(SUCCESS,
        syncable::Id());
    status.mutable_update_progress()->AddAppliedUpdate(CONFLICT,
        syncable::Id());
    EXPECT_TRUE(status.update_progress()->HasConflictingUpdates());
  }

  EXPECT_TRUE(status.HasConflictingUpdates());

  {
    ScopedModelSafeGroupRestriction r(&status, GROUP_PASSIVE);
    EXPECT_FALSE(status.update_progress());
  }
}

TEST_F(StatusControllerTest, CountUpdates) {
  StatusController status(routes_);
  EXPECT_EQ(0, status.CountUpdates());
  ClientToServerResponse* response(status.mutable_updates_response());
  sync_pb::SyncEntity* entity1 = response->mutable_get_updates()->add_entries();
  sync_pb::SyncEntity* entity2 = response->mutable_get_updates()->add_entries();
  ASSERT_TRUE(entity1 != NULL && entity2 != NULL);
  EXPECT_EQ(2, status.CountUpdates());
}

// Test TotalNumConflictingItems
TEST_F(StatusControllerTest, TotalNumConflictingItems) {
  StatusController status(routes_);
  TestIdFactory f;
  {
    ScopedModelSafeGroupRestriction r(&status, GROUP_UI);
    EXPECT_FALSE(status.conflict_progress());
    status.mutable_conflict_progress()->AddConflictingItemById(f.NewLocalId());
    status.mutable_conflict_progress()->AddConflictingItemById(f.NewLocalId());
    EXPECT_EQ(2, status.conflict_progress()->ConflictingItemsSize());
  }
  EXPECT_EQ(2, status.TotalNumConflictingItems());
  {
    ScopedModelSafeGroupRestriction r(&status, GROUP_DB);
    EXPECT_FALSE(status.conflict_progress());
    status.mutable_conflict_progress()->AddConflictingItemById(f.NewLocalId());
    status.mutable_conflict_progress()->AddConflictingItemById(f.NewLocalId());
    EXPECT_EQ(2, status.conflict_progress()->ConflictingItemsSize());
  }
  EXPECT_EQ(4, status.TotalNumConflictingItems());
}

// Basic test that non group-restricted state accessors don't cause violations.
TEST_F(StatusControllerTest, Unrestricted) {
  StatusController status(routes_);
  const UpdateProgress* progress =
      status.GetUnrestrictedUpdateProgress(GROUP_UI);
  EXPECT_FALSE(progress);
  status.mutable_commit_message();
  status.commit_response();
  status.mutable_commit_response();
  status.updates_response();
  status.mutable_updates_response();
  status.error();
  status.syncer_status();
  status.num_server_changes_remaining();
  status.commit_ids();
  status.HasBookmarkCommitActivity();
  status.download_updates_succeeded();
  status.ServerSaysNothingMoreToDownload();
  status.group_restriction();
}

}  // namespace sessions
}  // namespace browser_sync
