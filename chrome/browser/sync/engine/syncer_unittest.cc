// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Syncer unit tests. Unfortunately a lot of these tests
// are outdated and need to be reworked and updated.

#include <list>
#include <map>
#include <set>
#include <string>

#include "base/at_exit.h"

#include "base/scoped_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/sync/engine/conflict_resolver.h"
#include "chrome/browser/sync/engine/get_commit_ids_command.h"
#include "chrome/browser/sync/engine/model_safe_worker.h"
#include "chrome/browser/sync/engine/net/server_connection_manager.h"
#include "chrome/browser/sync/engine/process_updates_command.h"
#include "chrome/browser/sync/engine/syncer.h"
#include "chrome/browser/sync/engine/syncer_proto_util.h"
#include "chrome/browser/sync/engine/syncer_util.h"
#include "chrome/browser/sync/engine/syncproto.h"
#include "chrome/browser/sync/protocol/sync.pb.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/browser/sync/syncable/syncable.h"
#include "chrome/browser/sync/util/closure.h"
#include "chrome/browser/sync/util/event_sys-inl.h"
#include "chrome/test/sync/engine/mock_server_connection.h"
#include "chrome/test/sync/engine/test_directory_setter_upper.h"
#include "chrome/test/sync/engine/test_id_factory.h"
#include "chrome/test/sync/engine/test_syncable_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::TimeDelta;

using std::map;
using std::multimap;
using std::set;
using std::string;

namespace browser_sync {

using syncable::BaseTransaction;
using syncable::Blob;
using syncable::CountEntriesWithName;
using syncable::Directory;
using syncable::Entry;
using syncable::ExtendedAttribute;
using syncable::ExtendedAttributeKey;
using syncable::GetFirstEntryWithName;
using syncable::GetOnlyEntryWithName;
using syncable::Id;
using syncable::MutableEntry;
using syncable::MutableExtendedAttribute;
using syncable::ReadTransaction;
using syncable::ScopedDirLookup;
using syncable::WriteTransaction;

using syncable::BASE_VERSION;
using syncable::CREATE;
using syncable::CREATE_NEW_UPDATE_ITEM;
using syncable::GET_BY_HANDLE;
using syncable::GET_BY_ID;
using syncable::GET_BY_TAG;
using syncable::ID;
using syncable::IS_DEL;
using syncable::IS_DIR;
using syncable::IS_UNAPPLIED_UPDATE;
using syncable::IS_UNSYNCED;
using syncable::META_HANDLE;
using syncable::MTIME;
using syncable::NEXT_ID;
using syncable::NON_UNIQUE_NAME;
using syncable::PARENT_ID;
using syncable::PREV_ID;
using syncable::SERVER_IS_DEL;
using syncable::SERVER_NON_UNIQUE_NAME;
using syncable::SERVER_PARENT_ID;
using syncable::SERVER_POSITION_IN_PARENT;
using syncable::SERVER_SPECIFICS;
using syncable::SERVER_VERSION;
using syncable::SINGLETON_TAG;
using syncable::SPECIFICS;
using syncable::UNITTEST;

using sessions::ConflictProgress;
using sessions::ScopedSetSessionWriteTransaction;
using sessions::StatusController;
using sessions::SyncSessionContext;
using sessions::SyncSession;

namespace {
const char* kTestData = "Hello World!";
const int kTestDataLen = 12;
const int64 kTestLogRequestTimestamp = 123456;
}  // namespace

class SyncerTest : public testing::Test,
                   public SyncSession::Delegate,
                   public ModelSafeWorkerRegistrar {
 protected:
  SyncerTest() : syncer_(NULL) {}

  // SyncSession::Delegate implementation.
  virtual void OnSilencedUntil(const base::TimeTicks& silenced_until) {
    FAIL() << "Should not get silenced.";
  }
  virtual bool IsSyncingCurrentlySilenced() {
    return false;
  }
  virtual void OnReceivedLongPollIntervalUpdate(
      const base::TimeDelta& new_interval) {
    last_long_poll_interval_received_ = new_interval;
  }
  virtual void OnReceivedShortPollIntervalUpdate(
      const base::TimeDelta& new_interval) {
    last_short_poll_interval_received_ = new_interval;
  }

  // ModelSafeWorkerRegistrar implementation.
  virtual void GetWorkers(std::vector<ModelSafeWorker*>* out) {
    out->push_back(worker_.get());
  }

  virtual void GetModelSafeRoutingInfo(ModelSafeRoutingInfo* out) {
    // We're just testing the sync engine here, so we shunt everything to
    // the SyncerThread.
    (*out)[syncable::BOOKMARKS] = GROUP_PASSIVE;
  }

  void HandleSyncerEvent(SyncerEvent event) {
    LOG(INFO) << "HandleSyncerEvent in unittest " << event.what_happened;
    // we only test for entry-specific events, not status changed ones.
    switch (event.what_happened) {
      case SyncerEvent::STATUS_CHANGED:
        // fall through
      case SyncerEvent::SYNC_CYCLE_ENDED:
        // fall through
      case SyncerEvent::COMMITS_SUCCEEDED:
        return;
      case SyncerEvent::SHUTDOWN_USE_WITH_CARE:
      case SyncerEvent::OVER_QUOTA:
      case SyncerEvent::REQUEST_SYNC_NUDGE:
        LOG(INFO) << "Handling event type " << event.what_happened;
        break;
      default:
        CHECK(false) << "Handling unknown error type in unit tests!!";
    }
    syncer_events_.insert(event);
  }

  void LoopSyncShare(Syncer* syncer) {
    bool should_loop = false;
    int loop_iterations = 0;
    do {
      ASSERT_LT(++loop_iterations, 100) << "infinite loop detected. please fix";
      should_loop = syncer->SyncShare(this);
    } while (should_loop);
  }

  virtual void SetUp() {
    syncdb_.SetUp();

    mock_server_.reset(
        new MockConnectionManager(syncdb_.manager(), syncdb_.name()));
    worker_.reset(new ModelSafeWorker());
    context_.reset(new SyncSessionContext(mock_server_.get(), syncdb_.manager(),
       this));
    context_->set_account_name(syncdb_.name());
    ASSERT_FALSE(context_->syncer_event_channel());
    ASSERT_FALSE(context_->resolver());
    syncer_ = new Syncer(context_.get());
    // The Syncer installs some components on the context.
    ASSERT_TRUE(context_->syncer_event_channel());
    ASSERT_TRUE(context_->resolver());

    hookup_.reset(NewEventListenerHookup(context_->syncer_event_channel(), this,
                                         &SyncerTest::HandleSyncerEvent));
    session_.reset(new SyncSession(context_.get(), this));

    ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
    CHECK(dir.good());
    ReadTransaction trans(dir, __FILE__, __LINE__);
    syncable::Directory::ChildHandles children;
    dir->GetChildHandles(&trans, trans.root_id(), &children);
    ASSERT_TRUE(0 == children.size());
    syncer_events_.clear();
    root_id_ = TestIdFactory::root();
    parent_id_ = ids_.MakeServer("parent id");
    child_id_ = ids_.MakeServer("child id");
  }

  virtual void TearDown() {
    mock_server_.reset();
    hookup_.reset();
    delete syncer_;
    syncer_ = NULL;
    syncdb_.TearDown();
  }
  void WriteTestDataToEntry(WriteTransaction* trans, MutableEntry* entry) {
    EXPECT_FALSE(entry->Get(IS_DIR));
    EXPECT_FALSE(entry->Get(IS_DEL));
    Blob test_value(kTestData, kTestData + kTestDataLen);
    ExtendedAttributeKey key(entry->Get(META_HANDLE), "DATA");
    MutableExtendedAttribute attr(trans, CREATE, key);
    attr.mutable_value()->swap(test_value);
    sync_pb::EntitySpecifics specifics;
    specifics.MutableExtension(sync_pb::bookmark)->set_url("http://demo/");
    specifics.MutableExtension(sync_pb::bookmark)->set_favicon("PNG");
    entry->Put(syncable::SPECIFICS, specifics);
    entry->Put(syncable::IS_UNSYNCED, true);
  }
  void VerifyTestDataInEntry(BaseTransaction* trans, Entry* entry) {
    EXPECT_FALSE(entry->Get(IS_DIR));
    EXPECT_FALSE(entry->Get(IS_DEL));
    Blob test_value(kTestData, kTestData + kTestDataLen);
    ExtendedAttributeKey key(entry->Get(META_HANDLE), "DATA");
    ExtendedAttribute attr(trans, GET_BY_HANDLE, key);
    EXPECT_FALSE(attr.is_deleted());
    EXPECT_TRUE(test_value == attr.value());
    VerifyTestBookmarkDataInEntry(entry);
  }
  void VerifyTestBookmarkDataInEntry(Entry* entry) {
    const sync_pb::EntitySpecifics& specifics = entry->Get(syncable::SPECIFICS);
    EXPECT_TRUE(specifics.HasExtension(sync_pb::bookmark));
    EXPECT_EQ("PNG", specifics.GetExtension(sync_pb::bookmark).favicon());
    EXPECT_EQ("http://demo/", specifics.GetExtension(sync_pb::bookmark).url());
  }

  void SyncRepeatedlyToTriggerConflictResolution(SyncSession* session) {
    // We should trigger after less than 6 syncs, but extra does no harm.
    for (int i = 0 ; i < 6 ; ++i)
      syncer_->SyncShare(session);
  }
  void SyncRepeatedlyToTriggerStuckSignal(SyncSession* session) {
    // We should trigger after less than 10 syncs, but we want to avoid brittle
    // tests.
    for (int i = 0 ; i < 12 ; ++i)
      syncer_->SyncShare(session);
  }
  sync_pb::EntitySpecifics DefaultBookmarkSpecifics() {
    sync_pb::EntitySpecifics result;
    result.MutableExtension(sync_pb::bookmark);
    return result;
  }
  // Enumeration of alterations to entries for commit ordering tests.
  enum EntryFeature {
    LIST_END = 0,  // Denotes the end of the list of features from below.
    SYNCED,  // Items are unsynced by default
    DELETED,
    OLD_MTIME,
    MOVED_FROM_ROOT,
  };

  struct CommitOrderingTest {
    // expected commit index.
    int commit_index;
    // Details about the item
    syncable::Id id;
    syncable::Id parent_id;
    EntryFeature features[10];

    static const CommitOrderingTest LAST_COMMIT_ITEM;
  };

  void RunCommitOrderingTest(CommitOrderingTest* test) {
    ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
    ASSERT_TRUE(dir.good());

    map<int, syncable::Id> expected_positions;
    {  // Transaction scope.
      WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
      while (!test->id.IsRoot()) {
        if (test->commit_index >= 0) {
          map<int, syncable::Id>::value_type entry(test->commit_index,
                                                   test->id);
          bool double_position = !expected_positions.insert(entry).second;
          ASSERT_FALSE(double_position) << "Two id's expected at one position";
        }
        string utf8_name = test->id.GetServerId();
        string name(utf8_name.begin(), utf8_name.end());
        MutableEntry entry(&trans, CREATE, test->parent_id, name);

        entry.Put(syncable::ID, test->id);
        if (test->id.ServerKnows()) {
          entry.Put(BASE_VERSION, 5);
          entry.Put(SERVER_VERSION, 5);
          entry.Put(SERVER_PARENT_ID, test->parent_id);
        }
        entry.Put(syncable::IS_DIR, true);
        entry.Put(syncable::IS_UNSYNCED, true);
        entry.Put(syncable::SPECIFICS, DefaultBookmarkSpecifics());
        // Set the time to 30 seconds in the future to reduce the chance of
        // flaky tests.
        int64 now_server_time = ClientTimeToServerTime(syncable::Now());
        int64 now_plus_30s = ServerTimeToClientTime(now_server_time + 30000);
        int64 now_minus_2h = ServerTimeToClientTime(now_server_time - 7200000);
        entry.Put(syncable::MTIME, now_plus_30s);
        for (size_t i = 0 ; i < arraysize(test->features) ; ++i) {
          switch (test->features[i]) {
            case LIST_END:
              break;
            case SYNCED:
              entry.Put(syncable::IS_UNSYNCED, false);
              break;
            case DELETED:
              entry.Put(syncable::IS_DEL, true);
              break;
            case OLD_MTIME:
              entry.Put(MTIME, now_minus_2h);
              break;
            case MOVED_FROM_ROOT:
              entry.Put(SERVER_PARENT_ID, trans.root_id());
              break;
            default:
              FAIL() << "Bad value in CommitOrderingTest list";
          }
        }
        test++;
      }
    }
    LoopSyncShare(syncer_);
    ASSERT_TRUE(expected_positions.size() ==
                mock_server_->committed_ids().size());
    // If this test starts failing, be aware other sort orders could be valid.
    for (size_t i = 0; i < expected_positions.size(); ++i) {
      EXPECT_TRUE(1 == expected_positions.count(i));
      EXPECT_TRUE(expected_positions[i] == mock_server_->committed_ids()[i]);
    }
  }

  void DoTruncationTest(const ScopedDirLookup& dir,
                        const vector<int64>& unsynced_handle_view,
                        const vector<syncable::Id>& expected_id_order) {
    // The expected order is "x", "b", "c", "e", truncated appropriately.
    for (size_t limit = expected_id_order.size() + 2; limit > 0; --limit) {
      StatusController* status = session_->status_controller();
      status->ResetTransientState();
      WriteTransaction wtrans(dir, UNITTEST, __FILE__, __LINE__);
      ScopedSetSessionWriteTransaction set_trans(session_.get(), &wtrans);
      status->set_unsynced_handles(unsynced_handle_view);

      GetCommitIdsCommand command(limit);
      command.BuildCommitIds(session_->status_controller()->unsynced_handles(),
          session_->write_transaction());
      vector<syncable::Id> output = command.ordered_commit_set_.GetCommitIds();
      size_t truncated_size = std::min(limit, expected_id_order.size());
      ASSERT_TRUE(truncated_size == output.size());
      for (size_t i = 0; i < truncated_size; ++i) {
        ASSERT_TRUE(expected_id_order[i] == output[i])
            << "At index " << i << " with batch size limited to " << limit;
      }
    }
  }

  int64 CreateUnsyncedDirectory(const string& entry_name,
      const string& idstring) {
    return CreateUnsyncedDirectory(entry_name,
        syncable::Id::CreateFromServerId(idstring));
  }

  int64 CreateUnsyncedDirectory(const string& entry_name,
      const syncable::Id& id) {
    ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
    EXPECT_TRUE(dir.good());
    WriteTransaction wtrans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry entry(&wtrans, syncable::CREATE, wtrans.root_id(),
                       entry_name);
    EXPECT_TRUE(entry.good());
    entry.Put(syncable::IS_UNSYNCED, true);
    entry.Put(syncable::IS_DIR, true);
    entry.Put(syncable::SPECIFICS, DefaultBookmarkSpecifics());
    entry.Put(syncable::BASE_VERSION, id.ServerKnows() ? 1 : 0);
    entry.Put(syncable::ID, id);
    return entry.Get(META_HANDLE);
  }

  // Some ids to aid tests. Only the root one's value is specific. The rest
  // are named for test clarity.
  // TODO(chron): Get rid of these inbuilt IDs. They only make it
  // more confusing.
  syncable::Id root_id_;
  syncable::Id parent_id_;
  syncable::Id child_id_;

  TestIdFactory ids_;

  TestDirectorySetterUpper syncdb_;
  scoped_ptr<MockConnectionManager> mock_server_;
  scoped_ptr<EventListenerHookup> hookup_;

  Syncer* syncer_;

  scoped_ptr<SyncSession> session_;
  scoped_ptr<SyncSessionContext> context_;
  std::set<SyncerEvent> syncer_events_;
  base::TimeDelta last_short_poll_interval_received_;
  base::TimeDelta last_long_poll_interval_received_;
  scoped_ptr<ModelSafeWorker> worker_;

  DISALLOW_COPY_AND_ASSIGN(SyncerTest);
};

TEST_F(SyncerTest, TestCallGatherUnsyncedEntries) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  ASSERT_TRUE(dir.good());
  {
    Syncer::UnsyncedMetaHandles handles;
    {
      ReadTransaction trans(dir, __FILE__, __LINE__);
      SyncerUtil::GetUnsyncedEntries(&trans, &handles);
    }
    ASSERT_TRUE(0 == handles.size());
  }
  // TODO(sync): When we can dynamically connect and disconnect the mock
  // ServerConnectionManager test disconnected GetUnsyncedEntries here. It's a
  // regression for a very old bug.
}

TEST_F(SyncerTest, GetCommitIdsCommandTruncates) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  ASSERT_TRUE(dir.good());
  int64 handle_c = CreateUnsyncedDirectory("C", ids_.MakeLocal("c"));
  int64 handle_x = CreateUnsyncedDirectory("X", ids_.MakeLocal("x"));
  int64 handle_b = CreateUnsyncedDirectory("B", ids_.MakeLocal("b"));
  int64 handle_d = CreateUnsyncedDirectory("D", ids_.MakeLocal("d"));
  int64 handle_e = CreateUnsyncedDirectory("E", ids_.MakeLocal("e"));
  {
    WriteTransaction wtrans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry entry_x(&wtrans, GET_BY_HANDLE, handle_x);
    MutableEntry entry_b(&wtrans, GET_BY_HANDLE, handle_b);
    MutableEntry entry_c(&wtrans, GET_BY_HANDLE, handle_c);
    MutableEntry entry_d(&wtrans, GET_BY_HANDLE, handle_d);
    MutableEntry entry_e(&wtrans, GET_BY_HANDLE, handle_e);
    entry_x.Put(SPECIFICS, DefaultBookmarkSpecifics());
    entry_b.Put(SPECIFICS, DefaultBookmarkSpecifics());
    entry_c.Put(SPECIFICS, DefaultBookmarkSpecifics());
    entry_d.Put(SPECIFICS, DefaultBookmarkSpecifics());
    entry_e.Put(SPECIFICS, DefaultBookmarkSpecifics());
    entry_b.Put(PARENT_ID, entry_x.Get(ID));
    entry_c.Put(PARENT_ID, entry_x.Get(ID));
    entry_c.PutPredecessor(entry_b.Get(ID));
    entry_d.Put(PARENT_ID, entry_b.Get(ID));
    entry_e.Put(PARENT_ID, entry_c.Get(ID));
  }

  // The arrangement is now: x (b (d) c (e)).
  vector<int64> unsynced_handle_view;
  vector<syncable::Id> expected_order;
  // The expected order is "x", "b", "c", "e", truncated appropriately.
  unsynced_handle_view.push_back(handle_e);
  expected_order.push_back(ids_.MakeLocal("x"));
  expected_order.push_back(ids_.MakeLocal("b"));
  expected_order.push_back(ids_.MakeLocal("c"));
  expected_order.push_back(ids_.MakeLocal("e"));
  DoTruncationTest(dir, unsynced_handle_view, expected_order);
}

// TODO(chron): More corner case unit tests around validation.
TEST_F(SyncerTest, TestCommitMetahandleIterator) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  ASSERT_TRUE(dir.good());
  StatusController* status = session_->status_controller();
  const vector<int64>& unsynced(status->unsynced_handles());

  {
    WriteTransaction wtrans(dir, UNITTEST, __FILE__, __LINE__);
    ScopedSetSessionWriteTransaction set_trans(session_.get(), &wtrans);

    GetCommitIdsCommand::OrderedCommitSet commit_set;
    GetCommitIdsCommand::CommitMetahandleIterator iterator(unsynced, &wtrans,
        &commit_set);
    EXPECT_FALSE(iterator.Valid());
    EXPECT_FALSE(iterator.Increment());
  }

  {
    vector<int64> session_metahandles;
    session_metahandles.push_back(CreateUnsyncedDirectory("test1", "testid1"));
    session_metahandles.push_back(CreateUnsyncedDirectory("test2", "testid2"));
    session_metahandles.push_back(CreateUnsyncedDirectory("test3", "testid3"));
    status->set_unsynced_handles(session_metahandles);

    WriteTransaction wtrans(dir, UNITTEST, __FILE__, __LINE__);
    ScopedSetSessionWriteTransaction set_trans(session_.get(), &wtrans);
    GetCommitIdsCommand::OrderedCommitSet commit_set;
    GetCommitIdsCommand::CommitMetahandleIterator iterator(unsynced, &wtrans,
        &commit_set);

    EXPECT_TRUE(iterator.Valid());
    EXPECT_TRUE(iterator.Current() == session_metahandles[0]);
    EXPECT_TRUE(iterator.Increment());

    EXPECT_TRUE(iterator.Valid());
    EXPECT_TRUE(iterator.Current() == session_metahandles[1]);
    EXPECT_TRUE(iterator.Increment());

    EXPECT_TRUE(iterator.Valid());
    EXPECT_TRUE(iterator.Current() == session_metahandles[2]);
    EXPECT_FALSE(iterator.Increment());

    EXPECT_FALSE(iterator.Valid());
  }
}

TEST_F(SyncerTest, TestGetUnsyncedAndSimpleCommit) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  ASSERT_TRUE(dir.good());
  string xattr_key = "key";
  {
    WriteTransaction wtrans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry parent(&wtrans, syncable::CREATE, wtrans.root_id(),
                        "Pete");
    ASSERT_TRUE(parent.good());
    parent.Put(syncable::IS_UNSYNCED, true);
    parent.Put(syncable::IS_DIR, true);
    parent.Put(syncable::SPECIFICS, DefaultBookmarkSpecifics());
    parent.Put(syncable::BASE_VERSION, 1);
    parent.Put(syncable::ID, parent_id_);
    MutableEntry child(&wtrans, syncable::CREATE, parent_id_, "Pete");
    ASSERT_TRUE(child.good());
    child.Put(syncable::ID, child_id_);
    child.Put(syncable::BASE_VERSION, 1);
    WriteTestDataToEntry(&wtrans, &child);
  }

  StatusController* status = session_->status_controller();
  syncer_->SyncShare(session_.get());
  EXPECT_TRUE(2 == status->unsynced_handles().size());
  ASSERT_TRUE(2 == mock_server_->committed_ids().size());
  // If this test starts failing, be aware other sort orders could be valid.
  EXPECT_TRUE(parent_id_ == mock_server_->committed_ids()[0]);
  EXPECT_TRUE(child_id_ == mock_server_->committed_ids()[1]);
  {
    ReadTransaction rt(dir, __FILE__, __LINE__);
    Entry entry(&rt, syncable::GET_BY_ID, child_id_);
    ASSERT_TRUE(entry.good());
    VerifyTestDataInEntry(&rt, &entry);
  }
}

