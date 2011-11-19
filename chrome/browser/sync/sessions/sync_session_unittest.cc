// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sessions/sync_session.h"

#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop.h"
#include "chrome/browser/sync/engine/conflict_resolver.h"
#include "chrome/browser/sync/engine/mock_model_safe_workers.h"
#include "chrome/browser/sync/engine/syncer_types.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "chrome/browser/sync/syncable/syncable.h"
#include "chrome/browser/sync/test/engine/test_directory_setter_upper.h"
#include "testing/gtest/include/gtest/gtest.h"

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

  SyncSession* MakeSession() {
    return new SyncSession(context_.get(), this, SyncSourceInfo(), routes_,
                           std::vector<ModelSafeWorker*>());
  }

  virtual void SetUp() {
    context_.reset(new SyncSessionContext(NULL, NULL, this,
        std::vector<SyncEngineEventListener*>(), NULL));
    routes_.clear();
    routes_[syncable::BOOKMARKS] = GROUP_UI;
    routes_[syncable::AUTOFILL] = GROUP_UI;
    session_.reset(MakeSession());
  }
  virtual void TearDown() {
    session_.reset();
    context_.reset();
  }

  virtual void OnSilencedUntil(const base::TimeTicks& silenced_until) OVERRIDE {
    FailControllerInvocationIfDisabled("OnSilencedUntil");
  }
  virtual bool IsSyncingCurrentlySilenced() OVERRIDE {
    FailControllerInvocationIfDisabled("IsSyncingCurrentlySilenced");
    return false;
  }
  virtual void OnReceivedLongPollIntervalUpdate(
      const base::TimeDelta& new_interval) OVERRIDE {
    FailControllerInvocationIfDisabled("OnReceivedLongPollIntervalUpdate");
  }
  virtual void OnReceivedShortPollIntervalUpdate(
      const base::TimeDelta& new_interval) OVERRIDE {
    FailControllerInvocationIfDisabled("OnReceivedShortPollIntervalUpdate");
  }
  virtual void OnReceivedSessionsCommitDelay(
      const base::TimeDelta& new_delay) OVERRIDE {
    FailControllerInvocationIfDisabled("OnReceivedSessionsCommitDelay");
  }
  virtual void OnShouldStopSyncingPermanently() OVERRIDE {
    FailControllerInvocationIfDisabled("OnShouldStopSyncingPermanently");
  }
  virtual void OnSyncProtocolError(
      const sessions::SyncSessionSnapshot& snapshot) {
    FailControllerInvocationIfDisabled("SyncProtocolError");
  }

  // ModelSafeWorkerRegistrar implementation.
  virtual void GetWorkers(std::vector<ModelSafeWorker*>* out) OVERRIDE {}
  virtual void GetModelSafeRoutingInfo(ModelSafeRoutingInfo* out) OVERRIDE {
    out->swap(routes_);
  }

  StatusController* status() { return session_->status_controller(); }
 protected:
  void FailControllerInvocationIfDisabled(const std::string& msg) {
    if (!controller_invocations_allowed_)
      FAIL() << msg;
  }

  syncable::ModelTypeBitSet ParamsMeaningAllEnabledTypes() {
    syncable::ModelTypeBitSet request_params;
    request_params[syncable::BOOKMARKS] = true;
    request_params[syncable::AUTOFILL] = true;
    return request_params;
  }

  syncable::ModelTypeBitSet ParamsMeaningJustOneEnabledType() {
    syncable::ModelTypeBitSet request_params;
    request_params[syncable::AUTOFILL] = true;
    return request_params;
  }

  MessageLoop message_loop_;
  bool controller_invocations_allowed_;
  scoped_ptr<SyncSession> session_;
  scoped_ptr<SyncSessionContext> context_;
  ModelSafeRoutingInfo routes_;
};

