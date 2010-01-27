// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sessions/sync_session.h"

#include "chrome/browser/sync/engine/conflict_resolver.h"
#include "chrome/browser/sync/engine/syncer_types.h"
#include "chrome/browser/sync/engine/syncer_util.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/browser/sync/syncable/syncable.h"
#include "chrome/test/sync/engine/test_directory_setter_upper.h"
#include "testing/gtest/include/gtest/gtest.h"

using syncable::WriteTransaction;

namespace browser_sync {
namespace sessions {
namespace {

class SyncSessionTest : public testing::Test,
                        public SyncSession::Delegate,
                        public ModelSafeWorkerRegistrar {
 public:
  SyncSessionTest() : controller_invocations_allowed_(false) {}
  virtual void SetUp() {
    context_.reset(new SyncSessionContext(NULL, NULL, this));
    session_.reset(new SyncSession(context_.get(), this));
  }
  virtual void TearDown() {
    session_.reset();
    context_.reset();
  }

  virtual void OnSilencedUntil(const base::TimeTicks& silenced_until) {
    FailControllerInvocationIfDisabled("OnSilencedUntil");
  }
  virtual bool IsSyncingCurrentlySilenced() {
    FailControllerInvocationIfDisabled("IsSyncingCurrentlySilenced");
   return false;
  }
  virtual void OnReceivedLongPollIntervalUpdate(
      const base::TimeDelta& new_interval) {
    FailControllerInvocationIfDisabled("OnReceivedLongPollIntervalUpdate");
  }
  virtual void OnReceivedShortPollIntervalUpdate(
      const base::TimeDelta& new_interval) {
    FailControllerInvocationIfDisabled("OnReceivedShortPollIntervalUpdate");
  }

  // ModelSafeWorkerRegistrar implementation.
  virtual void GetWorkers(std::vector<ModelSafeWorker*>* out) {}
  virtual void GetModelSafeRoutingInfo(ModelSafeRoutingInfo* out) {}

  StatusController* status() { return session_->status_controller(); }
 protected:
  void FailControllerInvocationIfDisabled(const std::string& msg) {
    if (!controller_invocations_allowed_)
      FAIL() << msg;
  }
  bool controller_invocations_allowed_;
  scoped_ptr<SyncSession> session_;
  scoped_ptr<SyncSessionContext> context_;
};

TEST_F(SyncSessionTest, ScopedContextHelpers) {
  ConflictResolver resolver;
  SyncerEventChannel* channel = new SyncerEventChannel(
      SyncerEvent(SyncerEvent::SHUTDOWN_USE_WITH_CARE));
  EXPECT_FALSE(context_->resolver());
  EXPECT_FALSE(context_->syncer_event_channel());
  {
    ScopedSessionContextConflictResolver s_resolver(context_.get(), &resolver);
    ScopedSessionContextSyncerEventChannel s_channel(context_.get(), channel);
    EXPECT_EQ(&resolver, context_->resolver());
    EXPECT_EQ(channel, context_->syncer_event_channel());
  }
  EXPECT_FALSE(context_->resolver());
  EXPECT_FALSE(context_->syncer_event_channel());
  delete channel;
}

TEST_F(SyncSessionTest, SetWriteTransaction) {
  TestDirectorySetterUpper db;
  db.SetUp();
  session_.reset(NULL);
  context_.reset(new SyncSessionContext(NULL, db.manager(), this));
  session_.reset(new SyncSession(context_.get(), this));
  context_->set_account_name(db.name());
  syncable::ScopedDirLookup dir(context_->directory_manager(),
                                context_->account_name());
  ASSERT_TRUE(dir.good());

  SyncSession session(context_.get(), this);
  EXPECT_TRUE(NULL == session.write_transaction());
  {
    WriteTransaction trans(dir, syncable::UNITTEST, __FILE__, __LINE__);
    sessions::ScopedSetSessionWriteTransaction set_trans(&session, &trans);
    EXPECT_TRUE(&trans == session.write_transaction());
  }
  db.TearDown();
}

TEST_F(SyncSessionTest, MoreToSyncIfUnsyncedGreaterThanCommitted) {
  // If any forward progress was made during the session, and the number of
  // unsynced handles still exceeds the number of commit ids we added, there is
  // more to sync. For example, this occurs if we had more commit ids
  // than could fit in a single commit batch.
  EXPECT_FALSE(session_->HasMoreToSync());
  std::vector<syncable::Id> commit_ids;
  commit_ids.push_back(syncable::Id());
  status()->set_commit_ids(commit_ids);
  EXPECT_FALSE(session_->HasMoreToSync());

  std::vector<int64> unsynced_handles;
  unsynced_handles.push_back(1);
  unsynced_handles.push_back(2);
  status()->set_unsynced_handles(unsynced_handles);
  EXPECT_FALSE(session_->HasMoreToSync());
  status()->increment_num_successful_commits();
  EXPECT_TRUE(session_->HasMoreToSync());
  status()->ResetTransientState();
  EXPECT_FALSE(session_->HasMoreToSync());
}

TEST_F(SyncSessionTest, MoreToSyncIfConflictSetsBuilt) {
  // If we built conflict sets, then we need to loop back and try
  // to get updates & commit again.
  status()->set_conflict_sets_built(true);
  EXPECT_TRUE(session_->HasMoreToSync());
  status()->ResetTransientState();
  EXPECT_FALSE(session_->HasMoreToSync());
}

TEST_F(SyncSessionTest, MoreToSyncIfDidNotGetZeroUpdates) {
  // We're not done getting updates until we get an empty response.
  ClientToServerResponse response;
  response.mutable_get_updates()->add_entries();
  status()->mutable_updates_response()->CopyFrom(response);
  EXPECT_TRUE(session_->HasMoreToSync());
  status()->mutable_updates_response()->Clear();
  EXPECT_FALSE(session_->HasMoreToSync());
  status()->mutable_updates_response()->CopyFrom(response);
  EXPECT_TRUE(session_->HasMoreToSync());
  status()->ResetTransientState();
  EXPECT_FALSE(session_->HasMoreToSync());
}

TEST_F(SyncSessionTest, MoreToSyncIfConflictsResolved) {
  // Conflict resolution happens after get updates and commit,
  // so we need to loop back and get updates / commit again now
  // that we have made forward progress.
  status()->set_conflicts_resolved(true);
  EXPECT_TRUE(session_->HasMoreToSync());
  status()->ResetTransientState();
  EXPECT_FALSE(session_->HasMoreToSync());
}

TEST_F(SyncSessionTest, MoreToSyncIfTimestampDirty) {
  // If there are more changes on the server that weren't processed during this
  // GetUpdates request, the client should send another GetUpdates request and
  // use new_timestamp as the from_timestamp value within GetUpdatesMessage.
  status()->set_timestamp_dirty(true);
  status()->set_conflicts_resolved(true);
  EXPECT_TRUE(session_->HasMoreToSync());
  status()->ResetTransientState();
  EXPECT_FALSE(session_->HasMoreToSync());
}


}  // namespace
}  // namespace sessions
}  // namespace browser_sync