TEST_F(SyncerTest, TestCommitListOrderingTwoItemsTall) {
  CommitOrderingTest items[] = {
    {1, ids_.FromNumber(-1001), ids_.FromNumber(-1000)},
    {0, ids_.FromNumber(-1000), ids_.FromNumber(0)},
    CommitOrderingTest::LAST_COMMIT_ITEM,
  };
  RunCommitOrderingTest(items);
}

TEST_F(SyncerTest, TestCommitListOrderingThreeItemsTall) {
  CommitOrderingTest items[] = {
    {1, ids_.FromNumber(-2001), ids_.FromNumber(-2000)},
    {0, ids_.FromNumber(-2000), ids_.FromNumber(0)},
    {2, ids_.FromNumber(-2002), ids_.FromNumber(-2001)},
    CommitOrderingTest::LAST_COMMIT_ITEM,
  };
  RunCommitOrderingTest(items);
}

TEST_F(SyncerTest, TestCommitListOrderingThreeItemsTallLimitedSize) {
  syncer_->set_max_commit_batch_size(2);
  CommitOrderingTest items[] = {
    {1, ids_.FromNumber(-2001), ids_.FromNumber(-2000)},
    {0, ids_.FromNumber(-2000), ids_.FromNumber(0)},
    {2, ids_.FromNumber(-2002), ids_.FromNumber(-2001)},
    CommitOrderingTest::LAST_COMMIT_ITEM,
  };
  RunCommitOrderingTest(items);
}

TEST_F(SyncerTest, TestCommitListOrderingSingleDeletedItem) {
  CommitOrderingTest items[] = {
    {0, ids_.FromNumber(1000), ids_.FromNumber(0), {DELETED}},
    CommitOrderingTest::LAST_COMMIT_ITEM,
  };
  RunCommitOrderingTest(items);
}

TEST_F(SyncerTest, TestCommitListOrderingSingleUncommittedDeletedItem) {
  CommitOrderingTest items[] = {
    {-1, ids_.FromNumber(-1000), ids_.FromNumber(0), {DELETED}},
    CommitOrderingTest::LAST_COMMIT_ITEM,
  };
  RunCommitOrderingTest(items);
}

TEST_F(SyncerTest, TestCommitListOrderingSingleDeletedItemWithUnroll) {
  CommitOrderingTest items[] = {
    {0, ids_.FromNumber(1000), ids_.FromNumber(0), {DELETED}},
    CommitOrderingTest::LAST_COMMIT_ITEM,
  };
  RunCommitOrderingTest(items);
}

TEST_F(SyncerTest,
       TestCommitListOrderingSingleLongDeletedItemWithUnroll) {
  CommitOrderingTest items[] = {
    {0, ids_.FromNumber(1000), ids_.FromNumber(0), {DELETED, OLD_MTIME}},
    CommitOrderingTest::LAST_COMMIT_ITEM,
  };
  RunCommitOrderingTest(items);
}

TEST_F(SyncerTest, TestCommitListOrderingTwoLongDeletedItemWithUnroll) {
  CommitOrderingTest items[] = {
    {0, ids_.FromNumber(1000), ids_.FromNumber(0), {DELETED, OLD_MTIME}},
    {-1, ids_.FromNumber(1001), ids_.FromNumber(1000), {DELETED, OLD_MTIME}},
    CommitOrderingTest::LAST_COMMIT_ITEM,
  };
  RunCommitOrderingTest(items);
}

TEST_F(SyncerTest, TestCommitListOrdering3LongDeletedItemsWithSizeLimit) {
  syncer_->set_max_commit_batch_size(2);
  CommitOrderingTest items[] = {
    {0, ids_.FromNumber(1000), ids_.FromNumber(0), {DELETED, OLD_MTIME}},
    {1, ids_.FromNumber(1001), ids_.FromNumber(0), {DELETED, OLD_MTIME}},
    {2, ids_.FromNumber(1002), ids_.FromNumber(0), {DELETED, OLD_MTIME}},
    CommitOrderingTest::LAST_COMMIT_ITEM,
  };
  RunCommitOrderingTest(items);
}

TEST_F(SyncerTest, TestCommitListOrderingTwoDeletedItemsWithUnroll) {
  CommitOrderingTest items[] = {
    {0, ids_.FromNumber(1000), ids_.FromNumber(0), {DELETED}},
    {-1, ids_.FromNumber(1001), ids_.FromNumber(1000), {DELETED}},
    CommitOrderingTest::LAST_COMMIT_ITEM,
  };
  RunCommitOrderingTest(items);
}

TEST_F(SyncerTest, TestCommitListOrderingComplexDeletionScenario) {
  CommitOrderingTest items[] = {
    { 0, ids_.FromNumber(1000), ids_.FromNumber(0), {DELETED, OLD_MTIME}},
    {-1, ids_.FromNumber(1001), ids_.FromNumber(0), {SYNCED}},
    {1, ids_.FromNumber(1002), ids_.FromNumber(1001), {DELETED, OLD_MTIME}},
    {-1, ids_.FromNumber(1003), ids_.FromNumber(1001), {SYNCED}},
    {2, ids_.FromNumber(1004), ids_.FromNumber(1003), {DELETED}},
    CommitOrderingTest::LAST_COMMIT_ITEM,
  };
  RunCommitOrderingTest(items);
}

TEST_F(SyncerTest,
       TestCommitListOrderingComplexDeletionScenarioWith2RecentDeletes) {
  CommitOrderingTest items[] = {
    { 0, ids_.FromNumber(1000), ids_.FromNumber(0), {DELETED, OLD_MTIME}},
    {-1, ids_.FromNumber(1001), ids_.FromNumber(0), {SYNCED}},
    {1, ids_.FromNumber(1002), ids_.FromNumber(1001), {DELETED, OLD_MTIME}},
    {-1, ids_.FromNumber(1003), ids_.FromNumber(1001), {SYNCED}},
    {2, ids_.FromNumber(1004), ids_.FromNumber(1003), {DELETED}},
    {3, ids_.FromNumber(1005), ids_.FromNumber(1003), {DELETED}},
    CommitOrderingTest::LAST_COMMIT_ITEM,
  };
  RunCommitOrderingTest(items);
}

TEST_F(SyncerTest, TestCommitListOrderingDeleteMovedItems) {
  CommitOrderingTest items[] = {
    {1, ids_.FromNumber(1000), ids_.FromNumber(0), {DELETED, OLD_MTIME}},
    {0, ids_.FromNumber(1001), ids_.FromNumber(1000), {DELETED, OLD_MTIME,
                                              MOVED_FROM_ROOT}},
    CommitOrderingTest::LAST_COMMIT_ITEM,
  };
  RunCommitOrderingTest(items);
}

TEST_F(SyncerTest, TestCommitListOrderingWithNesting) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  ASSERT_TRUE(dir.good());
  int64 now_server_time = ClientTimeToServerTime(syncable::Now());
  int64 now_minus_2h = ServerTimeToClientTime(now_server_time - 7200000);

  {
    WriteTransaction wtrans(dir, UNITTEST, __FILE__, __LINE__);
    {
      MutableEntry parent(&wtrans, syncable::CREATE, wtrans.root_id(),
                          "Bob");
      ASSERT_TRUE(parent.good());
      parent.Put(syncable::IS_UNSYNCED, true);
      parent.Put(syncable::IS_DIR, true);
      parent.Put(syncable::SPECIFICS, DefaultBookmarkSpecifics());
      parent.Put(syncable::ID, ids_.FromNumber(100));
      parent.Put(syncable::BASE_VERSION, 1);
      MutableEntry child(&wtrans, syncable::CREATE, ids_.FromNumber(100),
                         "Bob");
      ASSERT_TRUE(child.good());
      child.Put(syncable::IS_UNSYNCED, true);
      child.Put(syncable::IS_DIR, true);
      child.Put(syncable::SPECIFICS, DefaultBookmarkSpecifics());
      child.Put(syncable::ID, ids_.FromNumber(101));
      child.Put(syncable::BASE_VERSION, 1);
      MutableEntry grandchild(&wtrans, syncable::CREATE, ids_.FromNumber(101),
                              "Bob");
      ASSERT_TRUE(grandchild.good());
      grandchild.Put(syncable::ID, ids_.FromNumber(102));
      grandchild.Put(syncable::IS_UNSYNCED, true);
      grandchild.Put(syncable::SPECIFICS, DefaultBookmarkSpecifics());
      grandchild.Put(syncable::BASE_VERSION, 1);
    }
    {
      // Create three deleted items which deletions we expect to be sent to the
      // server.
      MutableEntry parent(&wtrans, syncable::CREATE, wtrans.root_id(),
                          "Pete");
      ASSERT_TRUE(parent.good());
      parent.Put(syncable::IS_UNSYNCED, true);
      parent.Put(syncable::IS_DIR, true);
      parent.Put(syncable::IS_DEL, true);
      parent.Put(syncable::ID, ids_.FromNumber(103));
      parent.Put(syncable::BASE_VERSION, 1);
      parent.Put(syncable::MTIME, now_minus_2h);
      MutableEntry child(&wtrans, syncable::CREATE, ids_.FromNumber(103),
                         "Pete");
      ASSERT_TRUE(child.good());
      child.Put(syncable::IS_UNSYNCED, true);
      child.Put(syncable::IS_DIR, true);
      child.Put(syncable::IS_DEL, true);
      child.Put(syncable::ID, ids_.FromNumber(104));
      child.Put(syncable::BASE_VERSION, 1);
      child.Put(syncable::MTIME, now_minus_2h);
      MutableEntry grandchild(&wtrans, syncable::CREATE, ids_.FromNumber(104),
                              "Pete");
      ASSERT_TRUE(grandchild.good());
      grandchild.Put(syncable::IS_UNSYNCED, true);
      grandchild.Put(syncable::ID, ids_.FromNumber(105));
      grandchild.Put(syncable::IS_DEL, true);
      grandchild.Put(syncable::IS_DIR, false);
      grandchild.Put(syncable::BASE_VERSION, 1);
      grandchild.Put(syncable::MTIME, now_minus_2h);
    }
  }

  syncer_->SyncShare(session_.get());
  EXPECT_TRUE(6 == session_->status_controller()->unsynced_handles().size());
  ASSERT_TRUE(6 == mock_server_->committed_ids().size());
  // This test will NOT unroll deletes because SERVER_PARENT_ID is not set.
  // It will treat these like moves.
  vector<syncable::Id> commit_ids(mock_server_->committed_ids());
  EXPECT_TRUE(ids_.FromNumber(100) == commit_ids[0]);
  EXPECT_TRUE(ids_.FromNumber(101) == commit_ids[1]);
  EXPECT_TRUE(ids_.FromNumber(102) == commit_ids[2]);
  // We don't guarantee the delete orders in this test, only that they occur
  // at the end.
  std::sort(commit_ids.begin() + 3, commit_ids.end());
  EXPECT_TRUE(ids_.FromNumber(103) == commit_ids[3]);
  EXPECT_TRUE(ids_.FromNumber(104) == commit_ids[4]);
  EXPECT_TRUE(ids_.FromNumber(105) == commit_ids[5]);
}

TEST_F(SyncerTest, TestCommitListOrderingWithNewItems) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  ASSERT_TRUE(dir.good());
  {
    WriteTransaction wtrans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry parent(&wtrans, syncable::CREATE, wtrans.root_id(), "1");
    ASSERT_TRUE(parent.good());
    parent.Put(syncable::IS_UNSYNCED, true);
    parent.Put(syncable::IS_DIR, true);
    parent.Put(syncable::SPECIFICS, DefaultBookmarkSpecifics());
    parent.Put(syncable::ID, parent_id_);
    MutableEntry child(&wtrans, syncable::CREATE, wtrans.root_id(), "2");
    ASSERT_TRUE(child.good());
    child.Put(syncable::IS_UNSYNCED, true);
    child.Put(syncable::IS_DIR, true);
    child.Put(syncable::SPECIFICS, DefaultBookmarkSpecifics());
    child.Put(syncable::ID, child_id_);
    parent.Put(syncable::BASE_VERSION, 1);
    child.Put(syncable::BASE_VERSION, 1);
  }
  {
    WriteTransaction wtrans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry parent(&wtrans, syncable::CREATE, parent_id_, "A");
    ASSERT_TRUE(parent.good());
    parent.Put(syncable::IS_UNSYNCED, true);
    parent.Put(syncable::IS_DIR, true);
    parent.Put(syncable::SPECIFICS, DefaultBookmarkSpecifics());
    parent.Put(syncable::ID, ids_.FromNumber(102));
    MutableEntry child(&wtrans, syncable::CREATE, parent_id_, "B");
    ASSERT_TRUE(child.good());
    child.Put(syncable::IS_UNSYNCED, true);
    child.Put(syncable::IS_DIR, true);
    child.Put(syncable::SPECIFICS, DefaultBookmarkSpecifics());
    child.Put(syncable::ID, ids_.FromNumber(-103));
    parent.Put(syncable::BASE_VERSION, 1);
  }
  {
    WriteTransaction wtrans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry parent(&wtrans, syncable::CREATE, child_id_, "A");
    ASSERT_TRUE(parent.good());
    parent.Put(syncable::IS_UNSYNCED, true);
    parent.Put(syncable::IS_DIR, true);
    parent.Put(syncable::SPECIFICS, DefaultBookmarkSpecifics());
    parent.Put(syncable::ID, ids_.FromNumber(-104));
    MutableEntry child(&wtrans, syncable::CREATE, child_id_, "B");
    ASSERT_TRUE(child.good());
    child.Put(syncable::IS_UNSYNCED, true);
    child.Put(syncable::IS_DIR, true);
    child.Put(syncable::SPECIFICS, DefaultBookmarkSpecifics());
    child.Put(syncable::ID, ids_.FromNumber(105));
    child.Put(syncable::BASE_VERSION, 1);
  }

  syncer_->SyncShare(session_.get());
  EXPECT_TRUE(6 == session_->status_controller()->unsynced_handles().size());
  ASSERT_TRUE(6 == mock_server_->committed_ids().size());
  // If this test starts failing, be aware other sort orders could be valid.
  EXPECT_TRUE(parent_id_ == mock_server_->committed_ids()[0]);
  EXPECT_TRUE(child_id_ == mock_server_->committed_ids()[1]);
  EXPECT_TRUE(ids_.FromNumber(102) == mock_server_->committed_ids()[2]);
  EXPECT_TRUE(ids_.FromNumber(-103) == mock_server_->committed_ids()[3]);
  EXPECT_TRUE(ids_.FromNumber(-104) == mock_server_->committed_ids()[4]);
  EXPECT_TRUE(ids_.FromNumber(105) == mock_server_->committed_ids()[5]);
}

TEST_F(SyncerTest, TestCommitListOrderingCounterexample) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  ASSERT_TRUE(dir.good());

  syncable::Id child2_id = ids_.NewServerId();

  {
    WriteTransaction wtrans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry parent(&wtrans, syncable::CREATE, wtrans.root_id(), "P");
    ASSERT_TRUE(parent.good());
    parent.Put(syncable::IS_UNSYNCED, true);
    parent.Put(syncable::IS_DIR, true);
    parent.Put(syncable::SPECIFICS, DefaultBookmarkSpecifics());
    parent.Put(syncable::ID, parent_id_);
    MutableEntry child1(&wtrans, syncable::CREATE, parent_id_, "1");
    ASSERT_TRUE(child1.good());
    child1.Put(syncable::IS_UNSYNCED, true);
    child1.Put(syncable::ID, child_id_);
    MutableEntry child2(&wtrans, syncable::CREATE, parent_id_, "2");
    ASSERT_TRUE(child2.good());
    child2.Put(syncable::IS_UNSYNCED, true);
    child2.Put(syncable::ID, child2_id);
    parent.Put(syncable::BASE_VERSION, 1);
    child1.Put(syncable::BASE_VERSION, 1);
    child2.Put(syncable::BASE_VERSION, 1);
  }

  syncer_->SyncShare(session_.get());
  EXPECT_TRUE(3 == session_->status_controller()->unsynced_handles().size());
  ASSERT_TRUE(3 == mock_server_->committed_ids().size());
  // If this test starts failing, be aware other sort orders could be valid.
  EXPECT_TRUE(parent_id_ == mock_server_->committed_ids()[0]);
  EXPECT_TRUE(child_id_ == mock_server_->committed_ids()[1]);
  EXPECT_TRUE(child2_id == mock_server_->committed_ids()[2]);
}

TEST_F(SyncerTest, TestCommitListOrderingAndNewParent) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  ASSERT_TRUE(dir.good());

  string parent1_name = "1";
  string parent2_name = "A";
  string child_name = "B";

  {
    WriteTransaction wtrans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry parent(&wtrans, syncable::CREATE, wtrans.root_id(),
                        parent1_name);
    ASSERT_TRUE(parent.good());
    parent.Put(syncable::IS_UNSYNCED, true);
    parent.Put(syncable::IS_DIR, true);
    parent.Put(syncable::SPECIFICS, DefaultBookmarkSpecifics());
    parent.Put(syncable::ID, parent_id_);
    parent.Put(syncable::BASE_VERSION, 1);
  }

  syncable::Id parent2_id = ids_.NewLocalId();
  syncable::Id child_id = ids_.NewServerId();
  {
    WriteTransaction wtrans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry parent2(&wtrans, syncable::CREATE, parent_id_, parent2_name);
    ASSERT_TRUE(parent2.good());
    parent2.Put(syncable::IS_UNSYNCED, true);
    parent2.Put(syncable::IS_DIR, true);
    parent2.Put(syncable::SPECIFICS, DefaultBookmarkSpecifics());
    parent2.Put(syncable::ID, parent2_id);

    MutableEntry child(&wtrans, syncable::CREATE, parent2_id, child_name);
    ASSERT_TRUE(child.good());
    child.Put(syncable::IS_UNSYNCED, true);
    child.Put(syncable::IS_DIR, true);
    child.Put(syncable::SPECIFICS, DefaultBookmarkSpecifics());
    child.Put(syncable::ID, child_id);
    child.Put(syncable::BASE_VERSION, 1);
  }

  syncer_->SyncShare(session_.get());
  EXPECT_TRUE(3 == session_->status_controller()->unsynced_handles().size());
  ASSERT_TRUE(3 == mock_server_->committed_ids().size());
  // If this test starts failing, be aware other sort orders could be valid.
  EXPECT_TRUE(parent_id_ == mock_server_->committed_ids()[0]);
  EXPECT_TRUE(parent2_id == mock_server_->committed_ids()[1]);
  EXPECT_TRUE(child_id == mock_server_->committed_ids()[2]);
  {
    ReadTransaction rtrans(dir, __FILE__, __LINE__);
    // Check that things committed correctly.
    Entry entry_1(&rtrans, syncable::GET_BY_ID, parent_id_);
    EXPECT_EQ(entry_1.Get(NON_UNIQUE_NAME), parent1_name);
    // Check that parent2 is a subfolder of parent1.
    EXPECT_EQ(1, CountEntriesWithName(&rtrans,
                                      parent_id_,
                                      parent2_name));

    // Parent2 was a local ID and thus should have changed on commit!
    Entry pre_commit_entry_parent2(&rtrans, syncable::GET_BY_ID, parent2_id);
    ASSERT_FALSE(pre_commit_entry_parent2.good());

    // Look up the new ID.
    Id parent2_committed_id =
        GetOnlyEntryWithName(&rtrans, parent_id_, parent2_name);
    EXPECT_TRUE(parent2_committed_id.ServerKnows());

    Entry child(&rtrans, syncable::GET_BY_ID, child_id);
    EXPECT_EQ(parent2_committed_id, child.Get(syncable::PARENT_ID));
  }
}

TEST_F(SyncerTest, TestCommitListOrderingAndNewParentAndChild) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  ASSERT_TRUE(dir.good());

  string parent_name = "1";
  string parent2_name = "A";
  string child_name = "B";

  {
    WriteTransaction wtrans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry parent(&wtrans,
                        syncable::CREATE,
                        wtrans.root_id(),
                        parent_name);
    ASSERT_TRUE(parent.good());
    parent.Put(syncable::IS_UNSYNCED, true);
    parent.Put(syncable::IS_DIR, true);
    parent.Put(syncable::SPECIFICS, DefaultBookmarkSpecifics());
    parent.Put(syncable::ID, parent_id_);
    parent.Put(syncable::BASE_VERSION, 1);
  }

  int64 meta_handle_a, meta_handle_b;
  const Id parent2_local_id = ids_.NewLocalId();
  const Id child_local_id = ids_.NewLocalId();
  {
    WriteTransaction wtrans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry parent2(&wtrans, syncable::CREATE, parent_id_, parent2_name);
    ASSERT_TRUE(parent2.good());
    parent2.Put(syncable::IS_UNSYNCED, true);
    parent2.Put(syncable::IS_DIR, true);
    parent2.Put(syncable::SPECIFICS, DefaultBookmarkSpecifics());

    parent2.Put(syncable::ID, parent2_local_id);
    meta_handle_a = parent2.Get(syncable::META_HANDLE);
    MutableEntry child(&wtrans, syncable::CREATE, parent2_local_id, child_name);
    ASSERT_TRUE(child.good());
    child.Put(syncable::IS_UNSYNCED, true);
    child.Put(syncable::IS_DIR, true);
    child.Put(syncable::SPECIFICS, DefaultBookmarkSpecifics());
    child.Put(syncable::ID, child_local_id);
    meta_handle_b = child.Get(syncable::META_HANDLE);
  }

  syncer_->SyncShare(session_.get());
  EXPECT_TRUE(3 == session_->status_controller()->unsynced_handles().size());
  ASSERT_TRUE(3 == mock_server_->committed_ids().size());
  // If this test starts failing, be aware other sort orders could be valid.
  EXPECT_TRUE(parent_id_ == mock_server_->committed_ids()[0]);
  EXPECT_TRUE(parent2_local_id == mock_server_->committed_ids()[1]);
  EXPECT_TRUE(child_local_id == mock_server_->committed_ids()[2]);
  {
    ReadTransaction rtrans(dir, __FILE__, __LINE__);

    Entry parent(&rtrans, syncable::GET_BY_ID,
                 GetOnlyEntryWithName(&rtrans, rtrans.root_id(), parent_name));
    ASSERT_TRUE(parent.good());
    EXPECT_TRUE(parent.Get(syncable::ID).ServerKnows());

    Entry parent2(&rtrans, syncable::GET_BY_ID,
                  GetOnlyEntryWithName(&rtrans, parent.Get(ID), parent2_name));
    ASSERT_TRUE(parent2.good());
    EXPECT_TRUE(parent2.Get(syncable::ID).ServerKnows());

    // Id changed on commit, so this should fail.
    Entry local_parent2_id_entry(&rtrans,
                                 syncable::GET_BY_ID,
                                 parent2_local_id);
    ASSERT_FALSE(local_parent2_id_entry.good());

    Entry entry_b(&rtrans, syncable::GET_BY_HANDLE, meta_handle_b);
    EXPECT_TRUE(entry_b.Get(syncable::ID).ServerKnows());
    EXPECT_TRUE(parent2.Get(syncable::ID) == entry_b.Get(syncable::PARENT_ID));
  }
}

