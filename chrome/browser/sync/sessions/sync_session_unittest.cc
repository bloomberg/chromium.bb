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

using syncable::MultiTypeTimeStamp;
using syncable::WriteTransaction;

namespace browser_sync {
namespace sessions {
namespace {

class SyncSessionTest : public testing::Test,
                        public SyncSession::Delegate,
                        public ModelSafeWorkerRegistrar {
 public:
  SyncSessionTest() : controller_invocations_allowed_(false) {
    GetModelSafeRoutingInfo(&routes_);
  }
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
  virtual void OnShouldStopSyncingPermanently() {
    FailControllerInvocationIfDisabled("OnShouldStopSyncingPermanently");
  }

  // ModelSafeWorkerRegistrar implementation.
  virtual void GetWorkers(std::vector<ModelSafeWorker*>* out) {}
  virtual void GetModelSafeRoutingInfo(ModelSafeRoutingInfo* out) {
    (*out)[syncable::BOOKMARKS] = GROUP_UI;
    (*out)[syncable::AUTOFILL] = GROUP_UI;
  }

  StatusController* status() { return session_->status_controller(); }
 protected:
  void FailControllerInvocationIfDisabled(const std::string& msg) {
    if (!controller_invocations_allowed_)
      FAIL() << msg;
  }

  MultiTypeTimeStamp ParamsMeaningAllEnabledTypes() {
    MultiTypeTimeStamp request_params;
    request_params.timestamp = 2000;
    request_params.data_types[syncable::BOOKMARKS] = true;
    request_params.data_types[syncable::AUTOFILL] = true;
    return request_params;
  }

  MultiTypeTimeStamp ParamsMeaningJustOneEnabledType() {
    MultiTypeTimeStamp request_params;
    request_params.timestamp = 5000;
    request_params.data_types[syncable::AUTOFILL] = true;
    return request_params;
  }

  bool controller_invocations_allowed_;
  scoped_ptr<SyncSession> session_;
  scoped_ptr<SyncSessionContext> context_;
  ModelSafeRoutingInfo routes_;
};

TEST_F(SyncSessionTest, ScopedContextHelpers) {
  ConflictResolver resolver;
  SyncerEventChannel* channel = new SyncerEventChannel();
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
  channel->Notify(SyncerEvent(SyncerEvent::SHUTDOWN_USE_WITH_CARE));
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
  OrderedCommitSet commit_set(routes_);
  commit_set.AddCommitItem(0, syncable::Id(), syncable::BOOKMARKS);
  status()->set_commit_set(commit_set);
  EXPECT_FALSE(session_->HasMoreToSync());

  std::vector<int64> unsynced_handles;
  unsynced_handles.push_back(1);
  unsynced_handles.push_back(2);
  status()->set_unsynced_handles(unsynced_handles);
  EXPECT_FALSE(session_->HasMoreToSync());
  status()->increment_num_successful_commits();
  EXPECT_TRUE(session_->HasMoreToSync());
}

TEST_F(SyncSessionTest, MoreToSyncIfConflictSetsBuilt) {
  // If we built conflict sets, then we need to loop back and try
  // to get updates & commit again.
  status()->update_conflict_sets_built(true);
  EXPECT_TRUE(session_->HasMoreToSync());
}

TEST_F(SyncSessionTest, MoreToDownloadIfDownloadFailed) {
  status()->set_updates_request_parameters(ParamsMeaningAllEnabledTypes());

  // When DownloadUpdatesCommand fails, these should be false.
  EXPECT_FALSE(status()->ServerSaysNothingMoreToDownload());
  EXPECT_FALSE(status()->download_updates_succeeded());

  // Download updates has its own loop in the syncer; it shouldn't factor
  // into HasMoreToSync.
  EXPECT_FALSE(session_->HasMoreToSync());
}

TEST_F(SyncSessionTest, MoreToDownloadIfGotTimestamp) {
  status()->set_updates_request_parameters(ParamsMeaningAllEnabledTypes());

  // When the server returns a timestamp, that means there's more to download.
  status()->mutable_updates_response()->mutable_get_updates()
      ->set_new_timestamp(1000000L);
  EXPECT_FALSE(status()->ServerSaysNothingMoreToDownload());
  EXPECT_TRUE(status()->download_updates_succeeded());

  // Download updates has its own loop in the syncer; it shouldn't factor
  // into HasMoreToSync.
  EXPECT_FALSE(session_->HasMoreToSync());
}

TEST_F(SyncSessionTest, MoreToDownloadIfGotNoTimestamp) {
  status()->set_updates_request_parameters(ParamsMeaningAllEnabledTypes());

  // When the server returns a timestamp, that means we're up to date.
  status()->mutable_updates_response()->mutable_get_updates()
      ->clear_new_timestamp();
  EXPECT_TRUE(status()->ServerSaysNothingMoreToDownload());
  EXPECT_TRUE(status()->download_updates_succeeded());

  // Download updates has its own loop in the syncer; it shouldn't factor
  // into HasMoreToSync.
  EXPECT_FALSE(session_->HasMoreToSync());
}

TEST_F(SyncSessionTest, MoreToDownloadIfGotNoTimestampForSubset) {
  status()->set_updates_request_parameters(ParamsMeaningJustOneEnabledType());

  // When the server returns a timestamp, that means we're up to date for that
  // type.  But there may still be more to download if there are other
  // datatypes that we didn't request on this go-round.
  status()->mutable_updates_response()->mutable_get_updates()
    ->clear_new_timestamp();
  EXPECT_FALSE(status()->ServerSaysNothingMoreToDownload());
  EXPECT_TRUE(status()->download_updates_succeeded());

  // Download updates has its own loop in the syncer; it shouldn't factor
  // into HasMoreToSync.
  EXPECT_FALSE(session_->HasMoreToSync());
}

TEST_F(SyncSessionTest, MoreToDownloadIfGotTimestampAndEntries) {
  status()->set_updates_request_parameters(ParamsMeaningAllEnabledTypes());
  // The actual entry count should not factor into the HasMoreToSync
  // determination.
  status()->mutable_updates_response()->mutable_get_updates()->add_entries();
  status()->mutable_updates_response()->mutable_get_updates()
      ->set_new_timestamp(1000000L);;
  EXPECT_FALSE(status()->ServerSaysNothingMoreToDownload());
  EXPECT_TRUE(status()->download_updates_succeeded());

  // Download updates has its own loop in the syncer; it shouldn't factor
  // into HasMoreToSync.
  EXPECT_FALSE(session_->HasMoreToSync());
}


TEST_F(SyncSessionTest, MoreToSyncIfConflictsResolved) {
  // Conflict resolution happens after get updates and commit,
  // so we need to loop back and get updates / commit again now
  // that we have made forward progress.
  status()->update_conflicts_resolved(true);
  EXPECT_TRUE(session_->HasMoreToSync());
}

}  // namespace
}  // namespace sessions
}  // namespace browser_sync