TEST_F(SyncSessionTest, ScopedContextHelpers) {
  ConflictResolver resolver;
  EXPECT_FALSE(context_->resolver());
  {
    ScopedSessionContextConflictResolver s_resolver(context_.get(), &resolver);
    EXPECT_EQ(&resolver, context_->resolver());
  }
  EXPECT_FALSE(context_->resolver());
}

TEST_F(SyncSessionTest, SetWriteTransaction) {
  TestDirectorySetterUpper db;
  db.SetUp();
  session_.reset();
  context_.reset(new SyncSessionContext(NULL, db.manager(), this,
      std::vector<SyncEngineEventListener*>(), NULL));
  session_.reset(MakeSession());
  context_->set_account_name(db.name());
  syncable::ScopedDirLookup dir(context_->directory_manager(),
                                context_->account_name());
  ASSERT_TRUE(dir.good());

  scoped_ptr<SyncSession> session(MakeSession());
  EXPECT_TRUE(NULL == session->write_transaction());
  {
    WriteTransaction trans(FROM_HERE, syncable::UNITTEST, dir);
    sessions::ScopedSetSessionWriteTransaction set_trans(session.get(), &trans);
    EXPECT_TRUE(&trans == session->write_transaction());
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
  status()->set_updates_request_types(ParamsMeaningAllEnabledTypes());

  // When DownloadUpdatesCommand fails, these should be false.
  EXPECT_FALSE(status()->ServerSaysNothingMoreToDownload());
  EXPECT_FALSE(status()->download_updates_succeeded());

  // Download updates has its own loop in the syncer; it shouldn't factor
  // into HasMoreToSync.
  EXPECT_FALSE(session_->HasMoreToSync());
}

TEST_F(SyncSessionTest, MoreToDownloadIfGotChangesRemaining) {
  status()->set_updates_request_types(ParamsMeaningAllEnabledTypes());

  // When the server returns changes_remaining, that means there's
  // more to download.
  status()->mutable_updates_response()->mutable_get_updates()
     ->set_changes_remaining(1000L);
  EXPECT_FALSE(status()->ServerSaysNothingMoreToDownload());
  EXPECT_TRUE(status()->download_updates_succeeded());

  // Download updates has its own loop in the syncer; it shouldn't factor
  // into HasMoreToSync.
  EXPECT_FALSE(session_->HasMoreToSync());
}

TEST_F(SyncSessionTest, MoreToDownloadIfGotNoChangesRemaining) {
  status()->set_updates_request_types(ParamsMeaningAllEnabledTypes());

  // When the server returns a timestamp, that means we're up to date.
  status()->mutable_updates_response()->mutable_get_updates()
      ->set_changes_remaining(0);
  EXPECT_TRUE(status()->ServerSaysNothingMoreToDownload());
  EXPECT_TRUE(status()->download_updates_succeeded());

  // Download updates has its own loop in the syncer; it shouldn't factor
  // into HasMoreToSync.
  EXPECT_FALSE(session_->HasMoreToSync());
}

TEST_F(SyncSessionTest, MoreToDownloadIfGotNoChangesRemainingForSubset) {
  status()->set_updates_request_types(ParamsMeaningJustOneEnabledType());

  // When the server returns a timestamp, that means we're up to date for that
  // type.  But there may still be more to download if there are other
  // datatypes that we didn't request on this go-round.
  status()->mutable_updates_response()->mutable_get_updates()
      ->set_changes_remaining(0);

  EXPECT_TRUE(status()->ServerSaysNothingMoreToDownload());
  EXPECT_TRUE(status()->download_updates_succeeded());

  // Download updates has its own loop in the syncer; it shouldn't factor
  // into HasMoreToSync.
  EXPECT_FALSE(session_->HasMoreToSync());
}

TEST_F(SyncSessionTest, MoreToDownloadIfGotChangesRemainingAndEntries) {
  status()->set_updates_request_types(ParamsMeaningAllEnabledTypes());
  // The actual entry count should not factor into the HasMoreToSync
  // determination.
  status()->mutable_updates_response()->mutable_get_updates()->add_entries();
  status()->mutable_updates_response()->mutable_get_updates()
      ->set_changes_remaining(1000000L);;
  EXPECT_FALSE(status()->ServerSaysNothingMoreToDownload());
  EXPECT_TRUE(status()->download_updates_succeeded());

  // Download updates has its own loop in the syncer; it shouldn't factor
  // into HasMoreToSync.
  EXPECT_FALSE(session_->HasMoreToSync());
}

TEST_F(SyncSessionTest, MoreToDownloadIfGotNoChangesRemainingAndEntries) {
  status()->set_updates_request_types(ParamsMeaningAllEnabledTypes());
  // The actual entry count should not factor into the HasMoreToSync
  // determination.
  status()->mutable_updates_response()->mutable_get_updates()->add_entries();
  status()->mutable_updates_response()->mutable_get_updates()
      ->set_changes_remaining(0);
  EXPECT_TRUE(status()->ServerSaysNothingMoreToDownload());
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

TEST_F(SyncSessionTest, ResetTransientState) {
  status()->update_conflicts_resolved(true);
  status()->increment_num_successful_commits();
  EXPECT_TRUE(session_->HasMoreToSync());
  session_->ResetTransientState();
  EXPECT_FALSE(status()->conflicts_resolved());
  EXPECT_FALSE(session_->HasMoreToSync());
  EXPECT_FALSE(status()->TestAndClearIsDirty());
}

TEST_F(SyncSessionTest, Coalesce) {
  std::vector<ModelSafeWorker*> workers_one, workers_two;
  ModelSafeRoutingInfo routes_one, routes_two;
  syncable::ModelTypePayloadMap one_type =
      syncable::ModelTypePayloadMapFromBitSet(
          ParamsMeaningJustOneEnabledType(),
          std::string());
  syncable::ModelTypePayloadMap all_types =
      syncable::ModelTypePayloadMapFromBitSet(
          ParamsMeaningAllEnabledTypes(),
          std::string());
  SyncSourceInfo source_one(sync_pb::GetUpdatesCallerInfo::PERIODIC, one_type);
  SyncSourceInfo source_two(sync_pb::GetUpdatesCallerInfo::LOCAL, all_types);

  scoped_refptr<MockDBModelWorker> db_worker(new MockDBModelWorker());
  scoped_refptr<MockUIModelWorker> ui_worker(new MockUIModelWorker());
  workers_one.push_back(db_worker);
  workers_two.push_back(db_worker);
  workers_two.push_back(ui_worker);
  routes_one[syncable::AUTOFILL] = GROUP_DB;
  routes_two[syncable::AUTOFILL] = GROUP_DB;
  routes_two[syncable::BOOKMARKS] = GROUP_UI;
  SyncSession one(context_.get(), this, source_one, routes_one, workers_one);
  SyncSession two(context_.get(), this, source_two, routes_two, workers_two);

  one.Coalesce(two);

  EXPECT_EQ(two.source().updates_source, one.source().updates_source);
  EXPECT_EQ(all_types, one.source().types);
  std::vector<ModelSafeWorker*>::const_iterator it_db =
      std::find(one.workers().begin(), one.workers().end(), db_worker);
  std::vector<ModelSafeWorker*>::const_iterator it_ui =
      std::find(one.workers().begin(), one.workers().end(), ui_worker);
  EXPECT_NE(it_db, one.workers().end());
  EXPECT_NE(it_ui, one.workers().end());
  EXPECT_EQ(routes_two, one.routing_info());
}

TEST_F(SyncSessionTest, RebaseRoutingInfoWithLatestRemoveOneType) {
  std::vector<ModelSafeWorker*> workers_one, workers_two;
  ModelSafeRoutingInfo routes_one, routes_two;
  syncable::ModelTypePayloadMap one_type =
      syncable::ModelTypePayloadMapFromBitSet(
          ParamsMeaningJustOneEnabledType(),
          std::string());
  syncable::ModelTypePayloadMap all_types =
      syncable::ModelTypePayloadMapFromBitSet(
          ParamsMeaningAllEnabledTypes(),
          std::string());
  SyncSourceInfo source_one(sync_pb::GetUpdatesCallerInfo::PERIODIC, one_type);
  SyncSourceInfo source_two(sync_pb::GetUpdatesCallerInfo::LOCAL, all_types);

  scoped_refptr<MockDBModelWorker> db_worker(new MockDBModelWorker());
  scoped_refptr<MockUIModelWorker> ui_worker(new MockUIModelWorker());
  workers_one.push_back(db_worker);
  workers_two.push_back(db_worker);
  workers_two.push_back(ui_worker);
  routes_one[syncable::AUTOFILL] = GROUP_DB;
  routes_two[syncable::AUTOFILL] = GROUP_UI;
  routes_two[syncable::BOOKMARKS] = GROUP_UI;
  SyncSession one(context_.get(), this, source_one, routes_one, workers_one);
  SyncSession two(context_.get(), this, source_two, routes_two, workers_two);

  two.RebaseRoutingInfoWithLatest(&one);

  // Make sure the source has not been touched.
  EXPECT_EQ(two.source().updates_source,
      sync_pb::GetUpdatesCallerInfo::LOCAL);

  // Make sure the payload is reduced to one.
  EXPECT_EQ(one_type, two.source().types);

  // Make sure the workers are udpated.
  std::vector<ModelSafeWorker*>::const_iterator it_db =
      std::find(two.workers().begin(), two.workers().end(), db_worker);
  std::vector<ModelSafeWorker*>::const_iterator it_ui =
      std::find(two.workers().begin(), two.workers().end(), ui_worker);
  EXPECT_NE(it_db, two.workers().end());
  EXPECT_EQ(it_ui, two.workers().end());
  EXPECT_EQ(two.workers().size(), 1U);

  // Make sure the model safe routing info is reduced to one type.
  ModelSafeRoutingInfo::const_iterator it =
      two.routing_info().find(syncable::AUTOFILL);
  EXPECT_NE(it, two.routing_info().end());
  EXPECT_EQ(it->second, GROUP_DB);
  EXPECT_EQ(two.routing_info().size(), 1U);
}

TEST_F(SyncSessionTest, RebaseRoutingInfoWithLatestWithSameType) {
  std::vector<ModelSafeWorker*> workers_first, workers_second;
  ModelSafeRoutingInfo routes_first, routes_second;
  syncable::ModelTypePayloadMap all_types =
      syncable::ModelTypePayloadMapFromBitSet(
          ParamsMeaningAllEnabledTypes(),
          std::string());
  SyncSourceInfo source_first(sync_pb::GetUpdatesCallerInfo::PERIODIC,
      all_types);
  SyncSourceInfo source_second(sync_pb::GetUpdatesCallerInfo::LOCAL,
      all_types);

  scoped_refptr<MockDBModelWorker> db_worker(new MockDBModelWorker());
  scoped_refptr<MockUIModelWorker> ui_worker(new MockUIModelWorker());
  workers_first.push_back(db_worker);
  workers_first.push_back(ui_worker);
  workers_second.push_back(db_worker);
  workers_second.push_back(ui_worker);
  routes_first[syncable::AUTOFILL] = GROUP_DB;
  routes_first[syncable::BOOKMARKS] = GROUP_UI;
  routes_second[syncable::AUTOFILL] = GROUP_DB;
  routes_second[syncable::BOOKMARKS] = GROUP_UI;
  SyncSession first(context_.get(), this, source_first, routes_first,
      workers_first);
  SyncSession second(context_.get(), this, source_second, routes_second,
      workers_second);

  second.RebaseRoutingInfoWithLatest(&first);

  // Make sure the source has not been touched.
  EXPECT_EQ(second.source().updates_source,
      sync_pb::GetUpdatesCallerInfo::LOCAL);

  // Make sure our payload is still the same.
  EXPECT_EQ(all_types, second.source().types);

  // Make sure the workers are still the same.
  std::vector<ModelSafeWorker*>::const_iterator it_db =
      std::find(second.workers().begin(), second.workers().end(), db_worker);
  std::vector<ModelSafeWorker*>::const_iterator it_ui =
      std::find(second.workers().begin(), second.workers().end(), ui_worker);
  EXPECT_NE(it_db, second.workers().end());
  EXPECT_NE(it_ui, second.workers().end());
  EXPECT_EQ(second.workers().size(), 2U);

  // Make sure the model safe routing info is reduced to first type.
  ModelSafeRoutingInfo::const_iterator it1 =
      second.routing_info().find(syncable::AUTOFILL);
  ModelSafeRoutingInfo::const_iterator it2 =
      second.routing_info().find(syncable::BOOKMARKS);

  EXPECT_NE(it1, second.routing_info().end());
  EXPECT_EQ(it1->second, GROUP_DB);

  EXPECT_NE(it2, second.routing_info().end());
  EXPECT_EQ(it2->second, GROUP_UI);
  EXPECT_EQ(second.routing_info().size(), 2U);
}


TEST_F(SyncSessionTest, MakeTypePayloadMapFromBitSet) {
  syncable::ModelTypeBitSet types;
  std::string payload = "test";
  syncable::ModelTypePayloadMap types_with_payloads =
      syncable::ModelTypePayloadMapFromBitSet(types,
                                              payload);
  EXPECT_TRUE(types_with_payloads.empty());

  types[syncable::BOOKMARKS] = true;
  types[syncable::PASSWORDS] = true;
  types[syncable::AUTOFILL] = true;
  payload = "test2";
  types_with_payloads = syncable::ModelTypePayloadMapFromBitSet(types, payload);

  ASSERT_EQ(3U, types_with_payloads.size());
  EXPECT_EQ(types_with_payloads[syncable::BOOKMARKS], payload);
  EXPECT_EQ(types_with_payloads[syncable::PASSWORDS], payload);
  EXPECT_EQ(types_with_payloads[syncable::AUTOFILL], payload);
}

TEST_F(SyncSessionTest, MakeTypePayloadMapFromRoutingInfo) {
  std::string payload = "test";
  syncable::ModelTypePayloadMap types_with_payloads
      = syncable::ModelTypePayloadMapFromRoutingInfo(routes_, payload);
  ASSERT_EQ(routes_.size(), types_with_payloads.size());
  for (ModelSafeRoutingInfo::iterator iter = routes_.begin();
       iter != routes_.end();
       ++iter) {
    EXPECT_EQ(payload, types_with_payloads[iter->first]);
  }
}

TEST_F(SyncSessionTest, CoalescePayloads) {
  syncable::ModelTypePayloadMap original;
  std::string empty_payload;
  std::string payload1 = "payload1";
  std::string payload2 = "payload2";
  std::string payload3 = "payload3";
  original[syncable::BOOKMARKS] = empty_payload;
  original[syncable::PASSWORDS] = payload1;
  original[syncable::AUTOFILL] = payload2;
  original[syncable::THEMES] = payload3;

  syncable::ModelTypePayloadMap update;
  update[syncable::BOOKMARKS] = empty_payload;  // Same.
  update[syncable::PASSWORDS] = empty_payload;  // Overwrite with empty.
  update[syncable::AUTOFILL] = payload1;        // Overwrite with non-empty.
  update[syncable::SESSIONS] = payload2;        // New.
  // Themes untouched.

  CoalescePayloads(&original, update);
  ASSERT_EQ(5U, original.size());
  EXPECT_EQ(empty_payload, original[syncable::BOOKMARKS]);
  EXPECT_EQ(payload1, original[syncable::PASSWORDS]);
  EXPECT_EQ(payload1, original[syncable::AUTOFILL]);
  EXPECT_EQ(payload2, original[syncable::SESSIONS]);
  EXPECT_EQ(payload3, original[syncable::THEMES]);
}

}  // namespace
}  // namespace sessions
}  // namespace browser_sync