TEST_F(SyncerTest, UpdateWithZeroLengthName) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  ASSERT_TRUE(dir.good());
  // One illegal update
  mock_server_->AddUpdateDirectory(1, 0, "", 1, 10);
  // And one legal one that we're going to delete.
  mock_server_->AddUpdateDirectory(2, 0, "FOO", 1, 10);
  syncer_->SyncShare(this);
  // Delete the legal one. The new update has a null name.
  mock_server_->AddUpdateDirectory(2, 0, "", 2, 20);
  mock_server_->SetLastUpdateDeleted();
  syncer_->SyncShare(this);
}

TEST_F(SyncerTest, DontGetStuckWithTwoSameNames) {
  // We should not get stuck here because we get
  // two server updates with exactly the same name.
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  ASSERT_TRUE(dir.good());
  mock_server_->AddUpdateDirectory(1, 0, "foo:", 1, 10);
  syncer_->SyncShare(this);
  mock_server_->AddUpdateDirectory(2, 0, "foo:", 1, 20);
  SyncRepeatedlyToTriggerStuckSignal(session_.get());
  EXPECT_FALSE(session_->status_controller()->syncer_status().syncer_stuck);
  syncer_events_.clear();
}

TEST_F(SyncerTest, ExtendedAttributeWithNullCharacter) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  ASSERT_TRUE(dir.good());
  size_t xattr_count = 2;
  string xattr_keys[] = { "key", "key2" };
  syncable::Blob xattr_values[2];
  const char* value[] = { "value", "val\0ue" };
  int value_length[] = { 5, 6 };
  for (size_t i = 0; i < xattr_count; i++) {
    for (int j = 0; j < value_length[i]; j++)
      xattr_values[i].push_back(value[i][j]);
  }
  sync_pb::SyncEntity* ent =
      mock_server_->AddUpdateBookmark(1, 0, "bob", 1, 10);
  mock_server_->AddUpdateExtendedAttributes(
      ent, xattr_keys, xattr_values, xattr_count);

  // Add some other items.
  mock_server_->AddUpdateBookmark(2, 0, "fred", 2, 10);
  mock_server_->AddUpdateBookmark(3, 0, "sue", 15, 10);

  syncer_->SyncShare(this);
  ReadTransaction trans(dir, __FILE__, __LINE__);
  Entry entry1(&trans, syncable::GET_BY_ID, ids_.FromNumber(1));
  ASSERT_TRUE(entry1.good());
  EXPECT_TRUE(1 == entry1.Get(syncable::BASE_VERSION));
  EXPECT_TRUE(1 == entry1.Get(syncable::SERVER_VERSION));
  set<ExtendedAttribute> client_extended_attributes;
  entry1.GetAllExtendedAttributes(&trans, &client_extended_attributes);
  EXPECT_TRUE(xattr_count == client_extended_attributes.size());
  for (size_t i = 0; i < xattr_count; i++) {
    ExtendedAttributeKey key(entry1.Get(syncable::META_HANDLE), xattr_keys[i]);
    ExtendedAttribute expected_xattr(&trans, syncable::GET_BY_HANDLE, key);
    EXPECT_TRUE(expected_xattr.good());
    for (int j = 0; j < value_length[i]; ++j) {
      EXPECT_TRUE(xattr_values[i][j] ==
          static_cast<char>(expected_xattr.value().at(j)));
    }
  }
  Entry entry2(&trans, syncable::GET_BY_ID, ids_.FromNumber(2));
  ASSERT_TRUE(entry2.good());
  Entry entry3(&trans, syncable::GET_BY_ID, ids_.FromNumber(3));
  ASSERT_TRUE(entry3.good());
}

TEST_F(SyncerTest, TestBasicUpdate) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  ASSERT_TRUE(dir.good());
  string id = "some_id";
  string parent_id = "0";
  string name = "in_root";
  int64 version = 10;
  int64 timestamp = 10;
  mock_server_->AddUpdateDirectory(id, parent_id, name, version, timestamp);

  syncer_->SyncShare(this);
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    Entry entry(&trans, GET_BY_ID,
               syncable::Id::CreateFromServerId("some_id"));
    ASSERT_TRUE(entry.good());
    EXPECT_TRUE(entry.Get(IS_DIR));
    EXPECT_TRUE(entry.Get(SERVER_VERSION) == version);
    EXPECT_TRUE(entry.Get(BASE_VERSION) == version);
    EXPECT_FALSE(entry.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_FALSE(entry.Get(IS_UNSYNCED));
    EXPECT_FALSE(entry.Get(SERVER_IS_DEL));
    EXPECT_FALSE(entry.Get(IS_DEL));
  }
}

TEST_F(SyncerTest, IllegalAndLegalUpdates) {
  Id root = TestIdFactory::root();
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  ASSERT_TRUE(dir.good());
  // Should apply just fine.
  mock_server_->AddUpdateDirectory(1, 0, "in_root", 10, 10);

  // Same name. But this SHOULD work.
  mock_server_->AddUpdateDirectory(2, 0, "in_root", 10, 10);

  // Unknown parent: should never be applied. "-80" is a legal server ID,
  // because any string sent by the server is a legal server ID in the sync
  // protocol, but it's not the ID of any item known to the client.  This
  // update should succeed validation, but be stuck in the unapplied state
  // until an item with the server ID "-80" arrives.
  mock_server_->AddUpdateDirectory(3, -80, "bad_parent", 10, 10);

  syncer_->SyncShare(session_.get());
  StatusController* status = session_->status_controller();

  // Id 3 should be in conflict now.
  EXPECT_TRUE(1 == status->conflict_progress()->ConflictingItemsSize());

  // These entries will be used in the second set of updates.
  mock_server_->AddUpdateDirectory(4, 0, "newer_version", 20, 10);
  mock_server_->AddUpdateDirectory(5, 0, "circular1", 10, 10);
  mock_server_->AddUpdateDirectory(6, 5, "circular2", 10, 10);
  mock_server_->AddUpdateDirectory(9, 3, "bad_parent_child", 10, 10);
  mock_server_->AddUpdateDirectory(100, 9, "bad_parent_child2", 10, 10);
  mock_server_->AddUpdateDirectory(10, 0, "dir_to_bookmark", 10, 10);

  syncer_->SyncShare(session_.get());
  // The three items with an unresolved parent should be unapplied (3, 9, 100).
  // The name clash should also still be in conflict.
  EXPECT_TRUE(3 ==  status->conflict_progress()->ConflictingItemsSize());
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    // Even though it has the same name, it should work.
    Entry name_clash(&trans, GET_BY_ID, ids_.FromNumber(2));
    ASSERT_TRUE(name_clash.good());
    EXPECT_FALSE(name_clash.Get(IS_UNAPPLIED_UPDATE))
        << "Duplicate name SHOULD be OK.";

    Entry bad_parent(&trans, GET_BY_ID, ids_.FromNumber(3));
    ASSERT_TRUE(bad_parent.good());
    EXPECT_TRUE(bad_parent.Get(IS_UNAPPLIED_UPDATE))
        << "child of unknown parent should be in conflict";

    Entry bad_parent_child(&trans, GET_BY_ID, ids_.FromNumber(9));
    ASSERT_TRUE(bad_parent_child.good());
    EXPECT_TRUE(bad_parent_child.Get(IS_UNAPPLIED_UPDATE))
        << "grandchild of unknown parent should be in conflict";

    Entry bad_parent_child2(&trans, GET_BY_ID, ids_.FromNumber(100));
    ASSERT_TRUE(bad_parent_child2.good());
    EXPECT_TRUE(bad_parent_child2.Get(IS_UNAPPLIED_UPDATE))
        << "great-grandchild of unknown parent should be in conflict";
  }

  // Updating 1 should not affect item 2 of the same name.
  mock_server_->AddUpdateDirectory(1, 0, "new_name", 20, 20);

  // Moving 5 under 6 will create a cycle: a conflict.
  mock_server_->AddUpdateDirectory(5, 6, "circular3", 20, 20);

  // Flip the is_dir bit: should fail verify & be dropped.
  mock_server_->AddUpdateBookmark(10, 0, "dir_to_bookmark", 20, 20);
  syncer_->SyncShare(session_.get());

  // Version number older than last known: should fail verify & be dropped.
  mock_server_->AddUpdateDirectory(4, 0, "old_version", 10, 10);
  syncer_->SyncShare(session_.get());
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);

    Entry still_a_dir(&trans, GET_BY_ID, ids_.FromNumber(10));
    ASSERT_TRUE(still_a_dir.good());
    EXPECT_FALSE(still_a_dir.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_TRUE(10 == still_a_dir.Get(BASE_VERSION));
    EXPECT_TRUE(10 == still_a_dir.Get(SERVER_VERSION));
    EXPECT_TRUE(still_a_dir.Get(IS_DIR));

    Entry rename(&trans, GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(rename.good());
    EXPECT_EQ(root, rename.Get(PARENT_ID));
    EXPECT_EQ("new_name", rename.Get(NON_UNIQUE_NAME));
    EXPECT_FALSE(rename.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_TRUE(ids_.FromNumber(1) == rename.Get(ID));
    EXPECT_TRUE(20 == rename.Get(BASE_VERSION));

    Entry name_clash(&trans, GET_BY_ID, ids_.FromNumber(2));
    ASSERT_TRUE(name_clash.good());
    EXPECT_EQ(root, name_clash.Get(PARENT_ID));
    EXPECT_TRUE(ids_.FromNumber(2) == name_clash.Get(ID));
    EXPECT_TRUE(10 == name_clash.Get(BASE_VERSION));
    EXPECT_EQ("in_root", name_clash.Get(NON_UNIQUE_NAME));

    Entry ignored_old_version(&trans, GET_BY_ID, ids_.FromNumber(4));
    ASSERT_TRUE(ignored_old_version.good());
    EXPECT_TRUE(
        ignored_old_version.Get(NON_UNIQUE_NAME) == "newer_version");
    EXPECT_FALSE(ignored_old_version.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_TRUE(20 == ignored_old_version.Get(BASE_VERSION));

    Entry circular_parent_issue(&trans, GET_BY_ID, ids_.FromNumber(5));
    ASSERT_TRUE(circular_parent_issue.good());
    EXPECT_TRUE(circular_parent_issue.Get(IS_UNAPPLIED_UPDATE))
        << "circular move should be in conflict";
    EXPECT_TRUE(circular_parent_issue.Get(PARENT_ID) == root_id_);
    EXPECT_TRUE(circular_parent_issue.Get(SERVER_PARENT_ID) ==
                ids_.FromNumber(6));
    EXPECT_TRUE(10 == circular_parent_issue.Get(BASE_VERSION));

    Entry circular_parent_target(&trans, GET_BY_ID, ids_.FromNumber(6));
    ASSERT_TRUE(circular_parent_target.good());
    EXPECT_FALSE(circular_parent_target.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_TRUE(circular_parent_issue.Get(ID) ==
        circular_parent_target.Get(PARENT_ID));
    EXPECT_TRUE(10 == circular_parent_target.Get(BASE_VERSION));
  }

  EXPECT_TRUE(0 == syncer_events_.size());
  EXPECT_TRUE(4 == status->conflict_progress()->ConflictingItemsSize());
}

TEST_F(SyncerTest, CommitTimeRename) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  ASSERT_TRUE(dir.good());
  int64 metahandle_folder;
  int64 metahandle_new_entry;

  // Create a folder and an entry.
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry parent(&trans, CREATE, root_id_, "Folder");
    ASSERT_TRUE(parent.good());
    parent.Put(IS_DIR, true);
    parent.Put(syncable::SPECIFICS, DefaultBookmarkSpecifics());
    parent.Put(IS_UNSYNCED, true);
    metahandle_folder = parent.Get(META_HANDLE);

    MutableEntry entry(&trans, CREATE, parent.Get(ID), "new_entry");
    ASSERT_TRUE(entry.good());
    metahandle_new_entry = entry.Get(META_HANDLE);
    WriteTestDataToEntry(&trans, &entry);
  }

  // Mix in a directory creation too for later.
  mock_server_->AddUpdateDirectory(2, 0, "dir_in_root", 10, 10);
  mock_server_->SetCommitTimeRename("renamed_");
  syncer_->SyncShare(this);

  // Verify it was correctly renamed.
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    Entry entry_folder(&trans, GET_BY_HANDLE, metahandle_folder);
    ASSERT_TRUE(entry_folder.good());
    EXPECT_EQ("renamed_Folder", entry_folder.Get(NON_UNIQUE_NAME));

    Entry entry_new(&trans, GET_BY_HANDLE, metahandle_new_entry);
    ASSERT_TRUE(entry_new.good());
    EXPECT_EQ(entry_folder.Get(ID), entry_new.Get(PARENT_ID));
    EXPECT_EQ("renamed_new_entry", entry_new.Get(NON_UNIQUE_NAME));

    // And that the unrelated directory creation worked without a rename.
    Entry new_dir(&trans, GET_BY_ID, ids_.FromNumber(2));
    EXPECT_TRUE(new_dir.good());
    EXPECT_EQ("dir_in_root", new_dir.Get(NON_UNIQUE_NAME));
  }
}


TEST_F(SyncerTest, CommitTimeRenameI18N) {
  // This is utf-8 for the diacritized Internationalization.
  const char* i18nString = "\xc3\x8e\xc3\xb1\x74\xc3\xa9\x72\xc3\xb1"
      "\xc3\xa5\x74\xc3\xae\xc3\xb6\xc3\xb1\xc3\xa5\x6c\xc3\xae"
      "\xc2\x9e\xc3\xa5\x74\xc3\xae\xc3\xb6\xc3\xb1";

  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  ASSERT_TRUE(dir.good());
  int64 metahandle;
  // Create a folder, expect a commit time rename.
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry parent(&trans, CREATE, root_id_, "Folder");
    ASSERT_TRUE(parent.good());
    parent.Put(IS_DIR, true);
    parent.Put(SPECIFICS, DefaultBookmarkSpecifics());
    parent.Put(IS_UNSYNCED, true);
    metahandle = parent.Get(META_HANDLE);
  }

  mock_server_->SetCommitTimeRename(i18nString);
  syncer_->SyncShare(this);

  // Verify it was correctly renamed.
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    string expected_folder_name(i18nString);
    expected_folder_name.append("Folder");


    Entry entry_folder(&trans, GET_BY_HANDLE, metahandle);
    ASSERT_TRUE(entry_folder.good());
    EXPECT_EQ(expected_folder_name, entry_folder.Get(NON_UNIQUE_NAME));
  }
}

// A commit with a lost response produces an update that has to be reunited with
// its parent.
TEST_F(SyncerTest, CommitReuniteUpdateAdjustsChildren) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  ASSERT_TRUE(dir.good());

  // Create a folder in the root.
  int64 metahandle_folder;
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry entry(&trans, CREATE, trans.root_id(), "new_folder");
    ASSERT_TRUE(entry.good());
    entry.Put(IS_DIR, true);
    entry.Put(SPECIFICS, DefaultBookmarkSpecifics());
    entry.Put(IS_UNSYNCED, true);
    metahandle_folder = entry.Get(META_HANDLE);
  }

  // Verify it and pull the ID out of the folder.
  syncable::Id folder_id;
  int64 metahandle_entry;
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    Entry entry(&trans, GET_BY_HANDLE, metahandle_folder);
    ASSERT_TRUE(entry.good());
    folder_id = entry.Get(ID);
    ASSERT_TRUE(!folder_id.ServerKnows());
  }

  // Create an entry in the newly created folder.
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry entry(&trans, CREATE, folder_id, "new_entry");
    ASSERT_TRUE(entry.good());
    metahandle_entry = entry.Get(META_HANDLE);
    WriteTestDataToEntry(&trans, &entry);
  }

  // Verify it and pull the ID out of the entry.
  syncable::Id entry_id;
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    Entry entry(&trans, syncable::GET_BY_HANDLE, metahandle_entry);
    ASSERT_TRUE(entry.good());
    EXPECT_EQ(folder_id, entry.Get(PARENT_ID));
    EXPECT_EQ("new_entry", entry.Get(NON_UNIQUE_NAME));
    entry_id = entry.Get(ID);
    EXPECT_TRUE(!entry_id.ServerKnows());
    VerifyTestDataInEntry(&trans, &entry);
  }

  // Now, to emulate a commit response failure, we just don't commit it.
  int64 new_version = 150;  // any larger value.
  int64 timestamp = 20;  // arbitrary value.
  syncable::Id new_folder_id =
      syncable::Id::CreateFromServerId("folder_server_id");

  // The following update should cause the folder to both apply the update, as
  // well as reassociate the id.
  mock_server_->AddUpdateDirectory(new_folder_id, root_id_,
      "new_folder", new_version, timestamp);
  mock_server_->SetLastUpdateOriginatorFields(
      dir->cache_guid(), folder_id.GetServerId());

  // We don't want it accidentally committed, just the update applied.
  mock_server_->set_conflict_all_commits(true);

  // Alright! Apply that update!
  syncer_->SyncShare(this);
  {
    // The folder's ID should have been updated.
    ReadTransaction trans(dir, __FILE__, __LINE__);
    Entry folder(&trans, GET_BY_HANDLE, metahandle_folder);
    ASSERT_TRUE(folder.good());
    EXPECT_EQ("new_folder", folder.Get(NON_UNIQUE_NAME));
    EXPECT_TRUE(new_version == folder.Get(BASE_VERSION));
    EXPECT_TRUE(new_folder_id == folder.Get(ID));
    EXPECT_TRUE(folder.Get(ID).ServerKnows());
    EXPECT_EQ(trans.root_id(), folder.Get(PARENT_ID));

    // Since it was updated, the old folder should not exist.
    Entry old_dead_folder(&trans, GET_BY_ID, folder_id);
    EXPECT_FALSE(old_dead_folder.good());

    // The child's parent should have changed.
    Entry entry(&trans, syncable::GET_BY_HANDLE, metahandle_entry);
    ASSERT_TRUE(entry.good());
    EXPECT_EQ("new_entry", entry.Get(NON_UNIQUE_NAME));
    EXPECT_EQ(new_folder_id, entry.Get(PARENT_ID));
    EXPECT_TRUE(!entry.Get(ID).ServerKnows());
    VerifyTestDataInEntry(&trans, &entry);
  }
}

// A commit with a lost response produces an update that has to be reunited with
// its parent.
TEST_F(SyncerTest, CommitReuniteUpdate) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  ASSERT_TRUE(dir.good());

  // Create an entry in the root.
  int64 entry_metahandle;
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry entry(&trans, CREATE, trans.root_id(), "new_entry");
    ASSERT_TRUE(entry.good());
    entry_metahandle = entry.Get(META_HANDLE);
    WriteTestDataToEntry(&trans, &entry);
  }

  // Verify it and pull the ID out.
  syncable::Id entry_id;
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);

    Entry entry(&trans, GET_BY_HANDLE, entry_metahandle);
    ASSERT_TRUE(entry.good());
    entry_id = entry.Get(ID);
    EXPECT_TRUE(!entry_id.ServerKnows());
    VerifyTestDataInEntry(&trans, &entry);
  }

  // Now, to emulate a commit response failure, we just don't commit it.
  int64 new_version = 150;  // any larger value.
  int64 timestamp = 20;  // arbitrary value.
  syncable::Id new_entry_id = syncable::Id::CreateFromServerId("server_id");

  // Generate an update from the server with a relevant ID reassignment.
  mock_server_->AddUpdateBookmark(new_entry_id, root_id_,
      "new_entry", new_version, timestamp);
  mock_server_->SetLastUpdateOriginatorFields(
      dir->cache_guid(), entry_id.GetServerId());

  // We don't want it accidentally committed, just the update applied.
  mock_server_->set_conflict_all_commits(true);

  // Alright! Apply that update!
  syncer_->SyncShare(this);
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    Entry entry(&trans, GET_BY_HANDLE, entry_metahandle);
    ASSERT_TRUE(entry.good());
    EXPECT_TRUE(new_version == entry.Get(BASE_VERSION));
    EXPECT_TRUE(new_entry_id == entry.Get(ID));
    EXPECT_EQ("new_entry", entry.Get(NON_UNIQUE_NAME));
  }
}

// A commit with a lost response must work even if the local entry was deleted
// before the update is applied. We should not duplicate the local entry in
// this case, but just create another one alongside. We may wish to examine
// this behavior in the future as it can create hanging uploads that never
// finish, that must be cleaned up on the server side after some time.
TEST_F(SyncerTest, CommitReuniteUpdateDoesNotChokeOnDeletedLocalEntry) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  ASSERT_TRUE(dir.good());

  // Create a entry in the root.
  int64 entry_metahandle;
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry entry(&trans, CREATE, trans.root_id(), "new_entry");
    ASSERT_TRUE(entry.good());
    entry_metahandle = entry.Get(META_HANDLE);
    WriteTestDataToEntry(&trans, &entry);
  }
  // Verify it and pull the ID out.
  syncable::Id entry_id;
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    Entry entry(&trans, GET_BY_HANDLE, entry_metahandle);
    ASSERT_TRUE(entry.good());
    entry_id = entry.Get(ID);
    EXPECT_TRUE(!entry_id.ServerKnows());
    VerifyTestDataInEntry(&trans, &entry);
  }

  // Now, to emulate a commit response failure, we just don't commit it.
  int64 new_version = 150;  // any larger value.
  int64 timestamp = 20;  // arbitrary value.
  syncable::Id new_entry_id = syncable::Id::CreateFromServerId("server_id");

  // Generate an update from the server with a relevant ID reassignment.
  mock_server_->AddUpdateBookmark(new_entry_id, root_id_,
      "new_entry", new_version, timestamp);
  mock_server_->SetLastUpdateOriginatorFields(
      dir->cache_guid(),
      entry_id.GetServerId());

  // We don't want it accidentally committed, just the update applied.
  mock_server_->set_conflict_all_commits(true);

  // Purposefully delete the entry now before the update application finishes.
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    Id new_entry_id = GetOnlyEntryWithName(
        &trans, trans.root_id(), "new_entry");
    MutableEntry entry(&trans, GET_BY_ID, new_entry_id);
    ASSERT_TRUE(entry.good());
    entry.Put(syncable::IS_DEL, true);
  }

  // Just don't CHECK fail in sync, have the update split.
  syncer_->SyncShare(this);
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    Id new_entry_id = GetOnlyEntryWithName(
        &trans, trans.root_id(), "new_entry");
    Entry entry(&trans, GET_BY_ID, new_entry_id);
    ASSERT_TRUE(entry.good());
    EXPECT_FALSE(entry.Get(IS_DEL));

    Entry old_entry(&trans, GET_BY_ID, entry_id);
    ASSERT_TRUE(old_entry.good());
    EXPECT_TRUE(old_entry.Get(IS_DEL));
  }
}

// TODO(chron): Add more unsanitized name tests.
TEST_F(SyncerTest, ConflictMatchingEntryHandlesUnsanitizedNames) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());
  mock_server_->AddUpdateDirectory(1, 0, "A/A", 10, 10);
  mock_server_->AddUpdateDirectory(2, 0, "B/B", 10, 10);
  mock_server_->set_conflict_all_commits(true);
  syncer_->SyncShare(this);
  {
    WriteTransaction wtrans(dir, UNITTEST, __FILE__, __LINE__);

    MutableEntry A(&wtrans, GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(A.good());
    A.Put(IS_UNSYNCED, true);
    A.Put(IS_UNAPPLIED_UPDATE, true);
    A.Put(SERVER_VERSION, 20);

    MutableEntry B(&wtrans, GET_BY_ID, ids_.FromNumber(2));
    ASSERT_TRUE(B.good());
    B.Put(IS_UNAPPLIED_UPDATE, true);
    B.Put(SERVER_VERSION, 20);
  }
  LoopSyncShare(syncer_);
  syncer_events_.clear();
  mock_server_->set_conflict_all_commits(false);

  {
    ReadTransaction trans(dir, __FILE__, __LINE__);

    Entry A(&trans, GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(A.good());
    EXPECT_TRUE(A.Get(IS_UNSYNCED) == false);
    EXPECT_TRUE(A.Get(IS_UNAPPLIED_UPDATE) == false);
    EXPECT_TRUE(A.Get(SERVER_VERSION) == 20);

    Entry B(&trans, GET_BY_ID, ids_.FromNumber(2));
    ASSERT_TRUE(B.good());
    EXPECT_TRUE(B.Get(IS_UNSYNCED) == false);
    EXPECT_TRUE(B.Get(IS_UNAPPLIED_UPDATE) == false);
    EXPECT_TRUE(B.Get(SERVER_VERSION) == 20);
  }
}

TEST_F(SyncerTest, ConflictMatchingEntryHandlesNormalNames) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());
  mock_server_->AddUpdateDirectory(1, 0, "A", 10, 10);
  mock_server_->AddUpdateDirectory(2, 0, "B", 10, 10);
  mock_server_->set_conflict_all_commits(true);
  syncer_->SyncShare(this);
  {
    WriteTransaction wtrans(dir, UNITTEST, __FILE__, __LINE__);

    MutableEntry A(&wtrans, GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(A.good());
    A.Put(IS_UNSYNCED, true);
    A.Put(IS_UNAPPLIED_UPDATE, true);
    A.Put(SERVER_VERSION, 20);

    MutableEntry B(&wtrans, GET_BY_ID, ids_.FromNumber(2));
    ASSERT_TRUE(B.good());
    B.Put(IS_UNAPPLIED_UPDATE, true);
    B.Put(SERVER_VERSION, 20);
  }
  LoopSyncShare(syncer_);
  syncer_events_.clear();
  mock_server_->set_conflict_all_commits(false);

  {
    ReadTransaction trans(dir, __FILE__, __LINE__);

    Entry A(&trans, GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(A.good());
    EXPECT_TRUE(A.Get(IS_UNSYNCED) == false);
    EXPECT_TRUE(A.Get(IS_UNAPPLIED_UPDATE) == false);
    EXPECT_TRUE(A.Get(SERVER_VERSION) == 20);

    Entry B(&trans, GET_BY_ID, ids_.FromNumber(2));
    ASSERT_TRUE(B.good());
    EXPECT_TRUE(B.Get(IS_UNSYNCED) == false);
    EXPECT_TRUE(B.Get(IS_UNAPPLIED_UPDATE) == false);
    EXPECT_TRUE(B.Get(SERVER_VERSION) == 20);
  }
}

TEST_F(SyncerTest, ReverseFolderOrderingTest) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  ASSERT_TRUE(dir.good());
  mock_server_->AddUpdateDirectory(4, 3, "ggchild", 10, 10);
  mock_server_->AddUpdateDirectory(3, 2, "gchild", 10, 10);
  mock_server_->AddUpdateDirectory(5, 4, "gggchild", 10, 10);
  mock_server_->AddUpdateDirectory(2, 1, "child", 10, 10);
  mock_server_->AddUpdateDirectory(1, 0, "parent", 10, 10);
  LoopSyncShare(syncer_);
  ReadTransaction trans(dir, __FILE__, __LINE__);

  Id child_id = GetOnlyEntryWithName(
        &trans, ids_.FromNumber(4), "gggchild");
  Entry child(&trans, GET_BY_ID, child_id);
  ASSERT_TRUE(child.good());
}

class EntryCreatedInNewFolderTest : public SyncerTest {
 public:
  void CreateFolderInBob() {
    ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
    CHECK(dir.good());

    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry bob(&trans,
                     syncable::GET_BY_ID,
                     GetOnlyEntryWithName(&trans,
                                          TestIdFactory::root(),
                                          "bob"));
    CHECK(bob.good());

    MutableEntry entry2(&trans, syncable::CREATE, bob.Get(syncable::ID),
                        "bob");
    CHECK(entry2.good());
    entry2.Put(syncable::IS_DIR, true);
    entry2.Put(syncable::IS_UNSYNCED, true);
    entry2.Put(syncable::SPECIFICS, DefaultBookmarkSpecifics());
  }
};

TEST_F(EntryCreatedInNewFolderTest, EntryCreatedInNewFolderMidSync) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry entry(&trans, syncable::CREATE, trans.root_id(),
                       "bob");
    ASSERT_TRUE(entry.good());
    entry.Put(syncable::IS_DIR, true);
    entry.Put(syncable::IS_UNSYNCED, true);
    entry.Put(syncable::SPECIFICS, DefaultBookmarkSpecifics());
  }

  mock_server_->SetMidCommitCallback(
      NewCallback<EntryCreatedInNewFolderTest>(this,
          &EntryCreatedInNewFolderTest::CreateFolderInBob));
  syncer_->SyncShare(BUILD_COMMIT_REQUEST, SYNCER_END, this);
  EXPECT_TRUE(1 == mock_server_->committed_ids().size());
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    Entry parent_entry(&trans, syncable::GET_BY_ID,
        GetOnlyEntryWithName(&trans, TestIdFactory::root(), "bob"));
    ASSERT_TRUE(parent_entry.good());

    Id child_id =
        GetOnlyEntryWithName(&trans, parent_entry.Get(ID), "bob");
    Entry child(&trans, syncable::GET_BY_ID, child_id);
    ASSERT_TRUE(child.good());
    EXPECT_EQ(parent_entry.Get(ID), child.Get(PARENT_ID));
}
}

TEST_F(SyncerTest, NegativeIDInUpdate) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());
  mock_server_->AddUpdateBookmark(-10, 0, "bad", 40, 40);
  syncer_->SyncShare(this);
  // The negative id would make us CHECK!
}

TEST_F(SyncerTest, UnappliedUpdateOnCreatedItemItemDoesNotCrash) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());

  int64 metahandle_fred;
  {
    // Create an item.
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry fred_match(&trans, CREATE, trans.root_id(),
                            "fred_match");
    ASSERT_TRUE(fred_match.good());
    metahandle_fred = fred_match.Get(META_HANDLE);
    WriteTestDataToEntry(&trans, &fred_match);
  }
  // Commit it.
  syncer_->SyncShare(this);
  EXPECT_TRUE(1 == mock_server_->committed_ids().size());
  mock_server_->set_conflict_all_commits(true);
  syncable::Id fred_match_id;
  {
    // Now receive a change from outside.
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry fred_match(&trans, GET_BY_HANDLE, metahandle_fred);
    ASSERT_TRUE(fred_match.good());
    EXPECT_TRUE(fred_match.Get(ID).ServerKnows());
    fred_match_id = fred_match.Get(ID);
    mock_server_->AddUpdateBookmark(fred_match_id, trans.root_id(),
        "fred_match", 40, 40);
  }
  // Run the syncer.
  for (int i = 0 ; i < 30 ; ++i) {
  syncer_->SyncShare(this);
  }
}

/**
 * In the event that we have a double changed entry, that is changed on both
 * the client and the server, the conflict resolver should just drop one of
 * them and accept the other.
 */

TEST_F(SyncerTest, DoublyChangedWithResolver) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());
  {
    WriteTransaction wtrans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry parent(&wtrans, syncable::CREATE, root_id_, "Folder");
    ASSERT_TRUE(parent.good());
    parent.Put(syncable::IS_DIR, true);
    parent.Put(syncable::ID, parent_id_);
    parent.Put(syncable::BASE_VERSION, 5);
    parent.Put(syncable::SPECIFICS, DefaultBookmarkSpecifics());
    MutableEntry child(&wtrans, syncable::CREATE, parent_id_, "Pete.htm");
    ASSERT_TRUE(child.good());
    child.Put(syncable::ID, child_id_);
    child.Put(syncable::BASE_VERSION, 10);
    WriteTestDataToEntry(&wtrans, &child);
  }
  mock_server_->AddUpdateBookmark(child_id_, parent_id_, "Pete2.htm", 11, 10);
  mock_server_->set_conflict_all_commits(true);
  LoopSyncShare(syncer_);
  syncable::Directory::ChildHandles children;
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    dir->GetChildHandles(&trans, parent_id_, &children);
    // We expect the conflict resolver to preserve the local entry.
    Entry child(&trans, syncable::GET_BY_ID, child_id_);
    ASSERT_TRUE(child.good());
    EXPECT_TRUE(child.Get(syncable::IS_UNSYNCED));
    EXPECT_FALSE(child.Get(syncable::IS_UNAPPLIED_UPDATE));
    EXPECT_TRUE(child.Get(SPECIFICS).HasExtension(sync_pb::bookmark));
    EXPECT_EQ("Pete.htm", child.Get(NON_UNIQUE_NAME));
    VerifyTestBookmarkDataInEntry(&child);
  }

  // Only one entry, since we just overwrite one.
  EXPECT_TRUE(1 == children.size());
  syncer_events_.clear();
}

// We got this repro case when someone was editing bookmarks while sync was
// occuring. The entry had changed out underneath the user.
TEST_F(SyncerTest, CommitsUpdateDoesntAlterEntry) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());
  int64 test_time = 123456;
  int64 entry_metahandle;
  {
    WriteTransaction wtrans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry entry(&wtrans, syncable::CREATE, root_id_, "Pete");
    ASSERT_TRUE(entry.good());
    EXPECT_FALSE(entry.Get(ID).ServerKnows());
    entry.Put(syncable::IS_DIR, true);
    entry.Put(syncable::SPECIFICS, DefaultBookmarkSpecifics());
    entry.Put(syncable::IS_UNSYNCED, true);
    entry.Put(syncable::MTIME, test_time);
    entry_metahandle = entry.Get(META_HANDLE);
  }
  syncer_->SyncShare(this);
  syncable::Id id;
  int64 version;
  int64 server_position_in_parent;
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    Entry entry(&trans, syncable::GET_BY_HANDLE, entry_metahandle);
    ASSERT_TRUE(entry.good());
    id = entry.Get(ID);
    EXPECT_TRUE(id.ServerKnows());
    version = entry.Get(BASE_VERSION);
    server_position_in_parent = entry.Get(SERVER_POSITION_IN_PARENT);
  }
  mock_server_->AddUpdateDirectory(id, root_id_, "Pete", version, 10);
  mock_server_->SetLastUpdatePosition(server_position_in_parent);
  syncer_->SyncShare(this);
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    Entry entry(&trans, syncable::GET_BY_ID, id);
    ASSERT_TRUE(entry.good());
    EXPECT_TRUE(entry.Get(MTIME) == test_time);
  }
}

TEST_F(SyncerTest, ParentAndChildBothMatch) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());
  syncable::Id parent_id = ids_.NewServerId();
  syncable::Id child_id = ids_.NewServerId();

  {
    WriteTransaction wtrans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry parent(&wtrans, CREATE, root_id_, "Folder");
    ASSERT_TRUE(parent.good());
    parent.Put(IS_DIR, true);
    parent.Put(IS_UNSYNCED, true);
    parent.Put(ID, parent_id);
    parent.Put(BASE_VERSION, 1);
    parent.Put(SPECIFICS, DefaultBookmarkSpecifics());

    MutableEntry child(&wtrans, CREATE, parent.Get(ID), "test.htm");
    ASSERT_TRUE(child.good());
    child.Put(ID, child_id);
    child.Put(BASE_VERSION, 1);
    child.Put(SPECIFICS, DefaultBookmarkSpecifics());
    WriteTestDataToEntry(&wtrans, &child);
  }
  mock_server_->AddUpdateDirectory(parent_id, root_id_, "Folder", 10, 10);
  mock_server_->AddUpdateBookmark(child_id, parent_id, "test.htm", 10, 10);
  mock_server_->set_conflict_all_commits(true);
  syncer_->SyncShare(this);
  syncer_->SyncShare(this);
  syncer_->SyncShare(this);
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    Directory::ChildHandles children;
    dir->GetChildHandles(&trans, root_id_, &children);
    EXPECT_TRUE(1 == children.size());
    dir->GetChildHandles(&trans, parent_id, &children);
    EXPECT_TRUE(1 == children.size());
    Directory::UnappliedUpdateMetaHandles unapplied;
    dir->GetUnappliedUpdateMetaHandles(&trans, &unapplied);
    EXPECT_TRUE(0 == unapplied.size());
    syncable::Directory::UnsyncedMetaHandles unsynced;
    dir->GetUnsyncedMetaHandles(&trans, &unsynced);
    EXPECT_TRUE(0 == unsynced.size());
    syncer_events_.clear();
  }
}

TEST_F(SyncerTest, CommittingNewDeleted) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry entry(&trans, CREATE, trans.root_id(), "bob");
    entry.Put(IS_UNSYNCED, true);
    entry.Put(IS_DEL, true);
  }
  syncer_->SyncShare(this);
  EXPECT_TRUE(0 == mock_server_->committed_ids().size());
}

// Original problem synopsis:
// Check failed: entry->Get(BASE_VERSION) <= entry->Get(SERVER_VERSION)
// Client creates entry, client finishes committing entry. Between
// commit and getting update back, we delete the entry.
// We get the update for the entry, but the local one was modified
// so we store the entry but don't apply it. IS_UNAPPLIED_UPDATE is set.
// We commit deletion and get a new version number.
// We apply unapplied updates again before we get the update about the deletion.
// This means we have an unapplied update where server_version < base_version.
TEST_F(SyncerTest, UnappliedUpdateDuringCommit) {
  // This test is a little fake.
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry entry(&trans, CREATE, trans.root_id(), "bob");
    entry.Put(ID, ids_.FromNumber(20));
    entry.Put(BASE_VERSION, 1);
    entry.Put(SERVER_VERSION, 1);
    entry.Put(SERVER_PARENT_ID, ids_.FromNumber(9999));  // Bad parent.
    entry.Put(IS_UNSYNCED, true);
    entry.Put(IS_UNAPPLIED_UPDATE, true);
    entry.Put(SPECIFICS, DefaultBookmarkSpecifics());
    entry.Put(SERVER_SPECIFICS, DefaultBookmarkSpecifics());
    entry.Put(IS_DEL, false);
  }
  syncer_->SyncShare(session_.get());
  syncer_->SyncShare(session_.get());
  const ConflictProgress* progress =
      session_->status_controller()->conflict_progress();
  EXPECT_TRUE(0 == progress->ConflictingItemsSize());
  syncer_events_.clear();
}

// Original problem synopsis:
//   Illegal parent
// Unexpected error during sync if we:
//   make a new folder bob
//   wait for sync
//   make a new folder fred
//   move bob into fred
//   remove bob
//   remove fred
// if no syncing occured midway, bob will have an illegal parent
TEST_F(SyncerTest, DeletingEntryInFolder) {
  // This test is a little fake.
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());

  int64 existing_metahandle;
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry entry(&trans, CREATE, trans.root_id(), "existing");
    ASSERT_TRUE(entry.good());
    entry.Put(IS_DIR, true);
    entry.Put(SPECIFICS, DefaultBookmarkSpecifics());
    entry.Put(IS_UNSYNCED, true);
    existing_metahandle = entry.Get(META_HANDLE);
  }
  syncer_->SyncShare(session_.get());
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry newfolder(&trans, CREATE, trans.root_id(), "new");
    ASSERT_TRUE(newfolder.good());
    newfolder.Put(IS_DIR, true);
    newfolder.Put(SPECIFICS, DefaultBookmarkSpecifics());
    newfolder.Put(IS_UNSYNCED, true);

    MutableEntry existing(&trans, GET_BY_HANDLE, existing_metahandle);
    ASSERT_TRUE(existing.good());
    existing.Put(PARENT_ID, newfolder.Get(ID));
    existing.Put(IS_UNSYNCED, true);
    EXPECT_TRUE(existing.Get(ID).ServerKnows());

    newfolder.Put(IS_DEL, true);
    existing.Put(IS_DEL, true);
  }
  syncer_->SyncShare(session_.get());
  StatusController* status(session_->status_controller());
  EXPECT_TRUE(0 == status->error_counters().num_conflicting_commits);
}

TEST_F(SyncerTest, DeletingEntryWithLocalEdits) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());
  int64 newfolder_metahandle;

  mock_server_->AddUpdateDirectory(1, 0, "bob", 1, 10);
  syncer_->SyncShare(this);
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry newfolder(&trans, CREATE, ids_.FromNumber(1), "local");
    ASSERT_TRUE(newfolder.good());
    newfolder.Put(IS_UNSYNCED, true);
    newfolder.Put(IS_DIR, true);
    newfolder.Put(SPECIFICS, DefaultBookmarkSpecifics());
    newfolder_metahandle = newfolder.Get(META_HANDLE);
  }
  mock_server_->AddUpdateDirectory(1, 0, "bob", 2, 20);
  mock_server_->SetLastUpdateDeleted();
  syncer_->SyncShare(SYNCER_BEGIN, APPLY_UPDATES, this);
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    Entry entry(&trans, syncable::GET_BY_HANDLE, newfolder_metahandle);
    ASSERT_TRUE(entry.good());
  }
}

TEST_F(SyncerTest, FolderSwapUpdate) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());
  mock_server_->AddUpdateDirectory(7801, 0, "bob", 1, 10);
  mock_server_->AddUpdateDirectory(1024, 0, "fred", 1, 10);
  syncer_->SyncShare(this);
  mock_server_->AddUpdateDirectory(1024, 0, "bob", 2, 20);
  mock_server_->AddUpdateDirectory(7801, 0, "fred", 2, 20);
  syncer_->SyncShare(this);
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    Entry id1(&trans, GET_BY_ID, ids_.FromNumber(7801));
    ASSERT_TRUE(id1.good());
    EXPECT_TRUE("fred" == id1.Get(NON_UNIQUE_NAME));
    EXPECT_TRUE(root_id_ == id1.Get(PARENT_ID));
    Entry id2(&trans, GET_BY_ID, ids_.FromNumber(1024));
    ASSERT_TRUE(id2.good());
    EXPECT_TRUE("bob" == id2.Get(NON_UNIQUE_NAME));
    EXPECT_TRUE(root_id_ == id2.Get(PARENT_ID));
  }
  syncer_events_.clear();
}

TEST_F(SyncerTest, NameCollidingFolderSwapWorksFine) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());
  mock_server_->AddUpdateDirectory(7801, 0, "bob", 1, 10);
  mock_server_->AddUpdateDirectory(1024, 0, "fred", 1, 10);
  mock_server_->AddUpdateDirectory(4096, 0, "alice", 1, 10);
  syncer_->SyncShare(this);
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    Entry id1(&trans, GET_BY_ID, ids_.FromNumber(7801));
    ASSERT_TRUE(id1.good());
    EXPECT_TRUE("bob" == id1.Get(NON_UNIQUE_NAME));
    EXPECT_TRUE(root_id_ == id1.Get(PARENT_ID));
    Entry id2(&trans, GET_BY_ID, ids_.FromNumber(1024));
    ASSERT_TRUE(id2.good());
    EXPECT_TRUE("fred" == id2.Get(NON_UNIQUE_NAME));
    EXPECT_TRUE(root_id_ == id2.Get(PARENT_ID));
    Entry id3(&trans, GET_BY_ID, ids_.FromNumber(4096));
    ASSERT_TRUE(id3.good());
    EXPECT_TRUE("alice" == id3.Get(NON_UNIQUE_NAME));
    EXPECT_TRUE(root_id_ == id3.Get(PARENT_ID));
  }
  mock_server_->AddUpdateDirectory(1024, 0, "bob", 2, 20);
  mock_server_->AddUpdateDirectory(7801, 0, "fred", 2, 20);
  mock_server_->AddUpdateDirectory(4096, 0, "bob", 2, 20);
  syncer_->SyncShare(this);
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    Entry id1(&trans, GET_BY_ID, ids_.FromNumber(7801));
    ASSERT_TRUE(id1.good());
    EXPECT_TRUE("fred" == id1.Get(NON_UNIQUE_NAME));
    EXPECT_TRUE(root_id_ == id1.Get(PARENT_ID));
    Entry id2(&trans, GET_BY_ID, ids_.FromNumber(1024));
    ASSERT_TRUE(id2.good());
    EXPECT_TRUE("bob" == id2.Get(NON_UNIQUE_NAME));
    EXPECT_TRUE(root_id_ == id2.Get(PARENT_ID));
    Entry id3(&trans, GET_BY_ID, ids_.FromNumber(4096));
    ASSERT_TRUE(id3.good());
    EXPECT_TRUE("bob" == id3.Get(NON_UNIQUE_NAME));
    EXPECT_TRUE(root_id_ == id3.Get(PARENT_ID));
  }
  syncer_events_.clear();
}

TEST_F(SyncerTest, CommitManyItemsInOneGo) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  uint32 max_batches = 3;
  uint32 items_to_commit = kDefaultMaxCommitBatchSize * max_batches;
  CHECK(dir.good());
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    for (uint32 i = 0; i < items_to_commit; i++) {
      string nameutf8 = StringPrintf("%d", i);
      string name(nameutf8.begin(), nameutf8.end());
      MutableEntry e(&trans, CREATE, trans.root_id(), name);
      e.Put(IS_UNSYNCED, true);
      e.Put(IS_DIR, true);
      e.Put(SPECIFICS, DefaultBookmarkSpecifics());
    }
  }
  uint32 num_loops = 0;
  while (syncer_->SyncShare(this)) {
    num_loops++;
    ASSERT_LT(num_loops, max_batches * 2);
  }
  EXPECT_GE(mock_server_->commit_messages().size(), max_batches);
}

TEST_F(SyncerTest, HugeConflict) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  int item_count = 300;  // We should be able to do 300 or 3000 w/o issue.
  CHECK(dir.good());

  syncable::Id parent_id = ids_.NewServerId();
  syncable::Id last_id = parent_id;
  vector<syncable::Id> tree_ids;

  // Create a lot of updates for which the parent does not exist yet.
  // Generate a huge deep tree which should all fail to apply at first.
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    for (int i = 0; i < item_count ; i++) {
      syncable::Id next_id = ids_.NewServerId();
      tree_ids.push_back(next_id);
      mock_server_->AddUpdateDirectory(next_id, last_id, "BOB", 2, 20);
      last_id = next_id;
    }
  }
  syncer_->SyncShare(this);

  // Check they're in the expected conflict state.
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    for (int i = 0; i < item_count; i++) {
      Entry e(&trans, GET_BY_ID, tree_ids[i]);
      // They should all exist but none should be applied.
      ASSERT_TRUE(e.good());
      EXPECT_TRUE(e.Get(IS_DEL));
      EXPECT_TRUE(e.Get(IS_UNAPPLIED_UPDATE));
    }
  }

  // Add the missing parent directory.
  mock_server_->AddUpdateDirectory(parent_id, TestIdFactory::root(),
      "BOB", 2, 20);
  syncer_->SyncShare(this);

  // Now they should all be OK.
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    for (int i = 0; i < item_count; i++) {
      Entry e(&trans, GET_BY_ID, tree_ids[i]);
      ASSERT_TRUE(e.good());
      EXPECT_FALSE(e.Get(IS_DEL));
      EXPECT_FALSE(e.Get(IS_UNAPPLIED_UPDATE));
    }
  }
}

TEST_F(SyncerTest, DontCrashOnCaseChange) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());
  mock_server_->AddUpdateDirectory(1, 0, "bob", 1, 10);
  syncer_->SyncShare(this);
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry e(&trans, GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(e.good());
    e.Put(IS_UNSYNCED, true);
  }
  mock_server_->set_conflict_all_commits(true);
  mock_server_->AddUpdateDirectory(1, 0, "BOB", 2, 20);
  syncer_->SyncShare(this);  // USED TO CAUSE AN ASSERT
  syncer_events_.clear();
}

TEST_F(SyncerTest, UnsyncedItemAndUpdate) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());
  mock_server_->AddUpdateDirectory(1, 0, "bob", 1, 10);
  syncer_->SyncShare(this);
  mock_server_->set_conflict_all_commits(true);
  mock_server_->AddUpdateDirectory(2, 0, "bob", 2, 20);
  syncer_->SyncShare(this);  // USED TO CAUSE AN ASSERT
  syncer_events_.clear();
}

TEST_F(SyncerTest, NewEntryAndAlteredServerEntrySharePath) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());
  mock_server_->AddUpdateBookmark(1, 0, "Foo.htm", 10, 10);
  syncer_->SyncShare(this);
  int64 local_folder_handle;
  syncable::Id local_folder_id;
  {
    WriteTransaction wtrans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry new_entry(&wtrans, CREATE, wtrans.root_id(), "Bar.htm");
    ASSERT_TRUE(new_entry.good());
    local_folder_id = new_entry.Get(ID);
    local_folder_handle = new_entry.Get(META_HANDLE);
    new_entry.Put(IS_UNSYNCED, true);
    new_entry.Put(SPECIFICS, DefaultBookmarkSpecifics());
    MutableEntry old(&wtrans, GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(old.good());
    WriteTestDataToEntry(&wtrans, &old);
  }
  mock_server_->AddUpdateBookmark(1, 0, "Bar.htm", 20, 20);
  mock_server_->set_conflict_all_commits(true);
  syncer_->SyncShare(this);
  syncer_events_.clear();
  {
    // Update #20 should have been dropped in favor of the local version.
    WriteTransaction wtrans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry server(&wtrans, GET_BY_ID, ids_.FromNumber(1));
    MutableEntry local(&wtrans, GET_BY_HANDLE, local_folder_handle);
    ASSERT_TRUE(server.good());
    ASSERT_TRUE(local.good());
    EXPECT_TRUE(local.Get(META_HANDLE) != server.Get(META_HANDLE));
    EXPECT_FALSE(server.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_FALSE(local.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_TRUE(server.Get(IS_UNSYNCED));
    EXPECT_TRUE(local.Get(IS_UNSYNCED));
    EXPECT_EQ("Foo.htm", server.Get(NON_UNIQUE_NAME));
    EXPECT_EQ("Bar.htm", local.Get(NON_UNIQUE_NAME));
  }
  // Allow local changes to commit.
  mock_server_->set_conflict_all_commits(false);
  syncer_->SyncShare(this);
  syncer_events_.clear();

  // Now add a server change to make the two names equal.  There should
  // be no conflict with that, since names are not unique.
  mock_server_->AddUpdateBookmark(1, 0, "Bar.htm", 30, 30);
  syncer_->SyncShare(this);
  syncer_events_.clear();
  {
    WriteTransaction wtrans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry server(&wtrans, GET_BY_ID, ids_.FromNumber(1));
    MutableEntry local(&wtrans, GET_BY_HANDLE, local_folder_handle);
    ASSERT_TRUE(server.good());
    ASSERT_TRUE(local.good());
    EXPECT_TRUE(local.Get(META_HANDLE) != server.Get(META_HANDLE));
    EXPECT_FALSE(server.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_FALSE(local.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_FALSE(server.Get(IS_UNSYNCED));
    EXPECT_FALSE(local.Get(IS_UNSYNCED));
    EXPECT_EQ("Bar.htm", server.Get(NON_UNIQUE_NAME));
    EXPECT_EQ("Bar.htm", local.Get(NON_UNIQUE_NAME));
    EXPECT_EQ("http://google.com",  // Default from AddUpdateBookmark.
        server.Get(SPECIFICS).GetExtension(sync_pb::bookmark).url());
  }
}

// Same as NewEntryAnddServerEntrySharePath, but using the old-style protocol.
TEST_F(SyncerTest, NewEntryAndAlteredServerEntrySharePath_OldBookmarksProto) {
  mock_server_->set_use_legacy_bookmarks_protocol(true);
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());
  mock_server_->AddUpdateBookmark(1, 0, "Foo.htm", 10, 10);
  syncer_->SyncShare(this);
  int64 local_folder_handle;
  syncable::Id local_folder_id;
  {
    WriteTransaction wtrans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry new_entry(&wtrans, CREATE, wtrans.root_id(), "Bar.htm");
    ASSERT_TRUE(new_entry.good());
    local_folder_id = new_entry.Get(ID);
    local_folder_handle = new_entry.Get(META_HANDLE);
    new_entry.Put(IS_UNSYNCED, true);
    new_entry.Put(SPECIFICS, DefaultBookmarkSpecifics());
    MutableEntry old(&wtrans, GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(old.good());
    WriteTestDataToEntry(&wtrans, &old);
  }
  mock_server_->AddUpdateBookmark(1, 0, "Bar.htm", 20, 20);
  mock_server_->set_conflict_all_commits(true);
  syncer_->SyncShare(this);
  syncer_events_.clear();
  {
    // Update #20 should have been dropped in favor of the local version.
    WriteTransaction wtrans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry server(&wtrans, GET_BY_ID, ids_.FromNumber(1));
    MutableEntry local(&wtrans, GET_BY_HANDLE, local_folder_handle);
    ASSERT_TRUE(server.good());
    ASSERT_TRUE(local.good());
    EXPECT_TRUE(local.Get(META_HANDLE) != server.Get(META_HANDLE));
    EXPECT_FALSE(server.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_FALSE(local.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_TRUE(server.Get(IS_UNSYNCED));
    EXPECT_TRUE(local.Get(IS_UNSYNCED));
    EXPECT_EQ("Foo.htm", server.Get(NON_UNIQUE_NAME));
    EXPECT_EQ("Bar.htm", local.Get(NON_UNIQUE_NAME));
  }
  // Allow local changes to commit.
  mock_server_->set_conflict_all_commits(false);
  syncer_->SyncShare(this);
  syncer_events_.clear();

  // Now add a server change to make the two names equal.  There should
  // be no conflict with that, since names are not unique.
  mock_server_->AddUpdateBookmark(1, 0, "Bar.htm", 30, 30);
  syncer_->SyncShare(this);
  syncer_events_.clear();
  {
    WriteTransaction wtrans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry server(&wtrans, GET_BY_ID, ids_.FromNumber(1));
    MutableEntry local(&wtrans, GET_BY_HANDLE, local_folder_handle);
    ASSERT_TRUE(server.good());
    ASSERT_TRUE(local.good());
    EXPECT_TRUE(local.Get(META_HANDLE) != server.Get(META_HANDLE));
    EXPECT_FALSE(server.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_FALSE(local.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_FALSE(server.Get(IS_UNSYNCED));
    EXPECT_FALSE(local.Get(IS_UNSYNCED));
    EXPECT_EQ("Bar.htm", server.Get(NON_UNIQUE_NAME));
    EXPECT_EQ("Bar.htm", local.Get(NON_UNIQUE_NAME));
    EXPECT_EQ("http://google.com",  // Default from AddUpdateBookmark.
        server.Get(SPECIFICS).GetExtension(sync_pb::bookmark).url());
  }
}


// Circular links should be resolved by the server.
TEST_F(SyncerTest, SiblingDirectoriesBecomeCircular) {
  // we don't currently resolve this. This test ensures we don't.
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());
  mock_server_->AddUpdateDirectory(1, 0, "A", 10, 10);
  mock_server_->AddUpdateDirectory(2, 0, "B", 10, 10);
  syncer_->SyncShare(this);
  {
    WriteTransaction wtrans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry A(&wtrans, GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(A.good());
    A.Put(IS_UNSYNCED, true);
    ASSERT_TRUE(A.Put(PARENT_ID, ids_.FromNumber(2)));
    ASSERT_TRUE(A.Put(NON_UNIQUE_NAME, "B"));
  }
  mock_server_->AddUpdateDirectory(2, 1, "A", 20, 20);
  mock_server_->set_conflict_all_commits(true);
  syncer_->SyncShare(this);
  syncer_events_.clear();
  {
    WriteTransaction wtrans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry A(&wtrans, GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(A.good());
    MutableEntry B(&wtrans, GET_BY_ID, ids_.FromNumber(2));
    ASSERT_TRUE(B.good());
    EXPECT_TRUE(A.Get(NON_UNIQUE_NAME) == "B");
    EXPECT_TRUE(B.Get(NON_UNIQUE_NAME) == "B");
  }
}

TEST_F(SyncerTest, ConflictSetClassificationError) {
  // This code used to cause a CHECK failure because we incorrectly thought
  // a set was only unapplied updates.
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());
  mock_server_->AddUpdateDirectory(1, 0, "A", 10, 10);
  mock_server_->AddUpdateDirectory(2, 0, "B", 10, 10);
  mock_server_->set_conflict_all_commits(true);
  syncer_->SyncShare(this);
  {
    WriteTransaction wtrans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry A(&wtrans, GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(A.good());
    A.Put(IS_UNSYNCED, true);
    A.Put(IS_UNAPPLIED_UPDATE, true);
    A.Put(SERVER_NON_UNIQUE_NAME, "B");
    MutableEntry B(&wtrans, GET_BY_ID, ids_.FromNumber(2));
    ASSERT_TRUE(B.good());
    B.Put(IS_UNAPPLIED_UPDATE, true);
    B.Put(SERVER_NON_UNIQUE_NAME, "A");
  }
  syncer_->SyncShare(this);
  syncer_events_.clear();
}

TEST_F(SyncerTest, SwapEntryNames) {
  // Simple transaction test.
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());
  mock_server_->AddUpdateDirectory(1, 0, "A", 10, 10);
  mock_server_->AddUpdateDirectory(2, 0, "B", 10, 10);
  mock_server_->set_conflict_all_commits(true);
  syncer_->SyncShare(this);
  {
    WriteTransaction wtrans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry A(&wtrans, GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(A.good());
    A.Put(IS_UNSYNCED, true);
    MutableEntry B(&wtrans, GET_BY_ID, ids_.FromNumber(2));
    ASSERT_TRUE(B.good());
    B.Put(IS_UNSYNCED, true);
    ASSERT_TRUE(A.Put(NON_UNIQUE_NAME, "C"));
    ASSERT_TRUE(B.Put(NON_UNIQUE_NAME, "A"));
    ASSERT_TRUE(A.Put(NON_UNIQUE_NAME, "B"));
  }
  syncer_->SyncShare(this);
  syncer_events_.clear();
}

TEST_F(SyncerTest, DualDeletionWithNewItemNameClash) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());
  mock_server_->AddUpdateDirectory(1, 0, "A", 10, 10);
  mock_server_->AddUpdateBookmark(2, 0, "B", 10, 10);
  mock_server_->set_conflict_all_commits(true);
  syncer_->SyncShare(this);
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry B(&trans, GET_BY_ID, ids_.FromNumber(2));
    ASSERT_TRUE(B.good());
    WriteTestDataToEntry(&trans, &B);
    B.Put(IS_DEL, true);
  }
  mock_server_->AddUpdateBookmark(2, 0, "A", 11, 11);
  mock_server_->SetLastUpdateDeleted();
  syncer_->SyncShare(this);
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    Entry B(&trans, GET_BY_ID, ids_.FromNumber(2));
    ASSERT_TRUE(B.good());
    EXPECT_FALSE(B.Get(IS_UNSYNCED));
    EXPECT_FALSE(B.Get(IS_UNAPPLIED_UPDATE));
  }
  syncer_events_.clear();
}

TEST_F(SyncerTest, FixDirectoryLoopConflict) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());
  mock_server_->AddUpdateDirectory(1, 0, "bob", 1, 10);
  mock_server_->AddUpdateDirectory(2, 0, "fred", 1, 10);
  syncer_->SyncShare(this);
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry bob(&trans, GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(bob.good());
    bob.Put(IS_UNSYNCED, true);
    bob.Put(PARENT_ID, ids_.FromNumber(2));
  }
  mock_server_->AddUpdateDirectory(2, 1, "fred", 2, 20);
  mock_server_->set_conflict_all_commits(true);
  syncer_->SyncShare(this);
  syncer_->SyncShare(this);
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    Entry bob(&trans, GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(bob.good());
    Entry fred(&trans, GET_BY_ID, ids_.FromNumber(2));
    ASSERT_TRUE(fred.good());
    EXPECT_TRUE(fred.Get(IS_UNSYNCED));
    EXPECT_TRUE(bob.Get(IS_UNSYNCED));
    EXPECT_FALSE(fred.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_FALSE(bob.Get(IS_UNAPPLIED_UPDATE));
  }
  syncer_events_.clear();
}

TEST_F(SyncerTest, ResolveWeWroteTheyDeleted) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());

  int64 bob_metahandle;

  mock_server_->AddUpdateBookmark(1, 0, "bob", 1, 10);
  syncer_->SyncShare(this);
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry bob(&trans, GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(bob.good());
    bob_metahandle = bob.Get(META_HANDLE);
    WriteTestDataToEntry(&trans, &bob);
  }
  mock_server_->AddUpdateBookmark(1, 0, "bob", 2, 10);
  mock_server_->SetLastUpdateDeleted();
  mock_server_->set_conflict_all_commits(true);
  syncer_->SyncShare(this);
  syncer_->SyncShare(this);
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    Entry bob(&trans, GET_BY_HANDLE, bob_metahandle);
    ASSERT_TRUE(bob.good());
    EXPECT_TRUE(bob.Get(IS_UNSYNCED));
    EXPECT_FALSE(bob.Get(ID).ServerKnows());
    EXPECT_FALSE(bob.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_FALSE(bob.Get(IS_DEL));
  }
  syncer_events_.clear();
}

TEST_F(SyncerTest, ServerDeletingFolderWeHaveMovedSomethingInto) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());

  syncable::Id bob_id = ids_.NewServerId();
  syncable::Id fred_id = ids_.NewServerId();

  mock_server_->AddUpdateDirectory(bob_id, TestIdFactory::root(),
      "bob", 1, 10);
  mock_server_->AddUpdateDirectory(fred_id, TestIdFactory::root(),
      "fred", 1, 10);
  syncer_->SyncShare(this);
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry bob(&trans, GET_BY_ID, bob_id);
    ASSERT_TRUE(bob.good());
    bob.Put(IS_UNSYNCED, true);
    bob.Put(PARENT_ID, fred_id);
  }
  mock_server_->AddUpdateDirectory(fred_id, TestIdFactory::root(),
      "fred", 2, 20);
  mock_server_->SetLastUpdateDeleted();
  mock_server_->set_conflict_all_commits(true);
  syncer_->SyncShare(this);
  syncer_->SyncShare(this);
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);

    Entry bob(&trans, GET_BY_ID, bob_id);
    ASSERT_TRUE(bob.good());
    EXPECT_TRUE(bob.Get(IS_UNSYNCED));
    EXPECT_FALSE(bob.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_TRUE(bob.Get(NON_UNIQUE_NAME) == "bob");
    EXPECT_NE(bob.Get(PARENT_ID), fred_id);

    // Entry was deleted and reborn.
    Entry dead_fred(&trans, GET_BY_ID, fred_id);
    EXPECT_FALSE(dead_fred.good());

    // Reborn fred
    Entry fred(&trans, GET_BY_ID, bob.Get(PARENT_ID));
    ASSERT_TRUE(fred.good());
    EXPECT_TRUE(fred.Get(PARENT_ID) == trans.root_id());
    EXPECT_EQ("fred", fred.Get(NON_UNIQUE_NAME));
    EXPECT_TRUE(fred.Get(IS_UNSYNCED));
    EXPECT_FALSE(fred.Get(IS_UNAPPLIED_UPDATE));
  }
  syncer_events_.clear();
}

// TODO(ncarter): This test is bogus, but it actually seems to hit an
// interesting case the 4th time SyncShare is called.
// TODO(chron): The fourth time that SyncShare is called it crashes.
// This seems to be due to a bug in the conflict set building logic.
TEST_F(SyncerTest, DISABLED_ServerDeletingFolderWeHaveAnOpenEntryIn) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());
  mock_server_->AddUpdateBookmark(1, 0, "bob", 1, 10);
  mock_server_->AddUpdateDirectory(2, 0, "fred", 1, 10);
  syncer_->SyncShare(this);
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry bob(&trans, GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(bob.good());
    bob.Put(IS_UNSYNCED, true);
    WriteTestDataToEntry(&trans, &bob);
  }
  syncer_->SyncShare(this);
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry bob(&trans, GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(bob.good());
    EXPECT_FALSE(bob.Get(IS_UNSYNCED));
    bob.Put(IS_UNSYNCED, true);
    bob.Put(PARENT_ID, ids_.FromNumber(2));
  }
  mock_server_->AddUpdateDirectory(2, 0, "fred", 2, 20);
  mock_server_->SetLastUpdateDeleted();
  mock_server_->set_conflict_all_commits(true);
  syncer_events_.clear();
  // These SyncShares would cause a CHECK because we'd think we were stuck.
  syncer_->SyncShare(this);
  syncer_->SyncShare(this);
  syncer_->SyncShare(this);
  syncer_->SyncShare(this);
  syncer_->SyncShare(this);
  syncer_->SyncShare(this);
  syncer_->SyncShare(this);
  syncer_->SyncShare(this);
  EXPECT_TRUE(0 == syncer_events_.size());
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    Entry bob(&trans, GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(bob.good());
    Id fred_id =
        GetOnlyEntryWithName(&trans, TestIdFactory::root(), "fred");
    Entry fred(&trans, GET_BY_ID, fred_id);
    ASSERT_TRUE(fred.good());
    EXPECT_FALSE(fred.Get(IS_UNSYNCED));
    EXPECT_TRUE(fred.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_TRUE(bob.Get(PARENT_ID) == fred.Get(ID));
    EXPECT_FALSE(bob.Get(IS_UNAPPLIED_UPDATE));
  }
  syncer_events_.clear();
}


TEST_F(SyncerTest, WeMovedSomethingIntoAFolderServerHasDeleted) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());

  syncable::Id bob_id = ids_.NewServerId();
  syncable::Id fred_id = ids_.NewServerId();

  mock_server_->AddUpdateDirectory(bob_id, TestIdFactory::root(),
      "bob", 1, 10);
  mock_server_->AddUpdateDirectory(fred_id, TestIdFactory::root(),
      "fred", 1, 10);
  syncer_->SyncShare(this);
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    Entry fred(&trans, GET_BY_ID, fred_id);
    ASSERT_TRUE(fred.good());

    MutableEntry bob(&trans, GET_BY_ID, bob_id);
    ASSERT_TRUE(bob.good());
    bob.Put(IS_UNSYNCED, true);
    bob.Put(PARENT_ID, fred_id);
  }
  mock_server_->AddUpdateDirectory(fred_id, TestIdFactory::root(),
      "fred", 2, 20);
  mock_server_->SetLastUpdateDeleted();
  mock_server_->set_conflict_all_commits(true);
  syncer_->SyncShare(this);
  syncer_->SyncShare(this);
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    Entry bob(&trans, GET_BY_ID, bob_id);
    ASSERT_TRUE(bob.good());

    // Entry was deleted by server. We'll make a new one though with a new ID.
    Entry dead_fred(&trans, GET_BY_ID, fred_id);
    EXPECT_FALSE(dead_fred.good());

    // Fred is reborn with a local ID.
    Entry fred(&trans, GET_BY_ID, bob.Get(PARENT_ID));
    EXPECT_EQ("fred", fred.Get(NON_UNIQUE_NAME));
    EXPECT_EQ(TestIdFactory::root(), fred.Get(PARENT_ID));
    EXPECT_TRUE(fred.Get(IS_UNSYNCED));
    EXPECT_FALSE(fred.Get(ID).ServerKnows());

    // Bob needs to update his parent.
    EXPECT_TRUE(bob.Get(IS_UNSYNCED));
    EXPECT_TRUE(bob.Get(PARENT_ID) == fred.Get(ID));
    EXPECT_TRUE(fred.Get(PARENT_ID) == root_id_);
    EXPECT_FALSE(fred.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_FALSE(bob.Get(IS_UNAPPLIED_UPDATE));
  }
  syncer_events_.clear();
}

class FolderMoveDeleteRenameTest : public SyncerTest {
 public:
  FolderMoveDeleteRenameTest() : move_bob_count_(0), done_(false) {}

  static const int64 bob_id_number = 1;
  static const int64 fred_id_number = 2;

  void MoveBobIntoID2Runner() {
    if (!done_) {
      done_ = MoveBobIntoID2();
    }
  }

 protected:
  int move_bob_count_;
  bool done_;

  bool MoveBobIntoID2() {
    ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
    CHECK(dir.good());

    if (--move_bob_count_ > 0) {
    return false;
    }

    if (move_bob_count_ == 0) {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
      Entry alice(&trans, GET_BY_ID,
                  TestIdFactory::FromNumber(fred_id_number));
    CHECK(alice.good());
    CHECK(!alice.Get(IS_DEL));
      MutableEntry bob(&trans, GET_BY_ID,
                       TestIdFactory::FromNumber(bob_id_number));
    CHECK(bob.good());
    bob.Put(IS_UNSYNCED, true);
    bob.Put(PARENT_ID, alice.Get(ID));
    return true;
  }
  return false;
}
};

TEST_F(FolderMoveDeleteRenameTest,
       WeMovedSomethingIntoAFolderServerHasDeletedAndWeRenamed) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());

  const syncable::Id bob_id = TestIdFactory::FromNumber(
      FolderMoveDeleteRenameTest::bob_id_number);
  const syncable::Id fred_id = TestIdFactory::FromNumber(
      FolderMoveDeleteRenameTest::fred_id_number);

  mock_server_->AddUpdateDirectory(bob_id, TestIdFactory::root(),
      "bob", 1, 10);
  mock_server_->AddUpdateDirectory(fred_id, TestIdFactory::root(),
      "fred", 1, 10);
  syncer_->SyncShare(this);
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry fred(&trans, GET_BY_ID, fred_id);
    ASSERT_TRUE(fred.good());
    fred.Put(IS_UNSYNCED, true);
    fred.Put(NON_UNIQUE_NAME, "Alice");
  }
  mock_server_->AddUpdateDirectory(fred_id, TestIdFactory::root(),
      "fred", 2, 20);
  mock_server_->SetLastUpdateDeleted();
  mock_server_->set_conflict_all_commits(true);
  // This test is a little brittle. We want to move the item into the folder
  // such that we think we're dealing with a simple conflict, but in reality
  // it's actually a conflict set.
  move_bob_count_ = 2;
  mock_server_->SetMidCommitCallback(
      NewCallback<FolderMoveDeleteRenameTest>(this,
          &FolderMoveDeleteRenameTest::MoveBobIntoID2Runner));
  syncer_->SyncShare(this);
  syncer_->SyncShare(this);
  syncer_->SyncShare(this);
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    Entry bob(&trans, GET_BY_ID, bob_id);
    ASSERT_TRUE(bob.good());

    // Old entry is dead
    Entry dead_fred(&trans, GET_BY_ID, fred_id);
    EXPECT_FALSE(dead_fred.good());

    // New ID is created to fill parent folder, named correctly
    Entry alice(&trans, GET_BY_ID, bob.Get(PARENT_ID));
    ASSERT_TRUE(alice.good());
    EXPECT_EQ("Alice", alice.Get(NON_UNIQUE_NAME));
    EXPECT_TRUE(alice.Get(IS_UNSYNCED));
    EXPECT_FALSE(alice.Get(ID).ServerKnows());
    EXPECT_TRUE(bob.Get(IS_UNSYNCED));
    EXPECT_TRUE(bob.Get(PARENT_ID) == alice.Get(ID));
    EXPECT_TRUE(alice.Get(PARENT_ID) == root_id_);
    EXPECT_FALSE(alice.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_FALSE(bob.Get(IS_UNAPPLIED_UPDATE));
  }
  syncer_events_.clear();
}


TEST_F(SyncerTest,
       WeMovedADirIntoAndCreatedAnEntryInAFolderServerHasDeleted) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());

  syncable::Id bob_id = ids_.NewServerId();
  syncable::Id fred_id = ids_.NewServerId();

  mock_server_->AddUpdateDirectory(bob_id, TestIdFactory::root(),
      "bob", 1, 10);
  mock_server_->AddUpdateDirectory(fred_id, TestIdFactory::root(),
      "fred", 1, 10);
  syncer_->SyncShare(this);
  syncable::Id new_item_id;
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry bob(&trans, GET_BY_ID, bob_id);
    ASSERT_TRUE(bob.good());
    bob.Put(IS_UNSYNCED, true);
    bob.Put(PARENT_ID, fred_id);
    MutableEntry new_item(&trans, CREATE, fred_id, "new_item");
    WriteTestDataToEntry(&trans, &new_item);
    new_item_id = new_item.Get(ID);
  }
  mock_server_->AddUpdateDirectory(fred_id, TestIdFactory::root(),
      "fred", 2, 20);
  mock_server_->SetLastUpdateDeleted();
  mock_server_->set_conflict_all_commits(true);
  syncer_->SyncShare(this);
  syncer_->SyncShare(this);
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);

    Entry bob(&trans, GET_BY_ID, bob_id);
    ASSERT_TRUE(bob.good());
    EXPECT_TRUE(bob.Get(IS_UNSYNCED));
    EXPECT_FALSE(bob.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_NE(bob.Get(PARENT_ID), fred_id);

    // Was recreated. Old one shouldn't exist.
    Entry dead_fred(&trans, GET_BY_ID, fred_id);
    EXPECT_FALSE(dead_fred.good());

    Entry fred(&trans, GET_BY_ID, bob.Get(PARENT_ID));
    ASSERT_TRUE(fred.good());
    EXPECT_TRUE(fred.Get(IS_UNSYNCED));
    EXPECT_FALSE(fred.Get(ID).ServerKnows());
    EXPECT_EQ("fred", fred.Get(NON_UNIQUE_NAME));
    EXPECT_FALSE(fred.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_TRUE(fred.Get(PARENT_ID) == root_id_);

    Entry new_item(&trans, GET_BY_ID, new_item_id);
    ASSERT_TRUE(new_item.good());
    EXPECT_EQ(new_item.Get(PARENT_ID), fred.Get(ID));
  }
  syncer_events_.clear();
}

TEST_F(SyncerTest, ServerMovedSomethingIntoAFolderWeHaveDeleted) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());
  mock_server_->AddUpdateDirectory(1, 0, "bob", 1, 10);
  mock_server_->AddUpdateDirectory(2, 0, "fred", 1, 10);
  LoopSyncShare(syncer_);
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry bob(&trans, GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(bob.good());
    bob.Put(IS_UNSYNCED, true);
    bob.Put(IS_DEL, true);
  }
  mock_server_->AddUpdateDirectory(2, 1, "fred", 2, 20);
  mock_server_->set_conflict_all_commits(true);
  LoopSyncShare(syncer_);
  LoopSyncShare(syncer_);
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    Entry bob(&trans, GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(bob.good());
    Entry fred(&trans, GET_BY_ID, ids_.FromNumber(2));
    ASSERT_TRUE(fred.good());
    EXPECT_FALSE(fred.Get(IS_UNSYNCED));
    EXPECT_TRUE(bob.Get(IS_UNSYNCED));
    EXPECT_TRUE(fred.Get(PARENT_ID) == bob.Get(ID));
    EXPECT_TRUE(bob.Get(PARENT_ID) == root_id_);
    EXPECT_FALSE(fred.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_FALSE(bob.Get(IS_UNAPPLIED_UPDATE));
  }
  syncer_events_.clear();
}

TEST_F(SyncerTest, ServerMovedAFolderIntoAFolderWeHaveDeletedAndMovedIntoIt) {
  // This test combines circular folders and deleted parents.
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());
  mock_server_->AddUpdateDirectory(1, 0, "bob", 1, 10);
  mock_server_->AddUpdateDirectory(2, 0, "fred", 1, 10);
  syncer_->SyncShare(this);
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry bob(&trans, GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(bob.good());
    bob.Put(IS_UNSYNCED, true);
    bob.Put(IS_DEL, true);
    bob.Put(PARENT_ID, ids_.FromNumber(2));
  }
  mock_server_->AddUpdateDirectory(2, 1, "fred", 2, 20);
  mock_server_->set_conflict_all_commits(true);
  syncer_->SyncShare(this);
  syncer_->SyncShare(this);
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    Entry bob(&trans, GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(bob.good());
    Entry fred(&trans, GET_BY_ID, ids_.FromNumber(2));
    ASSERT_TRUE(fred.good());
    EXPECT_TRUE(fred.Get(IS_UNSYNCED));
    EXPECT_TRUE(bob.Get(IS_UNSYNCED));
    EXPECT_TRUE(bob.Get(IS_DEL));
    EXPECT_TRUE(fred.Get(PARENT_ID) == root_id_);
    EXPECT_TRUE(bob.Get(PARENT_ID) == fred.Get(ID));
    EXPECT_FALSE(fred.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_FALSE(bob.Get(IS_UNAPPLIED_UPDATE));
  }
  syncer_events_.clear();
}

TEST_F(SyncerTest, NewServerItemInAFolderWeHaveDeleted) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());
  mock_server_->AddUpdateDirectory(1, 0, "bob", 1, 10);
  LoopSyncShare(syncer_);
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry bob(&trans, GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(bob.good());
    bob.Put(IS_UNSYNCED, true);
    bob.Put(IS_DEL, true);
  }
  mock_server_->AddUpdateDirectory(2, 1, "fred", 2, 20);
  mock_server_->set_conflict_all_commits(true);
  LoopSyncShare(syncer_);
  LoopSyncShare(syncer_);
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    Entry bob(&trans, GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(bob.good());
    Entry fred(&trans, GET_BY_ID, ids_.FromNumber(2));
    ASSERT_TRUE(fred.good());
    EXPECT_FALSE(fred.Get(IS_UNSYNCED));
    EXPECT_TRUE(bob.Get(IS_UNSYNCED));
    EXPECT_TRUE(fred.Get(PARENT_ID) == bob.Get(ID));
    EXPECT_TRUE(bob.Get(PARENT_ID) == root_id_);
    EXPECT_FALSE(fred.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_FALSE(bob.Get(IS_UNAPPLIED_UPDATE));
  }
  syncer_events_.clear();
}

TEST_F(SyncerTest, NewServerItemInAFolderHierarchyWeHaveDeleted) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());
  mock_server_->AddUpdateDirectory(1, 0, "bob", 1, 10);
  mock_server_->AddUpdateDirectory(2, 1, "joe", 1, 10);
  LoopSyncShare(syncer_);
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry bob(&trans, GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(bob.good());
    bob.Put(IS_UNSYNCED, true);
    bob.Put(IS_DEL, true);
    MutableEntry joe(&trans, GET_BY_ID, ids_.FromNumber(2));
    ASSERT_TRUE(joe.good());
    joe.Put(IS_UNSYNCED, true);
    joe.Put(IS_DEL, true);
  }
  mock_server_->AddUpdateDirectory(3, 2, "fred", 2, 20);
  mock_server_->set_conflict_all_commits(true);
  LoopSyncShare(syncer_);
  LoopSyncShare(syncer_);
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    Entry bob(&trans, GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(bob.good());
    Entry joe(&trans, GET_BY_ID, ids_.FromNumber(2));
    ASSERT_TRUE(joe.good());
    Entry fred(&trans, GET_BY_ID, ids_.FromNumber(3));
    ASSERT_TRUE(fred.good());
    EXPECT_FALSE(fred.Get(IS_UNSYNCED));
    EXPECT_TRUE(bob.Get(IS_UNSYNCED));
    EXPECT_TRUE(joe.Get(IS_UNSYNCED));
    EXPECT_TRUE(fred.Get(PARENT_ID) == joe.Get(ID));
    EXPECT_TRUE(joe.Get(PARENT_ID) == bob.Get(ID));
    EXPECT_TRUE(bob.Get(PARENT_ID) == root_id_);
    EXPECT_FALSE(fred.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_FALSE(bob.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_FALSE(joe.Get(IS_UNAPPLIED_UPDATE));
  }
  syncer_events_.clear();
}

TEST_F(SyncerTest, NewServerItemInAFolderHierarchyWeHaveDeleted2) {
  // The difference here is that the hierarchy's not in the root. We have
  // another entry that shouldn't be touched.
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());
  mock_server_->AddUpdateDirectory(4, 0, "susan", 1, 10);
  mock_server_->AddUpdateDirectory(1, 4, "bob", 1, 10);
  mock_server_->AddUpdateDirectory(2, 1, "joe", 1, 10);
  LoopSyncShare(syncer_);
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry bob(&trans, GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(bob.good());
    bob.Put(IS_UNSYNCED, true);
    bob.Put(IS_DEL, true);
    MutableEntry joe(&trans, GET_BY_ID, ids_.FromNumber(2));
    ASSERT_TRUE(joe.good());
    joe.Put(IS_UNSYNCED, true);
    joe.Put(IS_DEL, true);
  }
  mock_server_->AddUpdateDirectory(3, 2, "fred", 2, 20);
  mock_server_->set_conflict_all_commits(true);
  LoopSyncShare(syncer_);
  LoopSyncShare(syncer_);
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    Entry bob(&trans, GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(bob.good());
    Entry joe(&trans, GET_BY_ID, ids_.FromNumber(2));
    ASSERT_TRUE(joe.good());
    Entry fred(&trans, GET_BY_ID, ids_.FromNumber(3));
    ASSERT_TRUE(fred.good());
    Entry susan(&trans, GET_BY_ID, ids_.FromNumber(4));
    ASSERT_TRUE(susan.good());
    EXPECT_FALSE(susan.Get(IS_UNSYNCED));
    EXPECT_FALSE(fred.Get(IS_UNSYNCED));
    EXPECT_TRUE(bob.Get(IS_UNSYNCED));
    EXPECT_TRUE(joe.Get(IS_UNSYNCED));
    EXPECT_EQ(fred.Get(PARENT_ID), joe.Get(ID));
    EXPECT_EQ(joe.Get(PARENT_ID), bob.Get(ID));
    EXPECT_EQ(bob.Get(PARENT_ID), susan.Get(ID));
    EXPECT_EQ(susan.Get(PARENT_ID), root_id_);
    EXPECT_FALSE(susan.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_FALSE(fred.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_FALSE(bob.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_FALSE(joe.Get(IS_UNAPPLIED_UPDATE));
  }
  syncer_events_.clear();
}


class SusanDeletingTest : public SyncerTest {
 public:
  SusanDeletingTest() : countdown_till_delete_(0) {}

  static const int64 susan_int_id_ = 4;

  void DeleteSusanInRoot() {
    ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
    ASSERT_TRUE(dir.good());

    const syncable::Id susan_id = TestIdFactory::FromNumber(susan_int_id_);
    ASSERT_GT(countdown_till_delete_, 0);
    if (0 != --countdown_till_delete_)
    return;
  WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry susan(&trans, GET_BY_ID, susan_id);
  Directory::ChildHandles children;
  dir->GetChildHandles(&trans, susan.Get(ID), &children);
  ASSERT_TRUE(0 == children.size());
  susan.Put(IS_DEL, true);
  susan.Put(IS_UNSYNCED, true);
}

 protected:
  int countdown_till_delete_;
};

TEST_F(SusanDeletingTest,
       NewServerItemInAFolderHierarchyWeHaveDeleted3) {
  // Same as 2, except we deleted the folder the set is in between set building
  // and conflict resolution.
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());

  const syncable::Id bob_id = TestIdFactory::FromNumber(1);
  const syncable::Id joe_id = TestIdFactory::FromNumber(2);
  const syncable::Id fred_id = TestIdFactory::FromNumber(3);
  const syncable::Id susan_id = TestIdFactory::FromNumber(susan_int_id_);

  mock_server_->AddUpdateDirectory(susan_id, TestIdFactory::root(),
      "susan", 1, 10);
  mock_server_->AddUpdateDirectory(bob_id, susan_id, "bob", 1, 10);
  mock_server_->AddUpdateDirectory(joe_id, bob_id, "joe", 1, 10);
  LoopSyncShare(syncer_);
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry bob(&trans, GET_BY_ID, bob_id);
    ASSERT_TRUE(bob.good());
    bob.Put(IS_UNSYNCED, true);
    bob.Put(IS_DEL, true);

    MutableEntry joe(&trans, GET_BY_ID, joe_id);
    ASSERT_TRUE(joe.good());
    joe.Put(IS_UNSYNCED, true);
    joe.Put(IS_DEL, true);
  }
  mock_server_->AddUpdateDirectory(fred_id, joe_id, "fred", 2, 20);
  mock_server_->set_conflict_all_commits(true);
  countdown_till_delete_ = 2;
  syncer_->pre_conflict_resolution_closure_ =
      NewCallback<SusanDeletingTest>(this,
          &SusanDeletingTest::DeleteSusanInRoot);
  syncer_->SyncShare(this);
  syncer_->SyncShare(this);
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    Entry bob(&trans, GET_BY_ID, bob_id);
    ASSERT_TRUE(bob.good());
    Entry joe(&trans, GET_BY_ID, joe_id);
    ASSERT_TRUE(joe.good());
    Entry fred(&trans, GET_BY_ID, fred_id);
    ASSERT_TRUE(fred.good());
    Entry susan(&trans, GET_BY_ID, susan_id);
    ASSERT_TRUE(susan.good());
    EXPECT_FALSE(susan.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_TRUE(fred.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_FALSE(bob.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_FALSE(joe.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_TRUE(susan.Get(IS_UNSYNCED));
    EXPECT_FALSE(fred.Get(IS_UNSYNCED));
    EXPECT_TRUE(bob.Get(IS_UNSYNCED));
    EXPECT_TRUE(joe.Get(IS_UNSYNCED));
  }
  EXPECT_TRUE(0 == countdown_till_delete_);
  syncer_->pre_conflict_resolution_closure_ = NULL;
  LoopSyncShare(syncer_);
  LoopSyncShare(syncer_);
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    Entry bob(&trans, GET_BY_ID, bob_id);
    ASSERT_TRUE(bob.good());
    Entry joe(&trans, GET_BY_ID, joe_id);
    ASSERT_TRUE(joe.good());
    Entry fred(&trans, GET_BY_ID, fred_id);
    ASSERT_TRUE(fred.good());
    Entry susan(&trans, GET_BY_ID, susan_id);
    ASSERT_TRUE(susan.good());
    EXPECT_TRUE(susan.Get(IS_UNSYNCED));
    EXPECT_FALSE(fred.Get(IS_UNSYNCED));
    EXPECT_TRUE(bob.Get(IS_UNSYNCED));
    EXPECT_TRUE(joe.Get(IS_UNSYNCED));
    EXPECT_TRUE(fred.Get(PARENT_ID) == joe.Get(ID));
    EXPECT_TRUE(joe.Get(PARENT_ID) == bob.Get(ID));
    EXPECT_TRUE(bob.Get(PARENT_ID) == susan.Get(ID));
    EXPECT_TRUE(susan.Get(PARENT_ID) == root_id_);
    EXPECT_FALSE(susan.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_FALSE(fred.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_FALSE(bob.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_FALSE(joe.Get(IS_UNAPPLIED_UPDATE));
  }
  syncer_events_.clear();
}

TEST_F(SyncerTest, WeMovedSomethingIntoAFolderHierarchyServerHasDeleted) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());

  const syncable::Id bob_id = ids_.NewServerId();
  const syncable::Id fred_id = ids_.NewServerId();
  const syncable::Id alice_id = ids_.NewServerId();

  mock_server_->AddUpdateDirectory(bob_id, TestIdFactory::root(),
      "bob", 1, 10);
  mock_server_->AddUpdateDirectory(fred_id, TestIdFactory::root(),
      "fred", 1, 10);
  mock_server_->AddUpdateDirectory(alice_id, fred_id, "alice", 1, 10);
  syncer_->SyncShare(this);
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry bob(&trans, GET_BY_ID, bob_id);
    ASSERT_TRUE(bob.good());
    bob.Put(IS_UNSYNCED, true);
    bob.Put(PARENT_ID, alice_id);  // Move into alice.
  }
  mock_server_->AddUpdateDirectory(fred_id, TestIdFactory::root(),
      "fred", 2, 20);
  mock_server_->SetLastUpdateDeleted();
  mock_server_->AddUpdateDirectory(alice_id, TestIdFactory::root(),
      "alice", 2, 20);
  mock_server_->SetLastUpdateDeleted();
  mock_server_->set_conflict_all_commits(true);
  syncer_->SyncShare(this);
  syncer_->SyncShare(this);
  {
    // Bob is the entry at the bottom of the tree.
    // The tree should be regenerated and old IDs removed.
    ReadTransaction trans(dir, __FILE__, __LINE__);
    Entry bob(&trans, GET_BY_ID, bob_id);
    ASSERT_TRUE(bob.good());
    EXPECT_FALSE(bob.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_TRUE(bob.Get(IS_UNSYNCED));

    // Old one should be deleted, but new one should have been made.
    Entry dead_alice(&trans, GET_BY_ID, alice_id);
    EXPECT_FALSE(dead_alice.good());
    EXPECT_NE(bob.Get(PARENT_ID), alice_id);

    // Newly born alice
    Entry alice(&trans, GET_BY_ID, bob.Get(PARENT_ID));
    ASSERT_TRUE(alice.good());
    EXPECT_FALSE(alice.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_TRUE(alice.Get(IS_UNSYNCED));
    EXPECT_FALSE(alice.Get(ID).ServerKnows());
    EXPECT_TRUE(alice.Get(NON_UNIQUE_NAME) == "alice");

    // Alice needs a parent as well. Old parent should have been erased.
    Entry dead_fred(&trans, GET_BY_ID, fred_id);
    EXPECT_FALSE(dead_fred.good());
    EXPECT_NE(alice.Get(PARENT_ID), fred_id);

    Entry fred(&trans, GET_BY_ID, alice.Get(PARENT_ID));
    ASSERT_TRUE(fred.good());
    EXPECT_EQ(fred.Get(PARENT_ID), TestIdFactory::root());
    EXPECT_TRUE(fred.Get(IS_UNSYNCED));
    EXPECT_FALSE(fred.Get(ID).ServerKnows());
    EXPECT_FALSE(fred.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_TRUE(fred.Get(NON_UNIQUE_NAME) == "fred");
  }
  syncer_events_.clear();
}

TEST_F(SyncerTest, WeMovedSomethingIntoAFolderHierarchyServerHasDeleted2) {
  // The difference here is that the hierarchy is not in the root. We have
  // another entry that shouldn't be touched.
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());

  const syncable::Id bob_id = ids_.NewServerId();
  const syncable::Id fred_id = ids_.NewServerId();
  const syncable::Id alice_id = ids_.NewServerId();
  const syncable::Id susan_id = ids_.NewServerId();

  mock_server_->AddUpdateDirectory(bob_id, TestIdFactory::root(),
      "bob", 1, 10);
  mock_server_->AddUpdateDirectory(susan_id, TestIdFactory::root(),
      "susan", 1, 10);
  mock_server_->AddUpdateDirectory(fred_id, susan_id, "fred", 1, 10);
  mock_server_->AddUpdateDirectory(alice_id, fred_id, "alice", 1, 10);
  syncer_->SyncShare(this);
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry bob(&trans, GET_BY_ID, bob_id);
    ASSERT_TRUE(bob.good());
    bob.Put(IS_UNSYNCED, true);
    bob.Put(PARENT_ID, alice_id);  // Move into alice.
  }
  mock_server_->AddUpdateDirectory(fred_id, TestIdFactory::root(),
      "fred", 2, 20);
  mock_server_->SetLastUpdateDeleted();
  mock_server_->AddUpdateDirectory(alice_id, TestIdFactory::root(),
      "alice", 2, 20);
  mock_server_->SetLastUpdateDeleted();
  mock_server_->set_conflict_all_commits(true);
  syncer_->SyncShare(this);
  syncer_->SyncShare(this);
  {
    // Root
    //   |- Susan
    //        |- Fred
    //            |- Alice
    //                 |- Bob

    ReadTransaction trans(dir, __FILE__, __LINE__);
    Entry bob(&trans, GET_BY_ID, bob_id);
    ASSERT_TRUE(bob.good());
    EXPECT_FALSE(bob.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_TRUE(bob.Get(IS_UNSYNCED));  // Parent changed
    EXPECT_NE(bob.Get(PARENT_ID), alice_id);

    // New one was born, this is the old one
    Entry dead_alice(&trans, GET_BY_ID, alice_id);
    EXPECT_FALSE(dead_alice.good());

    // Newly born
    Entry alice(&trans, GET_BY_ID, bob.Get(PARENT_ID));
    EXPECT_TRUE(alice.Get(IS_UNSYNCED));
    EXPECT_FALSE(alice.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_FALSE(alice.Get(ID).ServerKnows());
    EXPECT_NE(alice.Get(PARENT_ID), fred_id);  // This fred was deleted

    // New one was born, this is the old one
    Entry dead_fred(&trans, GET_BY_ID, fred_id);
    EXPECT_FALSE(dead_fred.good());

    // Newly born
    Entry fred(&trans, GET_BY_ID, alice.Get(PARENT_ID));
    EXPECT_TRUE(fred.Get(IS_UNSYNCED));
    EXPECT_FALSE(fred.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_FALSE(fred.Get(ID).ServerKnows());
    EXPECT_TRUE(fred.Get(PARENT_ID) == susan_id);

    // Unchanged
    Entry susan(&trans, GET_BY_ID, susan_id);
    ASSERT_TRUE(susan.good());
    EXPECT_FALSE(susan.Get(IS_UNSYNCED));
    EXPECT_TRUE(susan.Get(PARENT_ID) == root_id_);
    EXPECT_FALSE(susan.Get(IS_UNAPPLIED_UPDATE));
  }
  syncer_events_.clear();
}

// This test is to reproduce a check failure. Sometimes we would get a bad ID
// back when creating an entry.
TEST_F(SyncerTest, DuplicateIDReturn) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  ASSERT_TRUE(dir.good());
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry folder(&trans, CREATE, trans.root_id(), "bob");
    ASSERT_TRUE(folder.good());
    folder.Put(IS_UNSYNCED, true);
    folder.Put(IS_DIR, true);
    folder.Put(SPECIFICS, DefaultBookmarkSpecifics());
    MutableEntry folder2(&trans, CREATE, trans.root_id(), "fred");
    ASSERT_TRUE(folder2.good());
    folder2.Put(IS_UNSYNCED, false);
    folder2.Put(IS_DIR, true);
    folder2.Put(SPECIFICS, DefaultBookmarkSpecifics());
    folder2.Put(BASE_VERSION, 3);
    folder2.Put(ID, syncable::Id::CreateFromServerId("mock_server:10000"));
  }
  mock_server_->set_next_new_id(10000);
  EXPECT_TRUE(1 == dir->unsynced_entity_count());
  syncer_->SyncShare(this);  // we get back a bad id in here (should never happen).
  EXPECT_TRUE(1 == dir->unsynced_entity_count());
  syncer_->SyncShare(this);  // another bad id in here.
  EXPECT_TRUE(0 == dir->unsynced_entity_count());
  syncer_events_.clear();
}

TEST_F(SyncerTest, DeletedEntryWithBadParentInLoopCalculation) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  ASSERT_TRUE(dir.good());
  mock_server_->AddUpdateDirectory(1, 0, "bob", 1, 10);
  syncer_->SyncShare(this);
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry bob(&trans, GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(bob.good());
    // This is valid, because the parent could have gone away a long time ago.
    bob.Put(PARENT_ID, ids_.FromNumber(54));
    bob.Put(IS_DEL, true);
    bob.Put(IS_UNSYNCED, true);
  }
  mock_server_->AddUpdateDirectory(2, 1, "fred", 1, 10);
  syncer_->SyncShare(this);
  syncer_->SyncShare(this);
}

TEST_F(SyncerTest, ConflictResolverMergeOverwritesLocalEntry) {
  // This test would die because it would rename a entry to a name that was
  // taken in the namespace
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());

  ConflictSet conflict_set;
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);

    MutableEntry local_deleted(&trans, CREATE, trans.root_id(), "name");
    local_deleted.Put(ID, ids_.FromNumber(1));
    local_deleted.Put(BASE_VERSION, 1);
    local_deleted.Put(IS_DEL, true);
    local_deleted.Put(IS_UNSYNCED, true);

    MutableEntry in_the_way(&trans, CREATE, trans.root_id(), "name");
    in_the_way.Put(ID, ids_.FromNumber(2));
    in_the_way.Put(BASE_VERSION, 1);

    MutableEntry update(&trans, CREATE_NEW_UPDATE_ITEM, ids_.FromNumber(3));
    update.Put(BASE_VERSION, 1);
    update.Put(SERVER_NON_UNIQUE_NAME, "name");
    update.Put(PARENT_ID, ids_.FromNumber(0));
    update.Put(IS_UNAPPLIED_UPDATE, true);

    conflict_set.push_back(ids_.FromNumber(1));
    conflict_set.push_back(ids_.FromNumber(3));
  }
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    context_->resolver()->ProcessConflictSet(&trans, &conflict_set, 50);
  }
}

TEST_F(SyncerTest, ConflictResolverMergesLocalDeleteAndServerUpdate) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());

  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);

    MutableEntry local_deleted(&trans, CREATE, trans.root_id(), "name");
    local_deleted.Put(ID, ids_.FromNumber(1));
    local_deleted.Put(BASE_VERSION, 1);
    local_deleted.Put(IS_DEL, true);
    local_deleted.Put(IS_DIR, false);
    local_deleted.Put(IS_UNSYNCED, true);
    local_deleted.Put(SPECIFICS, DefaultBookmarkSpecifics());
  }

  mock_server_->AddUpdateBookmark(ids_.FromNumber(1), root_id_, "name", 10, 10);

  // We don't care about actually committing, just the resolution.
  mock_server_->set_conflict_all_commits(true);
  syncer_->SyncShare(this);

  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    Entry local_deleted(&trans, GET_BY_ID, ids_.FromNumber(1));
    EXPECT_TRUE(local_deleted.Get(BASE_VERSION) == 10);
    EXPECT_TRUE(local_deleted.Get(IS_UNAPPLIED_UPDATE) == false);
    EXPECT_TRUE(local_deleted.Get(IS_UNSYNCED) == true);
    EXPECT_TRUE(local_deleted.Get(IS_DEL) == true);
    EXPECT_TRUE(local_deleted.Get(IS_DIR) == false);
  }
}

// See what happens if the IS_DIR bit gets flipped.  This can cause us
// all kinds of disasters.
TEST_F(SyncerTest, UpdateFlipsTheFolderBit) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());

  // Local object: a deleted directory (container), revision 1, unsynced.
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);

    MutableEntry local_deleted(&trans, CREATE, trans.root_id(), "name");
    local_deleted.Put(ID, ids_.FromNumber(1));
    local_deleted.Put(BASE_VERSION, 1);
    local_deleted.Put(IS_DEL, true);
    local_deleted.Put(IS_DIR, true);
    local_deleted.Put(IS_UNSYNCED, true);
  }

  // Server update: entry-type object (not a container), revision 10.
  mock_server_->AddUpdateBookmark(ids_.FromNumber(1), root_id_, "name", 10, 10);

  // Don't attempt to commit.
  mock_server_->set_conflict_all_commits(true);

  // The syncer should not attempt to apply the invalid update.
  syncer_->SyncShare(this);

  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    Entry local_deleted(&trans, GET_BY_ID, ids_.FromNumber(1));
    EXPECT_TRUE(local_deleted.Get(BASE_VERSION) == 1);
    EXPECT_TRUE(local_deleted.Get(IS_UNAPPLIED_UPDATE) == false);
    EXPECT_TRUE(local_deleted.Get(IS_UNSYNCED) == true);
    EXPECT_TRUE(local_deleted.Get(IS_DEL) == true);
    EXPECT_TRUE(local_deleted.Get(IS_DIR) == true);
  }
}

TEST(SyncerSyncProcessState, MergeSetsTest) {
  TestIdFactory id_factory;
  syncable::Id id[7];
  for (int i = 1; i < 7; i++) {
    id[i] = id_factory.NewServerId();
  }
  ConflictProgress c;
  c.MergeSets(id[1], id[2]);
  c.MergeSets(id[2], id[3]);
  c.MergeSets(id[4], id[5]);
  c.MergeSets(id[5], id[6]);
  EXPECT_TRUE(6 == c.IdToConflictSetSize());
  for (int i = 1; i < 7; i++) {
    EXPECT_TRUE(NULL != c.IdToConflictSetGet(id[i]));
    EXPECT_TRUE(c.IdToConflictSetGet(id[(i & ~3) + 1]) ==
                c.IdToConflictSetGet(id[i]));
  }
  c.MergeSets(id[1], id[6]);
  for (int i = 1; i < 7; i++) {
    EXPECT_TRUE(NULL != c.IdToConflictSetGet(id[i]));
    EXPECT_TRUE(c.IdToConflictSetGet(id[1]) == c.IdToConflictSetGet(id[i]));
  }

  // Check dupes don't cause double sets.
  ConflictProgress identical_set;
  identical_set.MergeSets(id[1], id[1]);
  EXPECT_TRUE(identical_set.IdToConflictSetSize() == 1);
  EXPECT_TRUE(identical_set.IdToConflictSetGet(id[1])->size() == 1);
}

// Bug Synopsis:
// Merge conflict resolution will merge a new local entry with another entry
// that needs updates, resulting in CHECK.
TEST_F(SyncerTest, MergingExistingItems) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());
  mock_server_->set_conflict_all_commits(true);
  mock_server_->AddUpdateBookmark(1, 0, "base", 10, 10);
  syncer_->SyncShare(this);
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry entry(&trans, CREATE, trans.root_id(), "Copy of base");
    WriteTestDataToEntry(&trans, &entry);
  }
  mock_server_->AddUpdateBookmark(1, 0, "Copy of base", 50, 50);
  SyncRepeatedlyToTriggerConflictResolution(session_.get());
}

// In this test a long changelog contains a child at the start of the changelog
// and a parent at the end. While these updates are in progress the client would
// appear stuck.
TEST_F(SyncerTest, LongChangelistWithApplicationConflict) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());
  const int DEPTH = 400;
  syncable::Id folder_id = ids_.FromNumber(1);

  // First we an item in a folder in the root. However the folder won't come
  // till much later.
  syncable::Id stuck_entry_id = TestIdFactory::FromNumber(99999);
  mock_server_->AddUpdateDirectory(stuck_entry_id,
      folder_id, "stuck", 1, 1);
  mock_server_->SetChangesRemaining(DEPTH - 1);
  syncer_->SyncShare(session_.get());

  // Very long changelist. We should never be stuck.
  for (int i = 0; i < DEPTH; i++) {
    mock_server_->SetNewTimestamp(i);
    mock_server_->SetChangesRemaining(DEPTH - i);
    syncer_->SyncShare(session_.get());
    EXPECT_FALSE(session_->status_controller()->syncer_status().syncer_stuck);

    // Ensure our folder hasn't somehow applied.
    ReadTransaction trans(dir, __FILE__, __LINE__);
    Entry child(&trans, GET_BY_ID, stuck_entry_id);
    EXPECT_TRUE(child.good());
    EXPECT_TRUE(child.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_TRUE(child.Get(IS_DEL));
    EXPECT_FALSE(child.Get(IS_UNSYNCED));
  }

  // And finally the folder.
  mock_server_->AddUpdateDirectory(folder_id,
      TestIdFactory::root(), "folder", 1, 1);
  mock_server_->SetChangesRemaining(0);
  LoopSyncShare(syncer_);
  LoopSyncShare(syncer_);
  // Check that everything is as expected after the commit.
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    Entry entry(&trans, GET_BY_ID, folder_id);
    ASSERT_TRUE(entry.good());
    Entry child(&trans, GET_BY_ID, stuck_entry_id);
    EXPECT_EQ(entry.Get(ID), child.Get(PARENT_ID));
    EXPECT_EQ("stuck", child.Get(NON_UNIQUE_NAME));
    EXPECT_TRUE(child.good());
  }
}

TEST_F(SyncerTest, DontMergeTwoExistingItems) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  EXPECT_TRUE(dir.good());
  mock_server_->set_conflict_all_commits(true);
  mock_server_->AddUpdateBookmark(1, 0, "base", 10, 10);
  mock_server_->AddUpdateBookmark(2, 0, "base2", 10, 10);
  syncer_->SyncShare(this);
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry entry(&trans, GET_BY_ID, ids_.FromNumber(2));
    ASSERT_TRUE(entry.good());
    EXPECT_TRUE(entry.Put(NON_UNIQUE_NAME, "Copy of base"));
    entry.Put(IS_UNSYNCED, true);
  }
  mock_server_->AddUpdateBookmark(1, 0, "Copy of base", 50, 50);
  SyncRepeatedlyToTriggerConflictResolution(session_.get());
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    Entry entry1(&trans, GET_BY_ID, ids_.FromNumber(1));
    EXPECT_FALSE(entry1.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_FALSE(entry1.Get(IS_UNSYNCED));
    EXPECT_FALSE(entry1.Get(IS_DEL));
    Entry entry2(&trans, GET_BY_ID, ids_.FromNumber(2));
    EXPECT_FALSE(entry2.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_TRUE(entry2.Get(IS_UNSYNCED));
    EXPECT_FALSE(entry2.Get(IS_DEL));
    EXPECT_EQ(entry1.Get(NON_UNIQUE_NAME), entry2.Get(NON_UNIQUE_NAME));
  }
}

TEST_F(SyncerTest, TestUndeleteUpdate) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  EXPECT_TRUE(dir.good());
  mock_server_->set_conflict_all_commits(true);
  mock_server_->AddUpdateDirectory(1, 0, "foo", 1, 1);
  mock_server_->AddUpdateDirectory(2, 1, "bar", 1, 2);
  syncer_->SyncShare(this);
  mock_server_->AddUpdateDirectory(2, 1, "bar", 2, 3);
  mock_server_->SetLastUpdateDeleted();
  syncer_->SyncShare(this);
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    Entry entry(&trans, GET_BY_ID, ids_.FromNumber(2));
    ASSERT_TRUE(entry.good());
    EXPECT_TRUE(entry.Get(IS_DEL));
  }
  mock_server_->AddUpdateDirectory(1, 0, "foo", 2, 4);
  mock_server_->SetLastUpdateDeleted();
  syncer_->SyncShare(this);
  // This used to be rejected as it's an undeletion. Now, it results in moving
  // the delete path aside.
  mock_server_->AddUpdateDirectory(2, 1, "bar", 3, 5);
  syncer_->SyncShare(this);
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    Entry entry(&trans, GET_BY_ID, ids_.FromNumber(2));
    ASSERT_TRUE(entry.good());
    EXPECT_TRUE(entry.Get(IS_DEL));
    EXPECT_FALSE(entry.Get(SERVER_IS_DEL));
    EXPECT_TRUE(entry.Get(IS_UNAPPLIED_UPDATE));
  }
}

TEST_F(SyncerTest, TestMoveSanitizedNamedFolder) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  EXPECT_TRUE(dir.good());
  mock_server_->AddUpdateDirectory(1, 0, "foo", 1, 1);
  mock_server_->AddUpdateDirectory(2, 0, ":::", 1, 2);
  syncer_->SyncShare(this);
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry entry(&trans, GET_BY_ID, ids_.FromNumber(2));
    ASSERT_TRUE(entry.good());
    EXPECT_TRUE(entry.Put(PARENT_ID, ids_.FromNumber(1)));
    EXPECT_TRUE(entry.Put(IS_UNSYNCED, true));
  }
  syncer_->SyncShare(this);
  // We use the same sync ts as before so our times match up.
  mock_server_->AddUpdateDirectory(2, 1, ":::", 2, 2);
  syncer_->SyncShare(this);
}

TEST(SortedCollectionsIntersect, SortedCollectionsIntersectTest) {
  int negative[] = {-3, -2, -1};
  int straddle[] = {-1, 0, 1};
  int positive[] = {1, 2, 3};
  EXPECT_TRUE(SortedCollectionsIntersect(negative, negative + 3,
                                         straddle, straddle + 3));
  EXPECT_FALSE(SortedCollectionsIntersect(negative, negative + 3,
                                          positive, positive + 3));
  EXPECT_TRUE(SortedCollectionsIntersect(straddle, straddle + 3,
                                         positive, positive + 3));
  EXPECT_FALSE(SortedCollectionsIntersect(straddle + 2, straddle + 3,
                                          positive, positive));
  EXPECT_FALSE(SortedCollectionsIntersect(straddle, straddle + 3,
                                          positive + 1, positive + 1));
  EXPECT_TRUE(SortedCollectionsIntersect(straddle, straddle + 3,
                                         positive, positive + 1));
}

// Don't crash when this occurs.
TEST_F(SyncerTest, UpdateWhereParentIsNotAFolder) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());
  mock_server_->AddUpdateBookmark(1, 0, "B", 10, 10);
  mock_server_->AddUpdateDirectory(2, 1, "BookmarkParent", 10, 10);
  // Used to cause a CHECK
  syncer_->SyncShare(this);
  {
    ReadTransaction rtrans(dir, __FILE__, __LINE__);
    Entry good_entry(&rtrans, syncable::GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(good_entry.good());
    EXPECT_FALSE(good_entry.Get(IS_UNAPPLIED_UPDATE));
    Entry bad_parent(&rtrans, syncable::GET_BY_ID, ids_.FromNumber(2));
    ASSERT_TRUE(bad_parent.good());
    EXPECT_TRUE(bad_parent.Get(IS_UNAPPLIED_UPDATE));
  }
}

const char kRootId[] = "0";

TEST_F(SyncerTest, DirectoryUpdateTest) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());

  Id in_root_id = ids_.NewServerId();
  Id in_in_root_id = ids_.NewServerId();

  mock_server_->AddUpdateDirectory(in_root_id, TestIdFactory::root(),
                                   "in_root_name", 2, 2);
  mock_server_->AddUpdateDirectory(in_in_root_id, in_root_id,
                                   "in_in_root_name", 3, 3);
  syncer_->SyncShare(this);
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    Entry in_root(&trans, GET_BY_ID, in_root_id);
    ASSERT_TRUE(in_root.good());
    EXPECT_EQ("in_root_name", in_root.Get(NON_UNIQUE_NAME));
    EXPECT_EQ(TestIdFactory::root(), in_root.Get(PARENT_ID));

    Entry in_in_root(&trans, GET_BY_ID, in_in_root_id);
    ASSERT_TRUE(in_in_root.good());
    EXPECT_EQ("in_in_root_name", in_in_root.Get(NON_UNIQUE_NAME));
    EXPECT_EQ(in_root_id, in_in_root.Get(PARENT_ID));
  }
}

TEST_F(SyncerTest, DirectoryCommitTest) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());

  syncable::Id in_root_id, in_dir_id;
  int64 foo_metahandle;
  int64 bar_metahandle;

  {
    WriteTransaction wtrans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry parent(&wtrans, syncable::CREATE, root_id_, "foo");
    ASSERT_TRUE(parent.good());
    parent.Put(syncable::IS_UNSYNCED, true);
    parent.Put(syncable::IS_DIR, true);
    parent.Put(syncable::SPECIFICS, DefaultBookmarkSpecifics());
    in_root_id = parent.Get(syncable::ID);
    foo_metahandle = parent.Get(META_HANDLE);

    MutableEntry child(&wtrans, syncable::CREATE, parent.Get(ID), "bar");
    ASSERT_TRUE(child.good());
    child.Put(syncable::IS_UNSYNCED, true);
    child.Put(syncable::IS_DIR, true);
    child.Put(syncable::SPECIFICS, DefaultBookmarkSpecifics());
    bar_metahandle = child.Get(META_HANDLE);
    in_dir_id = parent.Get(syncable::ID);
  }
  syncer_->SyncShare(this);
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    Entry fail_by_old_id_entry(&trans, GET_BY_ID, in_root_id);
    ASSERT_FALSE(fail_by_old_id_entry.good());

    Entry foo_entry(&trans, GET_BY_HANDLE, foo_metahandle);
    ASSERT_TRUE(foo_entry.good());
    EXPECT_EQ("foo", foo_entry.Get(NON_UNIQUE_NAME));
    EXPECT_NE(foo_entry.Get(syncable::ID), in_root_id);

    Entry bar_entry(&trans, GET_BY_HANDLE, bar_metahandle);
    ASSERT_TRUE(bar_entry.good());
    EXPECT_EQ("bar", bar_entry.Get(NON_UNIQUE_NAME));
    EXPECT_NE(bar_entry.Get(syncable::ID), in_dir_id);
    EXPECT_EQ(foo_entry.Get(syncable::ID), bar_entry.Get(PARENT_ID));
  }
}

TEST_F(SyncerTest, ConflictSetSizeReducedToOne) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());

  syncable::Id in_root_id = ids_.NewServerId();

  mock_server_->AddUpdateBookmark(in_root_id, TestIdFactory::root(),
      "in_root", 1, 1);
  syncer_->SyncShare(this);
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry oentry(&trans, GET_BY_ID, in_root_id);
    ASSERT_TRUE(oentry.good());
    oentry.Put(NON_UNIQUE_NAME, "old_in_root");
    WriteTestDataToEntry(&trans, &oentry);
    MutableEntry entry(&trans, CREATE, trans.root_id(), "in_root");
    ASSERT_TRUE(entry.good());
    WriteTestDataToEntry(&trans, &entry);
  }
  mock_server_->set_conflict_all_commits(true);
  // This SyncShare call used to result in a CHECK failure.
  syncer_->SyncShare(this);
  syncer_events_.clear();
}

TEST_F(SyncerTest, TestClientCommand) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());
  using sync_pb::ClientCommand;

  ClientCommand* command = mock_server_->GetNextClientCommand();
  command->set_set_sync_poll_interval(8);
  command->set_set_sync_long_poll_interval(800);
  mock_server_->AddUpdateDirectory(1, 0, "in_root", 1, 1);
  syncer_->SyncShare(this);

  EXPECT_TRUE(TimeDelta::FromSeconds(8) ==
              last_short_poll_interval_received_);
  EXPECT_TRUE(TimeDelta::FromSeconds(800) ==
              last_long_poll_interval_received_);

  command = mock_server_->GetNextClientCommand();
  command->set_set_sync_poll_interval(180);
  command->set_set_sync_long_poll_interval(190);
  mock_server_->AddUpdateDirectory(1, 0, "in_root", 1, 1);
  syncer_->SyncShare(this);

  EXPECT_TRUE(TimeDelta::FromSeconds(180) ==
              last_short_poll_interval_received_);
  EXPECT_TRUE(TimeDelta::FromSeconds(190) ==
              last_long_poll_interval_received_);
}

TEST_F(SyncerTest, EnsureWeSendUpOldParent) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());

  syncable::Id folder_one_id = ids_.FromNumber(1);
  syncable::Id folder_two_id = ids_.FromNumber(2);

  mock_server_->AddUpdateDirectory(folder_one_id, TestIdFactory::root(),
      "folder_one", 1, 1);
  mock_server_->AddUpdateDirectory(folder_two_id, TestIdFactory::root(),
      "folder_two", 1, 1);
  syncer_->SyncShare(this);
  {
    // A moved entry should send an "old parent."
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry entry(&trans, GET_BY_ID, folder_one_id);
    ASSERT_TRUE(entry.good());
    entry.Put(PARENT_ID, folder_two_id);
    entry.Put(IS_UNSYNCED, true);
    // A new entry should send no "old parent."
    MutableEntry create(&trans, CREATE, trans.root_id(), "new_folder");
    create.Put(IS_UNSYNCED, true);
    create.Put(SPECIFICS, DefaultBookmarkSpecifics());
  }
  syncer_->SyncShare(this);
  const sync_pb::CommitMessage& commit = mock_server_->last_sent_commit();
  ASSERT_TRUE(2 == commit.entries_size());
  EXPECT_TRUE(commit.entries(0).parent_id_string() == "2");
  EXPECT_TRUE(commit.entries(0).old_parent_id() == "0");
  EXPECT_FALSE(commit.entries(1).has_old_parent_id());
}

TEST_F(SyncerTest, Test64BitVersionSupport) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());
  int64 really_big_int = std::numeric_limits<int64>::max() - 12;
  const string name("ringo's dang orang ran rings around my o-ring");
  int64 item_metahandle;

  // Try writing max int64 to the version fields of a meta entry.
  {
    WriteTransaction wtrans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry entry(&wtrans, syncable::CREATE, wtrans.root_id(), name);
    ASSERT_TRUE(entry.good());
    entry.Put(syncable::BASE_VERSION, really_big_int);
    entry.Put(syncable::SERVER_VERSION, really_big_int);
    entry.Put(syncable::ID, ids_.NewServerId());
    item_metahandle = entry.Get(META_HANDLE);
  }
  // Now read it back out and make sure the value is max int64.
  ReadTransaction rtrans(dir, __FILE__, __LINE__);
  Entry entry(&rtrans, syncable::GET_BY_HANDLE, item_metahandle);
  ASSERT_TRUE(entry.good());
  EXPECT_TRUE(really_big_int == entry.Get(syncable::BASE_VERSION));
}

TEST_F(SyncerTest, TestSimpleUndelete) {
  Id id = ids_.MakeServer("undeletion item"), root = TestIdFactory::root();
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  EXPECT_TRUE(dir.good());
  mock_server_->set_conflict_all_commits(true);
  // Let there be an entry from the server.
  mock_server_->AddUpdateBookmark(id, root, "foo", 1, 10);
  syncer_->SyncShare(this);
  // Check it out and delete it.
  {
    WriteTransaction wtrans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry entry(&wtrans, GET_BY_ID, id);
    ASSERT_TRUE(entry.good());
    EXPECT_FALSE(entry.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_FALSE(entry.Get(IS_UNSYNCED));
    EXPECT_FALSE(entry.Get(IS_DEL));
    // Delete it locally.
    entry.Put(IS_DEL, true);
  }
  syncer_->SyncShare(this);
  // Confirm we see IS_DEL and not SERVER_IS_DEL.
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    Entry entry(&trans, GET_BY_ID, id);
    ASSERT_TRUE(entry.good());
    EXPECT_FALSE(entry.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_FALSE(entry.Get(IS_UNSYNCED));
    EXPECT_TRUE(entry.Get(IS_DEL));
    EXPECT_FALSE(entry.Get(SERVER_IS_DEL));
  }
  syncer_->SyncShare(this);
  // Update from server confirming deletion.
  mock_server_->AddUpdateBookmark(id, root, "foo", 2, 11);
  mock_server_->SetLastUpdateDeleted();
  syncer_->SyncShare(this);
  // IS_DEL AND SERVER_IS_DEL now both true.
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    Entry entry(&trans, GET_BY_ID, id);
    ASSERT_TRUE(entry.good());
    EXPECT_FALSE(entry.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_FALSE(entry.Get(IS_UNSYNCED));
    EXPECT_TRUE(entry.Get(IS_DEL));
    EXPECT_TRUE(entry.Get(SERVER_IS_DEL));
  }
  // Undelete from server.
  mock_server_->AddUpdateBookmark(id, root, "foo", 2, 12);
  syncer_->SyncShare(this);
  // IS_DEL and SERVER_IS_DEL now both false.
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    Entry entry(&trans, GET_BY_ID, id);
    ASSERT_TRUE(entry.good());
    EXPECT_FALSE(entry.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_FALSE(entry.Get(IS_UNSYNCED));
    EXPECT_FALSE(entry.Get(IS_DEL));
    EXPECT_FALSE(entry.Get(SERVER_IS_DEL));
  }
}

TEST_F(SyncerTest, TestUndeleteWithMissingDeleteUpdate) {
  Id id = ids_.MakeServer("undeletion item"), root = TestIdFactory::root();
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  EXPECT_TRUE(dir.good());
  // Let there be a entry, from the server.
  mock_server_->set_conflict_all_commits(true);
  mock_server_->AddUpdateBookmark(id, root, "foo", 1, 10);
  syncer_->SyncShare(this);
  // Check it out and delete it.
  {
    WriteTransaction wtrans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry entry(&wtrans, GET_BY_ID, id);
    ASSERT_TRUE(entry.good());
    EXPECT_FALSE(entry.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_FALSE(entry.Get(IS_UNSYNCED));
    EXPECT_FALSE(entry.Get(IS_DEL));
    // Delete it locally.
    entry.Put(IS_DEL, true);
  }
  syncer_->SyncShare(this);
  // Confirm we see IS_DEL and not SERVER_IS_DEL.
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    Entry entry(&trans, GET_BY_ID, id);
    ASSERT_TRUE(entry.good());
    EXPECT_FALSE(entry.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_FALSE(entry.Get(IS_UNSYNCED));
    EXPECT_TRUE(entry.Get(IS_DEL));
    EXPECT_FALSE(entry.Get(SERVER_IS_DEL));
  }
  syncer_->SyncShare(this);
  // Say we do not get an update from server confirming deletion. Undelete
  // from server
  mock_server_->AddUpdateBookmark(id, root, "foo", 2, 12);
  syncer_->SyncShare(this);
  // IS_DEL and SERVER_IS_DEL now both false.
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    Entry entry(&trans, GET_BY_ID, id);
    ASSERT_TRUE(entry.good());
    EXPECT_FALSE(entry.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_FALSE(entry.Get(IS_UNSYNCED));
    EXPECT_FALSE(entry.Get(IS_DEL));
    EXPECT_FALSE(entry.Get(SERVER_IS_DEL));
  }
}

TEST_F(SyncerTest, TestUndeleteIgnoreCorrectlyUnappliedUpdate) {
  Id id1 = ids_.MakeServer("first"), id2 = ids_.MakeServer("second");
  Id root = TestIdFactory::root();
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  EXPECT_TRUE(dir.good());
  // Duplicate! expect path clashing!
  mock_server_->set_conflict_all_commits(true);
  mock_server_->AddUpdateBookmark(id1, root, "foo", 1, 10);
  mock_server_->AddUpdateBookmark(id2, root, "foo", 1, 10);
  syncer_->SyncShare(this);
  mock_server_->AddUpdateBookmark(id2, root, "foo2", 1, 10);
  syncer_->SyncShare(this);  // Now just don't explode.
}

TEST_F(SyncerTest, SingletonTagUpdates) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  EXPECT_TRUE(dir.good());
  // As a hurdle, introduce an item whose name is the same as the tag value
  // we'll use later.
  int64 hurdle_handle = CreateUnsyncedDirectory("bob", "id_bob");
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    Entry hurdle(&trans, GET_BY_HANDLE, hurdle_handle);
    ASSERT_TRUE(hurdle.good());
    ASSERT_TRUE(!hurdle.Get(IS_DEL));
    ASSERT_TRUE(hurdle.Get(SINGLETON_TAG).empty());
    ASSERT_TRUE(hurdle.Get(NON_UNIQUE_NAME) == "bob");

    // Try to lookup by the tagname.  These should fail.
    Entry tag_alpha(&trans, GET_BY_TAG, "alpha");
    EXPECT_FALSE(tag_alpha.good());
    Entry tag_bob(&trans, GET_BY_TAG, "bob");
    EXPECT_FALSE(tag_bob.good());
  }

  // Now download some tagged items as updates.
  mock_server_->AddUpdateDirectory(1, 0, "update1", 1, 10);
  mock_server_->SetLastUpdateSingletonTag("alpha");
  mock_server_->AddUpdateDirectory(2, 0, "update2", 2, 20);
  mock_server_->SetLastUpdateSingletonTag("bob");
  syncer_->SyncShare(this);

  {
    ReadTransaction trans(dir, __FILE__, __LINE__);

    // The new items should be applied as new entries, and we should be able
    // to look them up by their tag values.
    Entry tag_alpha(&trans, GET_BY_TAG, "alpha");
    ASSERT_TRUE(tag_alpha.good());
    ASSERT_TRUE(!tag_alpha.Get(IS_DEL));
    ASSERT_TRUE(tag_alpha.Get(SINGLETON_TAG) == "alpha");
    ASSERT_TRUE(tag_alpha.Get(NON_UNIQUE_NAME) == "update1");
    Entry tag_bob(&trans, GET_BY_TAG, "bob");
    ASSERT_TRUE(tag_bob.good());
    ASSERT_TRUE(!tag_bob.Get(IS_DEL));
    ASSERT_TRUE(tag_bob.Get(SINGLETON_TAG) == "bob");
    ASSERT_TRUE(tag_bob.Get(NON_UNIQUE_NAME) == "update2");
    // The old item should be unchanged.
    Entry hurdle(&trans, GET_BY_HANDLE, hurdle_handle);
    ASSERT_TRUE(hurdle.good());
    ASSERT_TRUE(!hurdle.Get(IS_DEL));
    ASSERT_TRUE(hurdle.Get(SINGLETON_TAG).empty());
    ASSERT_TRUE(hurdle.Get(NON_UNIQUE_NAME) == "bob");
  }
}

class SyncerPositionUpdateTest : public SyncerTest {
 public:
  SyncerPositionUpdateTest() : next_update_id_(1), next_revision_(1) {}

 protected:
  void ExpectLocalItemsInServerOrder() {
    if (position_map_.empty())
      return;

    ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
    EXPECT_TRUE(dir.good());
    ReadTransaction trans(dir, __FILE__, __LINE__);

    Id prev_id;
    DCHECK(prev_id.IsRoot());
    PosMap::iterator next = position_map_.begin();
    for (PosMap::iterator i = next++; i != position_map_.end(); ++i) {
      Id id = i->second;
      Entry entry_with_id(&trans, GET_BY_ID, id);
      EXPECT_TRUE(entry_with_id.good());
      EXPECT_TRUE(entry_with_id.Get(PREV_ID) == prev_id);
      EXPECT_TRUE(entry_with_id.Get(SERVER_POSITION_IN_PARENT) == i->first);
      if (next == position_map_.end()) {
        EXPECT_TRUE(entry_with_id.Get(NEXT_ID).IsRoot());
      } else {
        EXPECT_TRUE(entry_with_id.Get(NEXT_ID) == next->second);
        next++;
      }
      prev_id = id;
    }
  }

  void AddRootItemWithPosition(int64 position) {
    string id = string("ServerId") + Int64ToString(next_update_id_++);
    string name = "my name is my id -- " + id;
    int revision = next_revision_++;
    mock_server_->AddUpdateDirectory(id, kRootId, name, revision, revision);
    mock_server_->SetLastUpdatePosition(position);
    position_map_.insert(
        PosMap::value_type(position, Id::CreateFromServerId(id)));
  }
 private:
  typedef multimap<int64, Id> PosMap;
  PosMap position_map_;
  int next_update_id_;
  int next_revision_;
  DISALLOW_COPY_AND_ASSIGN(SyncerPositionUpdateTest);
};

TEST_F(SyncerPositionUpdateTest, InOrderPositive) {
  // Add a bunch of items in increasing order, starting with just positive
  // position values.
  AddRootItemWithPosition(100);
  AddRootItemWithPosition(199);
  AddRootItemWithPosition(200);
  AddRootItemWithPosition(201);
  AddRootItemWithPosition(400);

  syncer_->SyncShare(this);
  ExpectLocalItemsInServerOrder();
}

TEST_F(SyncerPositionUpdateTest, InOrderNegative) {
  // Test negative position values, but in increasing order.
  AddRootItemWithPosition(-400);
  AddRootItemWithPosition(-201);
  AddRootItemWithPosition(-200);
  AddRootItemWithPosition(-150);
  AddRootItemWithPosition(100);

  syncer_->SyncShare(this);
  ExpectLocalItemsInServerOrder();
}

TEST_F(SyncerPositionUpdateTest, ReverseOrder) {
  // Test when items are sent in the reverse order.
  AddRootItemWithPosition(400);
  AddRootItemWithPosition(201);
  AddRootItemWithPosition(200);
  AddRootItemWithPosition(100);
  AddRootItemWithPosition(-150);
  AddRootItemWithPosition(-201);
  AddRootItemWithPosition(-200);
  AddRootItemWithPosition(-400);

  syncer_->SyncShare(this);
  ExpectLocalItemsInServerOrder();
}

TEST_F(SyncerPositionUpdateTest, RandomOrderInBatches) {
  // Mix it all up, interleaving position values, and try multiple batches of
  // updates.
  AddRootItemWithPosition(400);
  AddRootItemWithPosition(201);
  AddRootItemWithPosition(-400);
  AddRootItemWithPosition(100);

  syncer_->SyncShare(this);
  ExpectLocalItemsInServerOrder();

  AddRootItemWithPosition(-150);
  AddRootItemWithPosition(-200);
  AddRootItemWithPosition(200);
  AddRootItemWithPosition(-201);

  syncer_->SyncShare(this);
  ExpectLocalItemsInServerOrder();

  AddRootItemWithPosition(-144);

  syncer_->SyncShare(this);
  ExpectLocalItemsInServerOrder();
}

class SyncerPositionTiebreakingTest : public SyncerTest {
 public:
  SyncerPositionTiebreakingTest()
      : low_id_(Id::CreateFromServerId("A")),
        mid_id_(Id::CreateFromServerId("M")),
        high_id_(Id::CreateFromServerId("Z")),
        next_revision_(1) {
    DCHECK(low_id_ < mid_id_);
    DCHECK(mid_id_ < high_id_);
    DCHECK(low_id_ < high_id_);
  }

  // Adds the item by its Id, using a constant value for the position
  // so that the syncer has to resolve the order some other way.
  void Add(const Id& id) {
    int revision = next_revision_++;
    mock_server_->AddUpdateDirectory(id.GetServerId(), kRootId,
        id.GetServerId(), revision, revision);
    // The update position doesn't vary.
    mock_server_->SetLastUpdatePosition(90210);
  }

  void ExpectLocalOrderIsByServerId() {
    ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
    EXPECT_TRUE(dir.good());
    ReadTransaction trans(dir, __FILE__, __LINE__);
    Id null_id;
    Entry low(&trans, GET_BY_ID, low_id_);
    Entry mid(&trans, GET_BY_ID, mid_id_);
    Entry high(&trans, GET_BY_ID, high_id_);
    EXPECT_TRUE(low.good());
    EXPECT_TRUE(mid.good());
    EXPECT_TRUE(high.good());
    EXPECT_TRUE(low.Get(PREV_ID) == null_id);
    EXPECT_TRUE(mid.Get(PREV_ID) == low_id_);
    EXPECT_TRUE(high.Get(PREV_ID) == mid_id_);
    EXPECT_TRUE(high.Get(NEXT_ID) == null_id);
    EXPECT_TRUE(mid.Get(NEXT_ID) == high_id_);
    EXPECT_TRUE(low.Get(NEXT_ID) == mid_id_);
  }

 protected:
  // When there's a tiebreak on the numeric position, it's supposed to be
  // broken by string comparison of the ids.  These ids are in increasing
  // order.
  const Id low_id_;
  const Id mid_id_;
  const Id high_id_;

 private:
  int next_revision_;
  DISALLOW_COPY_AND_ASSIGN(SyncerPositionTiebreakingTest);
};

TEST_F(SyncerPositionTiebreakingTest, LowMidHigh) {
  Add(low_id_);
  Add(mid_id_);
  Add(high_id_);
  syncer_->SyncShare(this);
  ExpectLocalOrderIsByServerId();
}

TEST_F(SyncerPositionTiebreakingTest, LowHighMid) {
  Add(low_id_);
  Add(high_id_);
  Add(mid_id_);
  syncer_->SyncShare(this);
  ExpectLocalOrderIsByServerId();
}

TEST_F(SyncerPositionTiebreakingTest, HighMidLow) {
  Add(high_id_);
  Add(mid_id_);
  Add(low_id_);
  syncer_->SyncShare(this);
  ExpectLocalOrderIsByServerId();
}

TEST_F(SyncerPositionTiebreakingTest, HighLowMid) {
  Add(high_id_);
  Add(low_id_);
  Add(mid_id_);
  syncer_->SyncShare(this);
  ExpectLocalOrderIsByServerId();
}

TEST_F(SyncerPositionTiebreakingTest, MidHighLow) {
  Add(mid_id_);
  Add(high_id_);
  Add(low_id_);
  syncer_->SyncShare(this);
  ExpectLocalOrderIsByServerId();
}

TEST_F(SyncerPositionTiebreakingTest, MidLowHigh) {
  Add(mid_id_);
  Add(low_id_);
  Add(high_id_);
  syncer_->SyncShare(this);
  ExpectLocalOrderIsByServerId();
}

const SyncerTest::CommitOrderingTest
SyncerTest::CommitOrderingTest::LAST_COMMIT_ITEM = {-1, TestIdFactory::root()};

}  // namespace browser_sync
