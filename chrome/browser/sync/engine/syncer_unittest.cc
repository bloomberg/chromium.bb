// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE entry.
//
// Syncer unit tests. Unfortunately a lot of these tests
// are outdated and need to be reworked and updated.

#include <list>
#include <map>
#include <set>

#include "base/at_exit.h"

#include "base/scoped_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/sync/engine/client_command_channel.h"
#include "chrome/browser/sync/engine/conflict_resolution_view.h"
#include "chrome/browser/sync/engine/conflict_resolver.h"
#include "chrome/browser/sync/engine/get_commit_ids_command.h"
#include "chrome/browser/sync/engine/model_safe_worker.h"
#include "chrome/browser/sync/engine/net/server_connection_manager.h"
#include "chrome/browser/sync/engine/process_updates_command.h"
#include "chrome/browser/sync/engine/syncer.h"
#include "chrome/browser/sync/engine/syncer_util.h"
#include "chrome/browser/sync/engine/syncer_proto_util.h"
#include "chrome/browser/sync/engine/syncer_session.h"
#include "chrome/browser/sync/protocol/sync.pb.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/browser/sync/syncable/syncable.h"
#include "chrome/browser/sync/util/character_set_converters.h"
#include "chrome/browser/sync/util/compat_file.h"
#include "chrome/browser/sync/util/event_sys-inl.h"
#include "chrome/test/sync/engine/mock_server_connection.h"
#include "chrome/test/sync/engine/test_directory_setter_upper.h"
#include "chrome/test/sync/engine/test_id_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

using std::map;
using std::multimap;
using std::set;
using std::string;

namespace browser_sync {

using syncable::BaseTransaction;
using syncable::Blob;
using syncable::Directory;
using syncable::Entry;
using syncable::ExtendedAttribute;
using syncable::ExtendedAttributeKey;
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
using syncable::GET_BY_PARENTID_AND_NAME;
using syncable::GET_BY_PATH;
using syncable::GET_BY_TAG;
using syncable::ID;
using syncable::IS_BOOKMARK_OBJECT;
using syncable::IS_DEL;
using syncable::IS_DIR;
using syncable::IS_UNAPPLIED_UPDATE;
using syncable::IS_UNSYNCED;
using syncable::META_HANDLE;
using syncable::MTIME;
using syncable::NAME;
using syncable::NEXT_ID;
using syncable::PARENT_ID;
using syncable::PREV_ID;
using syncable::SERVER_IS_DEL;
using syncable::SERVER_NAME;
using syncable::SERVER_PARENT_ID;
using syncable::SERVER_POSITION_IN_PARENT;
using syncable::SERVER_VERSION;
using syncable::SINGLETON_TAG;
using syncable::UNITTEST;
using syncable::UNSANITIZED_NAME;

namespace {
const char* kTestData = "Hello World!";
const int kTestDataLen = 12;
const int64 kTestLogRequestTimestamp = 123456;
}  // namespace

class SyncerTest : public testing::Test {
 protected:
  SyncerTest() : client_command_channel_(0) {
  }

  void HandleClientCommand(const sync_pb::ClientCommand* event) {
    last_client_command_ = *event;
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
    SyncProcessState state(syncdb_.manager(), syncdb_.name(),
                           mock_server_.get(),
                           syncer->conflict_resolver(),
                           syncer->channel(),
                           syncer->model_safe_worker());
    bool should_loop = false;
    int loop_iterations = 0;
    do {
      ASSERT_LT(++loop_iterations, 100) << "infinite loop detected. please fix";
      should_loop = syncer->SyncShare(&state);
    } while (should_loop);
  }

  virtual void SetUp() {
    syncdb_.SetUp();

    mock_server_.reset(
        new MockConnectionManager(syncdb_.manager(), syncdb_.name()));
    model_safe_worker_.reset(new ModelSafeWorker());
    // Safe to pass NULL as Authwatcher for now since the code path that
    // uses it is not unittested yet.
    syncer_ = new Syncer(syncdb_.manager(), syncdb_.name(),
        mock_server_.get(),
        model_safe_worker_.get());
    CHECK(syncer_->channel());

    hookup_.reset(NewEventListenerHookup(syncer_->channel(), this,
                                         &SyncerTest::HandleSyncerEvent));

    command_channel_hookup_.reset(NewEventListenerHookup(
        &client_command_channel_, this, &SyncerTest::HandleClientCommand));
    syncer_->set_command_channel(&client_command_channel_);

    state_.reset(new SyncProcessState(syncdb_.manager(), syncdb_.name(),
                                      mock_server_.get(),
                                      syncer_->conflict_resolver(),
                                      syncer_->channel(),
                                      syncer_->model_safe_worker()));

    ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
    CHECK(dir.good());
    ReadTransaction trans(dir, __FILE__, __LINE__);
    syncable::Directory::ChildHandles children;
    dir->GetChildHandles(&trans, trans.root_id(), &children);
    ASSERT_TRUE(0 == children.size());
    syncer_events_.clear();
    root_id_ = ids_.root();
    parent_id_ = ids_.MakeServer("parent id");
    child_id_ = ids_.MakeServer("child id");
  }

  virtual void TearDown() {
    mock_server_.reset();
    hookup_.reset();
    command_channel_hookup_.reset();
    delete syncer_;
    syncdb_.TearDown();
  }
  void WriteTestDataToEntry(WriteTransaction* trans, MutableEntry* entry) {
    EXPECT_FALSE(entry->Get(IS_DIR));
    EXPECT_FALSE(entry->Get(IS_DEL));
    Blob test_value(kTestData, kTestData + kTestDataLen);
    ExtendedAttributeKey key(entry->Get(META_HANDLE), PSTR("DATA"));
    MutableExtendedAttribute attr(trans, CREATE, key);
    attr.mutable_value()->swap(test_value);
    entry->Put(syncable::IS_UNSYNCED, true);
  }
  void VerifyTestDataInEntry(BaseTransaction* trans, Entry* entry) {
    EXPECT_FALSE(entry->Get(IS_DIR));
    EXPECT_FALSE(entry->Get(IS_DEL));
    Blob test_value(kTestData, kTestData + kTestDataLen);
    ExtendedAttributeKey key(entry->Get(META_HANDLE), PSTR("DATA"));
    ExtendedAttribute attr(trans, GET_BY_HANDLE, key);
    EXPECT_FALSE(attr.is_deleted());
    EXPECT_TRUE(test_value == attr.value());
  }
  bool SyncerStuck(SyncProcessState* state) {
    SyncerStatus status(NULL, state);
    return status.syncer_stuck();
  }
  void SyncRepeatedlyToTriggerConflictResolution(SyncProcessState* state) {
    // We should trigger after less than 6 syncs, but we want to avoid brittle
    // tests.
    for (int i = 0 ; i < 6 ; ++i)
      syncer_->SyncShare(state);
  }
  void SyncRepeatedlyToTriggerStuckSignal(SyncProcessState* state) {
    // We should trigger after less than 10 syncs, but we want to avoid brittle
    // tests.
    for (int i = 0 ; i < 12 ; ++i)
      syncer_->SyncShare(state);
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
        PathString name(utf8_name.begin(), utf8_name.end());
        MutableEntry entry(&trans, CREATE, test->parent_id, name);
        entry.Put(syncable::ID, test->id);
        if (test->id.ServerKnows()) {
          entry.Put(BASE_VERSION, 5);
          entry.Put(SERVER_VERSION, 5);
          entry.Put(SERVER_PARENT_ID, test->parent_id);
        }
        entry.Put(syncable::IS_DIR, true);
        entry.Put(syncable::IS_UNSYNCED, true);
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
      SyncCycleState cycle_state;
      SyncerSession session(&cycle_state, state_.get());
      WriteTransaction wtrans(dir, UNITTEST, __FILE__, __LINE__);
      SyncerSession::ScopedSetWriteTransaction set_trans(&session, &wtrans);
      session.set_unsynced_handles(unsynced_handle_view);

      GetCommitIdsCommand command(limit);
      command.BuildCommitIds(&session);
      vector<syncable::Id> output = command.ordered_commit_set_.GetCommitIds();
      size_t truncated_size = std::min(limit, expected_id_order.size());
      ASSERT_TRUE(truncated_size == output.size());
      for (size_t i = 0; i < truncated_size; ++i) {
        ASSERT_TRUE(expected_id_order[i] == output[i])
            << "At index " << i << " with batch size limited to " << limit;
      }
    }
  }

  int64 CreateUnsyncedDirectory(const PathString& entry_name,
      const string& idstring) {
    return CreateUnsyncedDirectory(entry_name,
        syncable::Id::CreateFromServerId(idstring));
  }

  int64 CreateUnsyncedDirectory(const PathString& entry_name,
      const syncable::Id& id) {
    ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
    EXPECT_TRUE(dir.good());
    WriteTransaction wtrans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry entry(&wtrans, syncable::CREATE, wtrans.root_id(),
                       entry_name);
    EXPECT_TRUE(entry.good());
    entry.Put(syncable::IS_UNSYNCED, true);
    entry.Put(syncable::IS_DIR, true);
    entry.Put(syncable::BASE_VERSION, id.ServerKnows() ? 1 : 0);
    entry.Put(syncable::ID, id);
    return entry.Get(META_HANDLE);
  }

  // Some ids to aid tests. Only the root one's value is specific. The rest
  // are named for test clarity.
  syncable::Id root_id_;
  syncable::Id parent_id_;
  syncable::Id child_id_;

  TestIdFactory ids_;

  TestDirectorySetterUpper syncdb_;
  scoped_ptr<MockConnectionManager> mock_server_;
  scoped_ptr<EventListenerHookup> hookup_;
  scoped_ptr<EventListenerHookup> command_channel_hookup_;
  ClientCommandChannel client_command_channel_;

  Syncer* syncer_;
  scoped_ptr<SyncProcessState> state_;
  scoped_ptr<ModelSafeWorker> model_safe_worker_;
  std::set<SyncerEvent> syncer_events_;
  sync_pb::ClientCommand last_client_command_;

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
  int64 handle_c = CreateUnsyncedDirectory(PSTR("C"), ids_.MakeLocal("c"));
  int64 handle_x = CreateUnsyncedDirectory(PSTR("X"), ids_.MakeLocal("x"));
  int64 handle_b = CreateUnsyncedDirectory(PSTR("B"), ids_.MakeLocal("b"));
  int64 handle_d = CreateUnsyncedDirectory(PSTR("D"), ids_.MakeLocal("d"));
  int64 handle_e = CreateUnsyncedDirectory(PSTR("E"), ids_.MakeLocal("e"));
  {
    WriteTransaction wtrans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry entry_x(&wtrans, GET_BY_HANDLE, handle_x);
    MutableEntry entry_b(&wtrans, GET_BY_HANDLE, handle_b);
    MutableEntry entry_c(&wtrans, GET_BY_HANDLE, handle_c);
    MutableEntry entry_d(&wtrans, GET_BY_HANDLE, handle_d);
    MutableEntry entry_e(&wtrans, GET_BY_HANDLE, handle_e);
    entry_x.Put(IS_BOOKMARK_OBJECT, true);
    entry_b.Put(IS_BOOKMARK_OBJECT, true);
    entry_c.Put(IS_BOOKMARK_OBJECT, true);
    entry_d.Put(IS_BOOKMARK_OBJECT, true);
    entry_e.Put(IS_BOOKMARK_OBJECT, true);
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
  SyncCycleState cycle_state;
  SyncerSession session(&cycle_state, state_.get());
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  ASSERT_TRUE(dir.good());

  {
    WriteTransaction wtrans(dir, UNITTEST, __FILE__, __LINE__);
    SyncerSession::ScopedSetWriteTransaction set_trans(&session, &wtrans);

    GetCommitIdsCommand::OrderedCommitSet commit_set;
    GetCommitIdsCommand::CommitMetahandleIterator iterator(&session,
        &commit_set);
    EXPECT_FALSE(iterator.Valid());
    EXPECT_FALSE(iterator.Increment());
  }

  {
    vector<int64> session_metahandles;
    session_metahandles.push_back(
        CreateUnsyncedDirectory(PSTR("test1"), "testid1"));
    session_metahandles.push_back(
        CreateUnsyncedDirectory(PSTR("test2"), "testid2"));
    session_metahandles.push_back(
        CreateUnsyncedDirectory(PSTR("test3"), "testid3"));
    session.set_unsynced_handles(session_metahandles);

    WriteTransaction wtrans(dir, UNITTEST, __FILE__, __LINE__);
    SyncerSession::ScopedSetWriteTransaction set_trans(&session, &wtrans);
    GetCommitIdsCommand::OrderedCommitSet commit_set;
    GetCommitIdsCommand::CommitMetahandleIterator iterator(&session,
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
  PathString xattr_key = PSTR("key");
  {
    WriteTransaction wtrans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry parent(&wtrans, syncable::CREATE, wtrans.root_id(),
                        PSTR("Pete"));
    ASSERT_TRUE(parent.good());
    parent.Put(syncable::IS_UNSYNCED, true);
    parent.Put(syncable::IS_DIR, true);
    parent.Put(syncable::BASE_VERSION, 1);
    parent.Put(syncable::ID, parent_id_);
    MutableEntry child(&wtrans, syncable::CREATE, parent_id_, PSTR("Pete"));
    ASSERT_TRUE(child.good());
    child.Put(syncable::ID, child_id_);
    child.Put(syncable::BASE_VERSION, 1);
    WriteTestDataToEntry(&wtrans, &child);
  }

  SyncCycleState cycle_state;
  SyncerSession session(&cycle_state, state_.get());

  syncer_->SyncShare(&session);
  EXPECT_TRUE(2 == session.unsynced_count());
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
                          PSTR("Bob"));
      ASSERT_TRUE(parent.good());
      parent.Put(syncable::IS_UNSYNCED, true);
      parent.Put(syncable::IS_DIR, true);
      parent.Put(syncable::ID, ids_.FromNumber(100));
      parent.Put(syncable::BASE_VERSION, 1);
      MutableEntry child(&wtrans, syncable::CREATE, ids_.FromNumber(100),
                         PSTR("Bob"));
      ASSERT_TRUE(child.good());
      child.Put(syncable::IS_UNSYNCED, true);
      child.Put(syncable::IS_DIR, true);
      child.Put(syncable::ID, ids_.FromNumber(101));
      child.Put(syncable::BASE_VERSION, 1);
      MutableEntry grandchild(&wtrans, syncable::CREATE, ids_.FromNumber(101),
                              PSTR("Bob"));
      ASSERT_TRUE(grandchild.good());
      grandchild.Put(syncable::ID, ids_.FromNumber(102));
      grandchild.Put(syncable::IS_UNSYNCED, true);
      grandchild.Put(syncable::BASE_VERSION, 1);
    }
    {
      // Create three deleted items which deletions we expect to be sent to the
      // server.
      MutableEntry parent(&wtrans, syncable::CREATE, wtrans.root_id(),
                          PSTR("Pete"));
      ASSERT_TRUE(parent.good());
      parent.Put(syncable::IS_UNSYNCED, true);
      parent.Put(syncable::IS_DIR, true);
      parent.Put(syncable::IS_DEL, true);
      parent.Put(syncable::ID, ids_.FromNumber(103));
      parent.Put(syncable::BASE_VERSION, 1);
      parent.Put(syncable::MTIME, now_minus_2h);
      MutableEntry child(&wtrans, syncable::CREATE, ids_.FromNumber(103),
                         PSTR("Pete"));
      ASSERT_TRUE(child.good());
      child.Put(syncable::IS_UNSYNCED, true);
      child.Put(syncable::IS_DIR, true);
      child.Put(syncable::IS_DEL, true);
      child.Put(syncable::ID, ids_.FromNumber(104));
      child.Put(syncable::BASE_VERSION, 1);
      child.Put(syncable::MTIME, now_minus_2h);
      MutableEntry grandchild(&wtrans, syncable::CREATE, ids_.FromNumber(104),
                              PSTR("Pete"));
      ASSERT_TRUE(grandchild.good());
      grandchild.Put(syncable::IS_UNSYNCED, true);
      grandchild.Put(syncable::ID, ids_.FromNumber(105));
      grandchild.Put(syncable::IS_DEL, true);
      grandchild.Put(syncable::IS_DIR, false);
      grandchild.Put(syncable::BASE_VERSION, 1);
      grandchild.Put(syncable::MTIME, now_minus_2h);
    }
  }

  SyncCycleState cycle_state;
  SyncerSession session(&cycle_state, state_.get());
  syncer_->SyncShare(&session);
  EXPECT_TRUE(6 == session.unsynced_count());
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
    MutableEntry parent(&wtrans, syncable::CREATE, wtrans.root_id(), PSTR("1"));
    ASSERT_TRUE(parent.good());
    parent.Put(syncable::IS_UNSYNCED, true);
    parent.Put(syncable::IS_DIR, true);
    parent.Put(syncable::ID, parent_id_);
    MutableEntry child(&wtrans, syncable::CREATE, wtrans.root_id(), PSTR("2"));
    ASSERT_TRUE(child.good());
    child.Put(syncable::IS_UNSYNCED, true);
    child.Put(syncable::IS_DIR, true);
    child.Put(syncable::ID, child_id_);
    parent.Put(syncable::BASE_VERSION, 1);
    child.Put(syncable::BASE_VERSION, 1);
  }
  {
    WriteTransaction wtrans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry parent(&wtrans, syncable::CREATE, parent_id_, PSTR("A"));
    ASSERT_TRUE(parent.good());
    parent.Put(syncable::IS_UNSYNCED, true);
    parent.Put(syncable::IS_DIR, true);
    parent.Put(syncable::ID, ids_.FromNumber(102));
    MutableEntry child(&wtrans, syncable::CREATE, parent_id_, PSTR("B"));
    ASSERT_TRUE(child.good());
    child.Put(syncable::IS_UNSYNCED, true);
    child.Put(syncable::IS_DIR, true);
    child.Put(syncable::ID, ids_.FromNumber(-103));
    parent.Put(syncable::BASE_VERSION, 1);
  }
  {
    WriteTransaction wtrans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry parent(&wtrans, syncable::CREATE, child_id_, PSTR("A"));
    ASSERT_TRUE(parent.good());
    parent.Put(syncable::IS_UNSYNCED, true);
    parent.Put(syncable::IS_DIR, true);
    parent.Put(syncable::ID, ids_.FromNumber(-104));
    MutableEntry child(&wtrans, syncable::CREATE, child_id_, PSTR("B"));
    ASSERT_TRUE(child.good());
    child.Put(syncable::IS_UNSYNCED, true);
    child.Put(syncable::IS_DIR, true);
    child.Put(syncable::ID, ids_.FromNumber(105));
    child.Put(syncable::BASE_VERSION, 1);
  }

  SyncCycleState cycle_state;
  SyncerSession session(&cycle_state, state_.get());
  syncer_->SyncShare(&session);
  EXPECT_TRUE(6 == session.unsynced_count());
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
    MutableEntry parent(&wtrans, syncable::CREATE, wtrans.root_id(), PSTR("P"));
    ASSERT_TRUE(parent.good());
    parent.Put(syncable::IS_UNSYNCED, true);
    parent.Put(syncable::IS_DIR, true);
    parent.Put(syncable::ID, parent_id_);
    MutableEntry child1(&wtrans, syncable::CREATE, parent_id_, PSTR("1"));
    ASSERT_TRUE(child1.good());
    child1.Put(syncable::IS_UNSYNCED, true);
    child1.Put(syncable::ID, child_id_);
    MutableEntry child2(&wtrans, syncable::CREATE, parent_id_, PSTR("2"));
    ASSERT_TRUE(child2.good());
    child2.Put(syncable::IS_UNSYNCED, true);
    child2.Put(syncable::ID, child2_id);
    parent.Put(syncable::BASE_VERSION, 1);
    child1.Put(syncable::BASE_VERSION, 1);
    child2.Put(syncable::BASE_VERSION, 1);
  }

  SyncCycleState cycle_state;
  SyncerSession session(&cycle_state, state_.get());
  syncer_->SyncShare(&session);
  EXPECT_TRUE(3 == session.unsynced_count());
  ASSERT_TRUE(3 == mock_server_->committed_ids().size());
  // If this test starts failing, be aware other sort orders could be valid.
  EXPECT_TRUE(parent_id_ == mock_server_->committed_ids()[0]);
  EXPECT_TRUE(child_id_ == mock_server_->committed_ids()[1]);
  EXPECT_TRUE(child2_id == mock_server_->committed_ids()[2]);
}

TEST_F(SyncerTest, TestCommitListOrderingAndNewParent) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  ASSERT_TRUE(dir.good());
  {
    WriteTransaction wtrans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry parent(&wtrans, syncable::CREATE, wtrans.root_id(), PSTR("1"));
    ASSERT_TRUE(parent.good());
    parent.Put(syncable::IS_UNSYNCED, true);
    parent.Put(syncable::IS_DIR, true);
    parent.Put(syncable::ID, parent_id_);
    parent.Put(syncable::BASE_VERSION, 1);
  }

  syncable::Id parent2_id = ids_.NewLocalId();
  syncable::Id child2_id = ids_.NewServerId();
  {
    WriteTransaction wtrans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry parent(&wtrans, syncable::CREATE, parent_id_, PSTR("A"));
    ASSERT_TRUE(parent.good());
    parent.Put(syncable::IS_UNSYNCED, true);
    parent.Put(syncable::IS_DIR, true);
    parent.Put(syncable::ID, parent2_id);
    MutableEntry child(&wtrans, syncable::CREATE, parent2_id, PSTR("B"));
    ASSERT_TRUE(child.good());
    child.Put(syncable::IS_UNSYNCED, true);
    child.Put(syncable::IS_DIR, true);
    child.Put(syncable::ID, child2_id);
    child.Put(syncable::BASE_VERSION, 1);
  }

  SyncCycleState cycle_state;
  SyncerSession session(&cycle_state, state_.get());

  syncer_->SyncShare(&session);
  EXPECT_TRUE(3 == session.unsynced_count());
  ASSERT_TRUE(3 == mock_server_->committed_ids().size());
  // If this test starts failing, be aware other sort orders could be valid.
  EXPECT_TRUE(parent_id_ == mock_server_->committed_ids()[0]);
  EXPECT_TRUE(parent2_id == mock_server_->committed_ids()[1]);
  EXPECT_TRUE(child2_id == mock_server_->committed_ids()[2]);
  {
    ReadTransaction rtrans(dir, __FILE__, __LINE__);
    PathChar path[] = { '1', *kPathSeparator, 'A', 0};
    Entry entry_1A(&rtrans, syncable::GET_BY_PATH, path);
    ASSERT_TRUE(entry_1A.good());
    Entry item_parent2(&rtrans, syncable::GET_BY_ID, parent2_id);
    ASSERT_FALSE(item_parent2.good());
    Entry item_child2(&rtrans, syncable::GET_BY_ID, child2_id);
    EXPECT_EQ(entry_1A.Get(syncable::ID), item_child2.Get(syncable::PARENT_ID));
    EXPECT_TRUE(entry_1A.Get(syncable::ID).ServerKnows());
  }
}

TEST_F(SyncerTest, TestCommitListOrderingAndNewParentAndChild) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  ASSERT_TRUE(dir.good());
  {
    WriteTransaction wtrans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry parent(&wtrans, syncable::CREATE, wtrans.root_id(), PSTR("1"));
    ASSERT_TRUE(parent.good());
    parent.Put(syncable::IS_UNSYNCED, true);
    parent.Put(syncable::IS_DIR, true);
    parent.Put(syncable::ID, parent_id_);
    parent.Put(syncable::BASE_VERSION, 1);
  }
  int64 meta_handle_a, meta_handle_b;
  {
    WriteTransaction wtrans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry parent(&wtrans, syncable::CREATE, parent_id_, PSTR("A"));
    ASSERT_TRUE(parent.good());
    parent.Put(syncable::IS_UNSYNCED, true);
    parent.Put(syncable::IS_DIR, true);
    parent.Put(syncable::ID, ids_.FromNumber(-101));
    meta_handle_a = parent.Get(syncable::META_HANDLE);
    MutableEntry child(&wtrans, syncable::CREATE, ids_.FromNumber(-101),
        PSTR("B"));
    ASSERT_TRUE(child.good());
    child.Put(syncable::IS_UNSYNCED, true);
    child.Put(syncable::IS_DIR, true);
    child.Put(syncable::ID, ids_.FromNumber(-102));
    meta_handle_b = child.Get(syncable::META_HANDLE);
  }

  SyncCycleState cycle_state;
  SyncerSession session(&cycle_state, state_.get());

  syncer_->SyncShare(&session);
  EXPECT_TRUE(3 == session.unsynced_count());
  ASSERT_TRUE(3 == mock_server_->committed_ids().size());
  // If this test starts failing, be aware other sort orders could be valid.
  EXPECT_TRUE(parent_id_ == mock_server_->committed_ids()[0]);
  EXPECT_TRUE(ids_.FromNumber(-101) == mock_server_->committed_ids()[1]);
  EXPECT_TRUE(ids_.FromNumber(-102) == mock_server_->committed_ids()[2]);
  {
    ReadTransaction rtrans(dir, __FILE__, __LINE__);
    PathChar path[] = { '1', *kPathSeparator, 'A', 0};
    Entry entry_1A(&rtrans, syncable::GET_BY_PATH, path);
    ASSERT_TRUE(entry_1A.good());
    Entry entry_id_minus_101(&rtrans, syncable::GET_BY_ID,
                            ids_.FromNumber(-101));
    ASSERT_FALSE(entry_id_minus_101.good());
    Entry entry_b(&rtrans, syncable::GET_BY_HANDLE, meta_handle_b);
    EXPECT_TRUE(entry_1A.Get(syncable::ID) == entry_b.Get(syncable::PARENT_ID));
    EXPECT_TRUE(entry_1A.Get(syncable::ID).ServerKnows());
  }
}

TEST_F(SyncerTest, UpdateWithZeroLengthName) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  ASSERT_TRUE(dir.good());
  // One illegal update
  mock_server_->AddUpdateDirectory(1, 0, "", 1, 10);
  // And one legal one that we're going to delete.
  mock_server_->AddUpdateDirectory(2, 0, "FOO", 1, 10);
  syncer_->SyncShare();
  // Delete the legal one. The new update has a null name.
  mock_server_->AddUpdateDirectory(2, 0, "", 2, 20);
  mock_server_->SetLastUpdateDeleted();
  syncer_->SyncShare();
}

#if defined(OS_WIN)
TEST_F(SyncerTest, NameSanitizationWithClientRename) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  ASSERT_TRUE(dir.good());
  mock_server_->AddUpdateDirectory(1, 0, "okay", 1, 10);
  syncer_->SyncShare();
  {
    ReadTransaction tr(dir, __FILE__, __LINE__);
    Entry e(&tr, syncable::GET_BY_PARENTID_AND_NAME, tr.root_id(),
            PSTR("okay"));
    ASSERT_TRUE(e.good());
  }
  mock_server_->AddUpdateDirectory(2, 0, "prn", 1, 20);
  syncer_->SyncShare();
  {
    WriteTransaction tr(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry e(&tr, syncable::GET_BY_PARENTID_AND_NAME, tr.root_id(),
                   PSTR("prn~1"));
    ASSERT_TRUE(e.good());
    e.PutName(syncable::Name(PSTR("printer")));
    e.Put(syncable::IS_UNSYNCED, true);
  }
  syncer_->SyncShare();
  {
    vector<CommitMessage*>::const_reverse_iterator it =
        mock_server_->commit_messages().rbegin();
    ASSERT_TRUE(mock_server_->commit_messages().rend() != it);
    const sync_pb::SyncEntity *const *s = (*it)->entries().data();
    int s_len = (*it)->entries_size();
    ASSERT_TRUE(1 == s_len);
    ASSERT_TRUE("printer" == (*s)[0].name());
  }
}

TEST_F(SyncerTest, NameSanitizationWithCascade) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  ASSERT_TRUE(dir.good());
  mock_server_->AddUpdateDirectory(1, 0, "prn~1", 1, 10);
  syncer_->SyncShare();
  {
    ReadTransaction tr(dir, __FILE__, __LINE__);
    Entry e(&tr, syncable::GET_BY_PARENTID_AND_NAME, tr.root_id(),
            PSTR("prn~1"));
    ASSERT_TRUE(e.good());
  }
  mock_server_->AddUpdateDirectory(2, 0, "prn", 1, 20);
  syncer_->SyncShare();
  {
    ReadTransaction tr(dir, __FILE__, __LINE__);
    Entry e(&tr, syncable::GET_BY_PARENTID_AND_NAME, tr.root_id(),
            PSTR("prn~2"));
    ASSERT_TRUE(e.good());
  }
  mock_server_->AddUpdateDirectory(3, 0, "prn~2", 1, 30);
  syncer_->SyncShare();
  {
    ReadTransaction tr(dir, __FILE__, __LINE__);
    Entry e(&tr, syncable::GET_BY_PARENTID_AND_NAME, tr.root_id(),
            PSTR("prn~3"));
    ASSERT_TRUE(e.good());
  }
}

TEST_F(SyncerTest, GetStuckWithConflictingSanitizedNames) {
  // We should get stuck here because we get two server updates with exactly the
  // same name.
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  ASSERT_TRUE(dir.good());
  mock_server_->AddUpdateDirectory(1, 0, "foo:", 1, 10);
  syncer_->SyncShare();
  mock_server_->AddUpdateDirectory(2, 0, "foo:", 1, 20);
  SyncRepeatedlyToTriggerStuckSignal(state_.get());
  EXPECT_TRUE(SyncerStuck(state_.get()));
  syncer_events_.clear();
}

TEST_F(SyncerTest, MergeFolderWithSanitizedNameMatches) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());
  {
    WriteTransaction wtrans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry parent(&wtrans, CREATE, wtrans.root_id(), PSTR("Folder"));
    ASSERT_TRUE(parent.good());
    parent.Put(IS_DIR, true);
    parent.Put(IS_UNSYNCED, true);
    parent.Put(UNSANITIZED_NAME, PSTR("Folder:"));
  }
  mock_server_->AddUpdateDirectory(100, 0, "Folder:", 10, 10);
  syncer_->SyncShare();
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    Directory::ChildHandles children;
    dir->GetChildHandles(&trans, trans.root_id(), &children);
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

// These two tests are the same as the two above, but they introduce case
// changes.
TEST_F(SyncerTest, GetStuckWithSanitizedNamesThatDifferOnlyByCase) {
  // We should get stuck here because we get two server updates with exactly the
  // same name.
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  ASSERT_TRUE(dir.good());
  mock_server_->AddUpdateDirectory(1, 0, "FOO:", 1, 10);
  syncer_->SyncShare();
  mock_server_->AddUpdateDirectory(2, 0, "foo:", 1, 20);
  SyncRepeatedlyToTriggerStuckSignal(state_.get());
  EXPECT_TRUE(SyncerStuck(state_.get()));
  syncer_events_.clear();
}

TEST_F(SyncerTest, MergeFolderWithSanitizedNameThatDiffersOnlyByCase) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());
  {
    WriteTransaction wtrans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry parent(&wtrans, CREATE, wtrans.root_id(), PSTR("FOLDER"));
    ASSERT_TRUE(parent.good());
    parent.Put(IS_DIR, true);
    parent.Put(IS_UNSYNCED, true);
    parent.Put(UNSANITIZED_NAME, PSTR("FOLDER:"));
  }
  mock_server_->AddUpdateDirectory(100, 0, "Folder:", 10, 10);
  mock_server_->set_conflict_all_commits(true);
  syncer_->SyncShare();
  syncer_->SyncShare();
  syncer_->SyncShare();  // Good gracious, these tests are not so good.
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    Directory::ChildHandles children;
    dir->GetChildHandles(&trans, trans.root_id(), &children);
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
#else  // Mac / Linux ...

TEST_F(SyncerTest, NameSanitizationWithClientRename) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  ASSERT_TRUE(dir.good());
  mock_server_->AddUpdateDirectory(1, 0, "okay", 1, 10);
  syncer_->SyncShare();
  {
    ReadTransaction tr(dir, __FILE__, __LINE__);
    Entry e(&tr, syncable::GET_BY_PARENTID_AND_NAME, tr.root_id(),
            PSTR("okay"));
    ASSERT_TRUE(e.good());
  }
  mock_server_->AddUpdateDirectory(2, 0, "a/b", 1, 20);
  syncer_->SyncShare();
  {
    WriteTransaction tr(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry e(&tr, syncable::GET_BY_PARENTID_AND_NAME, tr.root_id(),
                   PSTR("a:b"));
    ASSERT_TRUE(e.good());
    e.PutName(syncable::Name(PSTR("ab")));
    e.Put(syncable::IS_UNSYNCED, true);
  }
  syncer_->SyncShare();
  {
    vector<CommitMessage*>::const_reverse_iterator it =
        mock_server_->commit_messages().rbegin();
    ASSERT_TRUE(mock_server_->commit_messages().rend() != it);
    const sync_pb::SyncEntity *const *s = (*it)->entries().data();
    int s_len = (*it)->entries_size();
    ASSERT_TRUE(1 == s_len);
    ASSERT_TRUE("ab" == (*s)[0].name());
  }
}
#endif

namespace {
void VerifyExistsWithNameInRoot(syncable::Directory* dir,
                                const PathString& name,
                                const string& entry,
                                int line) {
  ReadTransaction tr(dir, __FILE__, __LINE__);
  Entry e(&tr, syncable::GET_BY_PARENTID_AND_NAME, tr.root_id(),
          name);
  EXPECT_TRUE(e.good()) << "failed on call from " << entry << ":" << line;
}
}  // namespace

TEST_F(SyncerTest, ExtendedAttributeWithNullCharacter) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  ASSERT_TRUE(dir.good());
  size_t xattr_count = 2;
  PathString xattr_keys[] = { PSTR("key"), PSTR("key2") };
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

  syncer_->SyncShare();
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

  syncer_->SyncShare(state_.get());
  SyncerStatus status(NULL, state_.get());
  EXPECT_TRUE(0 == status.stalled_updates());
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
  Id root = ids_.root();
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  ASSERT_TRUE(dir.good());
  // Should apply just fine.
  mock_server_->AddUpdateDirectory(1, 0, "in_root", 10, 10);

  // Name clash: this is a conflict.
  mock_server_->AddUpdateDirectory(2, 0, "in_root", 10, 10);

  // Unknown parent: should never be applied. "-80" is a legal server ID,
  // because any string sent by the server is a legal server ID in the sync
  // protocol, but it's not the ID of any item known to the client.  This
  // update should succeed validation, but be stuck in the unapplied state
  // until an item with the server ID "-80" arrives.
  mock_server_->AddUpdateDirectory(3, -80, "bad_parent", 10, 10);

  syncer_->SyncShare(state_.get());

  ConflictResolutionView conflict_view(state_.get());
  SyncerStatus status(NULL, state_.get());
  // Ids 2 and 3 are expected to be in conflict now.
  EXPECT_TRUE(2 == conflict_view.conflicting_updates());
  EXPECT_TRUE(0 == status.stalled_updates());

  // These entries will be used in the second set of updates.
  mock_server_->AddUpdateDirectory(4, 0, "newer_version", 20, 10);
  mock_server_->AddUpdateDirectory(5, 0, "circular1", 10, 10);
  mock_server_->AddUpdateDirectory(6, 5, "circular2", 10, 10);
  mock_server_->AddUpdateDirectory(9, 3, "bad_parent_child", 10, 10);
  mock_server_->AddUpdateDirectory(100, 9, "bad_parent_child2", 10, 10);
  mock_server_->AddUpdateDirectory(10, 0, "dir_to_bookmark", 10, 10);

  syncer_->SyncShare(state_.get());
  // The three items with an unresolved parent should be unapplied (3, 9, 100).
  // The name clash should also still be in conflict.
  EXPECT_TRUE(4 == conflict_view.conflicting_updates());
  EXPECT_TRUE(0 == status.stalled_updates());
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    Entry name_clash(&trans, GET_BY_ID, ids_.FromNumber(2));
    ASSERT_TRUE(name_clash.good());
    EXPECT_TRUE(name_clash.Get(IS_UNAPPLIED_UPDATE));

    Entry bad_parent(&trans, GET_BY_ID, ids_.FromNumber(3));
    ASSERT_TRUE(bad_parent.good());
    EXPECT_TRUE(name_clash.Get(IS_UNAPPLIED_UPDATE))
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

  // Updating 1 should unblock the clashing item 2.
  mock_server_->AddUpdateDirectory(1, 0, "new_name", 20, 20);

  // Moving 5 under 6 will create a cycle: a conflict.
  mock_server_->AddUpdateDirectory(5, 6, "circular3", 20, 20);

  // Flip the is_dir bit: should fail verify & be dropped.
  mock_server_->AddUpdateBookmark(10, 0, "dir_to_bookmark", 20, 20);
  syncer_->SyncShare(state_.get());

  // Version number older than last known: should fail verify & be dropped.
  mock_server_->AddUpdateDirectory(4, 0, "old_version", 10, 10);
  syncer_->SyncShare(state_.get());
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    Entry still_a_dir(&trans, GET_BY_ID, ids_.FromNumber(10));
    ASSERT_TRUE(still_a_dir.good());
    EXPECT_FALSE(still_a_dir.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_TRUE(10 == still_a_dir.Get(BASE_VERSION));
    EXPECT_TRUE(10 == still_a_dir.Get(SERVER_VERSION));
    EXPECT_TRUE(still_a_dir.Get(IS_DIR));

    Entry rename(&trans, GET_BY_PARENTID_AND_NAME, root, PSTR("new_name"));
    ASSERT_TRUE(rename.good());
    EXPECT_FALSE(rename.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_TRUE(ids_.FromNumber(1) == rename.Get(ID));
    EXPECT_TRUE(20 == rename.Get(BASE_VERSION));

    Entry unblocked(&trans, GET_BY_PARENTID_AND_NAME, root, PSTR("in_root"));
    ASSERT_TRUE(unblocked.good());
    EXPECT_FALSE(unblocked.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_TRUE(ids_.FromNumber(2) == unblocked.Get(ID));
    EXPECT_TRUE(10 == unblocked.Get(BASE_VERSION));

    Entry ignored_old_version(&trans, GET_BY_ID, ids_.FromNumber(4));
    ASSERT_TRUE(ignored_old_version.good());
    EXPECT_TRUE(ignored_old_version.Get(NAME) == PSTR("newer_version"));
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
  EXPECT_TRUE(4 == conflict_view.conflicting_updates());
}

TEST_F(SyncerTest, CommitTimeRename) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  ASSERT_TRUE(dir.good());
  // Create a folder and an entry.
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry parent(&trans, CREATE, root_id_, PSTR("Folder"));
    ASSERT_TRUE(parent.good());
    parent.Put(IS_DIR, true);
    parent.Put(IS_UNSYNCED, true);
    MutableEntry entry(&trans, CREATE, parent.Get(ID), PSTR("new_entry"));
    ASSERT_TRUE(entry.good());
    WriteTestDataToEntry(&trans, &entry);
  }

  // Mix in a directory creation too for later.
  mock_server_->AddUpdateDirectory(2, 0, "dir_in_root", 10, 10);
  mock_server_->SetCommitTimeRename("renamed_");
  syncer_->SyncShare();

  // Verify it was correctly renamed.
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    Entry entry_folder(&trans, GET_BY_PATH, PSTR("renamed_Folder"));
    ASSERT_TRUE(entry_folder.good());

    Entry entry_new(&trans, GET_BY_PATH,
        PSTR("renamed_Folder") + PathString(kPathSeparator)
            + PSTR("renamed_new_entry"));
    ASSERT_TRUE(entry_new.good());

    // And that the unrelated directory creation worked without a rename.
    Entry new_dir(&trans, GET_BY_PATH, PSTR("dir_in_root"));
    EXPECT_TRUE(new_dir.good());
  }
}


TEST_F(SyncerTest, CommitTimeRenameI18N) {
  // This is utf-8 for the diacritized Internationalization.
  const char* i18nString = "\xc3\x8e\xc3\xb1\x74\xc3\xa9\x72\xc3\xb1"
      "\xc3\xa5\x74\xc3\xae\xc3\xb6\xc3\xb1\xc3\xa5\x6c\xc3\xae"
      "\xc2\x9e\xc3\xa5\x74\xc3\xae\xc3\xb6\xc3\xb1";

  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  ASSERT_TRUE(dir.good());
  // Create a folder and entry.
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry parent(&trans, CREATE, root_id_, PSTR("Folder"));
    ASSERT_TRUE(parent.good());
    parent.Put(IS_DIR, true);
    parent.Put(IS_UNSYNCED, true);
    MutableEntry entry(&trans, CREATE, parent.Get(ID), PSTR("new_entry"));
    ASSERT_TRUE(entry.good());
    WriteTestDataToEntry(&trans, &entry);
  }

  // Mix in a directory creation too for later.
  mock_server_->AddUpdateDirectory(2, 0, "dir_in_root", 10, 10);
  mock_server_->SetCommitTimeRename(i18nString);
  syncer_->SyncShare();

  // Verify it was correctly renamed.
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    PathString expectedFolder;
    AppendUTF8ToPathString(i18nString, &expectedFolder);
    AppendUTF8ToPathString("Folder", &expectedFolder);
    Entry entry_folder(&trans, GET_BY_PATH, expectedFolder);
    ASSERT_TRUE(entry_folder.good());
    PathString expected = expectedFolder + PathString(kPathSeparator);
    AppendUTF8ToPathString(i18nString, &expected);
    AppendUTF8ToPathString("new_entry", &expected);

    Entry entry_new(&trans, GET_BY_PATH, expected);
    ASSERT_TRUE(entry_new.good());

    // And that the unrelated directory creation worked without a rename.
    Entry new_dir(&trans, GET_BY_PATH, PSTR("dir_in_root"));
    EXPECT_TRUE(new_dir.good());
  }
}

TEST_F(SyncerTest, CommitTimeRenameCollision) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  ASSERT_TRUE(dir.good());
  // Create a folder to collide with.
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry collider(&trans, CREATE, root_id_, PSTR("renamed_Folder"));
    ASSERT_TRUE(collider.good());
    collider.Put(IS_DIR, true);
    collider.Put(IS_UNSYNCED, true);
  }
  syncer_->SyncShare();  // Now we have a folder.

  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry folder(&trans, CREATE, root_id_, PSTR("Folder"));
    ASSERT_TRUE(folder.good());
    folder.Put(IS_DIR, true);
    folder.Put(IS_UNSYNCED, true);
  }

  mock_server_->set_next_new_id(30000);
  mock_server_->SetCommitTimeRename("renamed_");
  syncer_->SyncShare();  // Should collide and rename aside.
  // This case will only occur if we got a commit time rename aside
  // and the server attempts to rename to an entry that we know about, but it
  // does not.

  // Verify it was correctly renamed; one of them should have a sanitized name.
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    Entry collider_folder(&trans, GET_BY_PARENTID_AND_NAME, root_id_,
        PSTR("renamed_Folder"));
    EXPECT_TRUE(collider_folder.Get(UNSANITIZED_NAME) == PSTR(""));
    ASSERT_TRUE(collider_folder.good());

    // ID is generated by next_new_id_ and server mock prepending of strings.
    Entry entry_folder(&trans, GET_BY_ID,
        syncable::Id::CreateFromServerId("mock_server:30000"));
    ASSERT_TRUE(entry_folder.good());
    // A little arbitrary but nothing we can do about that.
    EXPECT_TRUE(entry_folder.Get(NAME) == PSTR("renamed_Folder~1"));
    EXPECT_TRUE(entry_folder.Get(UNSANITIZED_NAME) == PSTR("renamed_Folder"));
  }
}


// A commit with a lost response produces an update that has to be reunited with
// its parent.
TEST_F(SyncerTest, CommitReuniteUpdateAdjustsChildren) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  ASSERT_TRUE(dir.good());
  // Create a folder in the root.
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry entry(&trans, CREATE, trans.root_id(), PSTR("new_folder"));
    ASSERT_TRUE(entry.good());
    entry.Put(IS_DIR, true);
    entry.Put(IS_UNSYNCED, true);
  }

  // Verify it and pull the ID out of the folder.
  syncable::Id folder_id;
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    Entry entry(&trans, GET_BY_PATH, PSTR("new_folder"));
    ASSERT_TRUE(entry.good());
    folder_id = entry.Get(ID);
    ASSERT_TRUE(!folder_id.ServerKnows());
  }

  // Create an entry in the newly created folder.
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry entry(&trans, CREATE, folder_id, PSTR("new_entry"));
    ASSERT_TRUE(entry.good());
    WriteTestDataToEntry(&trans, &entry);
  }

  // Verify it and pull the ID out of the entry.
  syncable::Id entry_id;
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    Entry entry(&trans, syncable::GET_BY_PARENTID_AND_NAME, folder_id,
                PSTR("new_entry"));
    ASSERT_TRUE(entry.good());
    entry_id = entry.Get(ID);
    EXPECT_TRUE(!entry_id.ServerKnows());
    VerifyTestDataInEntry(&trans, &entry);
  }

  // Now, to emulate a commit response failure, we just don't commit it.
  int64 new_version = 150;  // any larger value.
  int64 timestamp = 20;  // arbitrary value.
  syncable::Id new_folder_id =
      syncable::Id::CreateFromServerId("folder_server_id");

  // the following update should cause the folder to both apply the update, as
  // well as reassociate the id.
  mock_server_->AddUpdateDirectory(new_folder_id, root_id_,
      "new_folder", new_version, timestamp);
  mock_server_->SetLastUpdateOriginatorFields(
      dir->cache_guid(), folder_id.GetServerId());

  // We don't want it accidentally committed, just the update applied.
  mock_server_->set_conflict_all_commits(true);

  // Alright! Apply that update!
  syncer_->SyncShare();
  {
    // The folder's ID should have been updated.
    ReadTransaction trans(dir, __FILE__, __LINE__);
    Entry folder(&trans, GET_BY_PATH, PSTR("new_folder"));
    ASSERT_TRUE(folder.good());
    EXPECT_TRUE(new_version == folder.Get(BASE_VERSION));
    EXPECT_TRUE(new_folder_id == folder.Get(ID));
    EXPECT_TRUE(folder.Get(ID).ServerKnows());

    // We changed the id of the parent, old lookups should fail.
    Entry bad_entry(&trans, syncable::GET_BY_PARENTID_AND_NAME, folder_id,
                   PSTR("new_entry"));
    EXPECT_FALSE(bad_entry.good());

    // The child's parent should have changed as well.
    Entry entry(&trans, syncable::GET_BY_PARENTID_AND_NAME, new_folder_id,
               PSTR("new_entry"));
    ASSERT_TRUE(entry.good());
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
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry entry(&trans, CREATE, trans.root_id(), PSTR("new_entry"));
    ASSERT_TRUE(entry.good());
    WriteTestDataToEntry(&trans, &entry);
  }
  // Verify it and pull the ID out.
  syncable::Id entry_id;
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    Entry entry(&trans, GET_BY_PATH, PSTR("new_entry"));
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
  syncer_->SyncShare();
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    Entry entry(&trans, GET_BY_PATH, PSTR("new_entry"));
    ASSERT_TRUE(entry.good());
    EXPECT_TRUE(new_version == entry.Get(BASE_VERSION));
    EXPECT_TRUE(new_entry_id == entry.Get(ID));
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
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry entry(&trans, CREATE, trans.root_id(), PSTR("new_entry"));
    ASSERT_TRUE(entry.good());
    WriteTestDataToEntry(&trans, &entry);
  }
  // Verify it and pull the ID out.
  syncable::Id entry_id;
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    Entry entry(&trans, GET_BY_PATH, PSTR("new_entry"));
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
    MutableEntry entry(&trans, GET_BY_PATH, PSTR("new_entry"));
    ASSERT_TRUE(entry.good());
    entry.Put(syncable::IS_DEL, true);
  }

  // Just don't CHECK fail in sync, have the update split.
  syncer_->SyncShare();
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    Entry entry(&trans, GET_BY_PATH, PSTR("new_entry"));
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
  syncer_->SyncShare();
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
  syncer_->SyncShare();
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
  Entry child(&trans, syncable::GET_BY_PARENTID_AND_NAME, ids_.FromNumber(4),
              PSTR("gggchild"));
  ASSERT_TRUE(child.good());
}

bool CreateFolderInBob(Directory* dir) {
  WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
  MutableEntry bob(&trans, syncable::GET_BY_PARENTID_AND_NAME, trans.root_id(),
                   PSTR("bob"));
  MutableEntry entry2(&trans, syncable::CREATE, bob.Get(syncable::ID),
                      PSTR("bob"));
  CHECK(entry2.good());
  entry2.Put(syncable::IS_DIR, true);
  entry2.Put(syncable::IS_UNSYNCED, true);
  return true;
}

TEST_F(SyncerTest, EntryCreatedInNewFolderMidSync) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry entry(&trans, syncable::CREATE, trans.root_id(), PSTR("bob"));
    ASSERT_TRUE(entry.good());
    entry.Put(syncable::IS_DIR, true);
    entry.Put(syncable::IS_UNSYNCED, true);
  }
  mock_server_->SetMidCommitCallbackFunction(CreateFolderInBob);
  syncer_->SyncShare(BUILD_COMMIT_REQUEST, SYNCER_END);
  EXPECT_TRUE(1 == mock_server_->committed_ids().size());
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    PathChar path[] = {*kPathSeparator, 'b', 'o', 'b', 0};
    Entry entry(&trans, syncable::GET_BY_PATH, path);
    ASSERT_TRUE(entry.good());
    PathChar path2[] = {*kPathSeparator, 'b', 'o', 'b',
                        *kPathSeparator, 'b', 'o', 'b', 0};
    Entry entry2(&trans, syncable::GET_BY_PATH, path2);
    ASSERT_TRUE(entry2.good());
  }
}

bool TouchFredAndGingerInRoot(Directory* dir) {
  WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
  MutableEntry fred(&trans, syncable::GET_BY_PARENTID_AND_NAME, trans.root_id(),
                    PSTR("fred"));
  CHECK(fred.good());
  // Equivalent to touching the entry.
  fred.Put(syncable::IS_UNSYNCED, true);
  fred.Put(syncable::SYNCING, false);
  MutableEntry ginger(&trans, syncable::GET_BY_PARENTID_AND_NAME,
                      trans.root_id(), PSTR("ginger"));
  CHECK(ginger.good());
  ginger.Put(syncable::IS_UNSYNCED, true);
  ginger.Put(syncable::SYNCING, false);
  return true;
}

TEST_F(SyncerTest, NegativeIDInUpdate) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());
  mock_server_->AddUpdateBookmark(-10, 0, "bad", 40, 40);
  syncer_->SyncShare();
  // The negative id would make us CHECK!
}

TEST_F(SyncerTest, UnappliedUpdateOnCreatedItemItemDoesNotCrash) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());
  {
    // Create an item.
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry fred_match(&trans, CREATE, trans.root_id(),
                            PSTR("fred_match"));
    ASSERT_TRUE(fred_match.good());
    WriteTestDataToEntry(&trans, &fred_match);
  }
  // Commit it.
  syncer_->SyncShare();
  EXPECT_TRUE(1 == mock_server_->committed_ids().size());
  mock_server_->set_conflict_all_commits(true);
  syncable::Id fred_match_id;
  {
    // Now receive a change from outside.
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry fred_match(&trans, GET_BY_PATH, PSTR("fred_match"));
    ASSERT_TRUE(fred_match.good());
    EXPECT_TRUE(fred_match.Get(ID).ServerKnows());
    fred_match_id = fred_match.Get(ID);
    mock_server_->AddUpdateBookmark(fred_match_id, trans.root_id(),
        "fred_match", 40, 40);
  }
  // Run the syncer.
  for (int i = 0 ; i < 30 ; ++i) {
    syncer_->SyncShare();
  }
}

TEST_F(SyncerTest, NameClashWithResolverInconsistentUpdates) {
  // I'm unsure what the client should really do when the scenario in this old
  // test occurs. The set of updates we've received are not consistent.
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());
  const char* base_name = "name_clash_with_resolver";
  const char* full_name = "name_clash_with_resolver.htm";
  const PathChar* base_name_p = PSTR("name_clash_with_resolver");
  mock_server_->AddUpdateBookmark(1, 0, full_name, 10, 10);
  syncer_->SyncShare();
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry entry(&trans, GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(entry.good());
    WriteTestDataToEntry(&trans, &entry);
  }
  mock_server_->AddUpdateBookmark(2, 0, full_name, 10, 10);
  mock_server_->set_conflict_n_commits(1);
  syncer_->SyncShare();
  mock_server_->set_conflict_n_commits(1);
  syncer_->SyncShare();
  EXPECT_TRUE(0 == syncer_events_.size());
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    Entry id1(&trans, GET_BY_ID, ids_.FromNumber(1));
    Entry id2(&trans, GET_BY_ID, ids_.FromNumber(2));
    ASSERT_TRUE(id1.good());
    ASSERT_TRUE(id2.good());
    EXPECT_TRUE(root_id_ == id1.Get(PARENT_ID));
    EXPECT_TRUE(root_id_ == id2.Get(PARENT_ID));
    PathString id1name = id1.Get(NAME);

    EXPECT_TRUE(base_name_p == id1name.substr(0, strlen(base_name)));
    EXPECT_TRUE(PSTR(".htm") == id1name.substr(id1name.length() - 4));
    EXPECT_LE(id1name.length(), 200ul);
    EXPECT_TRUE(PSTR("name_clash_with_resolver.htm") == id2.Get(NAME));
  }
}

TEST_F(SyncerTest, NameClashWithResolver) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());
  const char* base_name = "name_clash_with_resolver";
  const char* full_name = "name_clash_with_resolver.htm";
  const PathChar* base_name_p = PSTR("name_clash_with_resolver");
  const PathChar* full_name_p = PSTR("name_clash_with_resolver.htm");
  mock_server_->AddUpdateBookmark(1, 0, "fred", 10, 10);
  syncer_->SyncShare();
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry entry(&trans, GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(entry.good());
    entry.Put(NAME, full_name_p);
    WriteTestDataToEntry(&trans, &entry);
  }
  mock_server_->AddUpdateBookmark(2, 0, full_name, 10, 10);
  // We do NOT use LoopSyncShare here because of the way that
  // mock_server_->conflict_n_commits works.
  // It will only conflict the first n commits, so if we let the syncer loop,
  // the second commit of the update will succeed even though it shouldn't.
  mock_server_->set_conflict_n_commits(1);
  syncer_->SyncShare(state_.get());
  mock_server_->set_conflict_n_commits(1);
  syncer_->SyncShare(state_.get());
  EXPECT_TRUE(0 == syncer_events_.size());
  syncer_events_.clear();
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    Entry id1(&trans, GET_BY_ID, ids_.FromNumber(1));
    Entry id2(&trans, GET_BY_ID, ids_.FromNumber(2));
    ASSERT_TRUE(id1.good());
    ASSERT_TRUE(id2.good());
    EXPECT_TRUE(root_id_ == id1.Get(PARENT_ID));
    EXPECT_TRUE(root_id_ == id2.Get(PARENT_ID));
    PathString id1name = id1.Get(NAME);

    EXPECT_TRUE(base_name_p == id1name.substr(0, strlen(base_name)));
    EXPECT_TRUE(PSTR(".htm") == id1name.substr(id1name.length() - 4));
    EXPECT_LE(id1name.length(), 200ul);
    EXPECT_TRUE(full_name_p == id2.Get(NAME));
  }
}

TEST_F(SyncerTest, VeryLongNameClashWithResolver) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());
  string name;
  PathString name_w;
  name.resize(250, 'X');
  name_w.resize(250, 'X');
  name.append(".htm");
  name_w.append(PSTR(".htm"));
  mock_server_->AddUpdateBookmark(1, 0, "fred", 10, 10);
  syncer_->SyncShare();
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry entry(&trans, GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(entry.good());
    entry.Put(NAME, name_w);
    WriteTestDataToEntry(&trans, &entry);
  }
  mock_server_->AddUpdateBookmark(2, 0, name, 10, 10);
  mock_server_->set_conflict_n_commits(1);
  // We do NOT use LoopSyncShare here because of the way that
  // mock_server_->conflict_n_commits works.
  // It will only conflict the first n commits, so if we let the syncer loop,
  // the second commit of the update will succeed even though it shouldn't.
  syncer_->SyncShare(state_.get());
  mock_server_->set_conflict_n_commits(1);
  syncer_->SyncShare(state_.get());
  EXPECT_TRUE(0 == syncer_events_.size());
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    Entry id1(&trans, GET_BY_ID, ids_.FromNumber(1));
    Entry id2(&trans, GET_BY_ID, ids_.FromNumber(2));
    ASSERT_TRUE(id1.good());
    ASSERT_TRUE(id2.good());
    EXPECT_TRUE(root_id_ == id1.Get(PARENT_ID));
    EXPECT_TRUE(root_id_ == id2.Get(PARENT_ID));
    PathString id1name = id1.Get(NAME);
    EXPECT_TRUE(PSTR(".htm") == id1name.substr(id1name.length() - 4));
    EXPECT_TRUE(name_w == id2.Get(NAME));
  }
}

TEST_F(SyncerTest, NameClashWithResolverAndDotStartedName) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());
  mock_server_->AddUpdateBookmark(1, 0, ".bob.htm", 10, 10);
  syncer_->SyncShare();
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry entry(&trans, GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(entry.good());
    entry.Put(IS_UNSYNCED, true);
    entry.Put(NAME, PSTR(".htm"));
    WriteTestDataToEntry(&trans, &entry);
  }
  mock_server_->set_conflict_all_commits(true);
  mock_server_->AddUpdateBookmark(2, 0, ".htm", 10, 10);
  syncer_->SyncShare();
  syncer_->SyncShare();
  EXPECT_TRUE(0 == syncer_events_.size());
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    Entry id1(&trans, GET_BY_ID, ids_.FromNumber(1));
    Entry id2(&trans, GET_BY_ID, ids_.FromNumber(2));
    ASSERT_TRUE(id1.good());
    ASSERT_TRUE(id2.good());
    EXPECT_TRUE(root_id_ == id1.Get(PARENT_ID));
    EXPECT_TRUE(root_id_ == id2.Get(PARENT_ID));
    PathString id1name = id1.Get(NAME);
    EXPECT_TRUE(PSTR(".htm") == id1name.substr(0, 4));
    EXPECT_TRUE(PSTR(".htm") == id2.Get(NAME));
  }
}

TEST_F(SyncerTest, ThreeNamesClashWithResolver) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());
  mock_server_->set_conflict_all_commits(true);
  mock_server_->AddUpdateBookmark(1, 0, "in_root.htm", 10, 10);
  LoopSyncShare(syncer_);
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry entry(&trans, GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(entry.good());
    ASSERT_FALSE(entry.Get(IS_DEL));
    entry.Put(IS_UNSYNCED, true);
  }
  mock_server_->AddUpdateBookmark(2, 0, "in_root.htm", 10, 10);
  LoopSyncShare(syncer_);
  LoopSyncShare(syncer_);
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry entry(&trans, GET_BY_ID, ids_.FromNumber(2));
    ASSERT_TRUE(entry.good());
    ASSERT_FALSE(entry.Get(IS_DEL));
    entry.Put(IS_UNSYNCED, true);
  }
  mock_server_->AddUpdateBookmark(3, 0, "in_root.htm", 10, 10);
  LoopSyncShare(syncer_);
  LoopSyncShare(syncer_);
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry entry(&trans, GET_BY_ID, ids_.FromNumber(3));
    ASSERT_TRUE(entry.good());
    ASSERT_FALSE(entry.Get(IS_DEL));
    entry.Put(IS_UNSYNCED, true);
  }
  mock_server_->AddUpdateBookmark(4, 0, "in_root.htm", 10, 10);
  LoopSyncShare(syncer_);
  LoopSyncShare(syncer_);
  EXPECT_TRUE(0 == syncer_events_.size());
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    Entry id1(&trans, GET_BY_ID, ids_.FromNumber(1));
    Entry id2(&trans, GET_BY_ID, ids_.FromNumber(2));
    Entry id3(&trans, GET_BY_ID, ids_.FromNumber(3));
    Entry id4(&trans, GET_BY_ID, ids_.FromNumber(4));
    ASSERT_TRUE(id1.good());
    ASSERT_TRUE(id2.good());
    ASSERT_TRUE(id3.good());
    ASSERT_TRUE(id4.good());
    EXPECT_TRUE(root_id_ == id1.Get(PARENT_ID));
    EXPECT_TRUE(root_id_ == id2.Get(PARENT_ID));
    EXPECT_TRUE(root_id_ == id3.Get(PARENT_ID));
    EXPECT_TRUE(root_id_ == id4.Get(PARENT_ID));
    PathString id1name = id1.Get(NAME);
    ASSERT_GE(id1name.length(), 4ul);
    EXPECT_TRUE(PSTR("in_root") == id1name.substr(0, 7));
    EXPECT_TRUE(PSTR(".htm") == id1name.substr(id1name.length() - 4));
    EXPECT_NE(PSTR("in_root.htm"), id1.Get(NAME));
    PathString id2name = id2.Get(NAME);
    ASSERT_GE(id2name.length(), 4ul);
    EXPECT_TRUE(PSTR("in_root") == id2name.substr(0, 7));
    EXPECT_TRUE(PSTR(".htm") == id2name.substr(id2name.length() - 4));
    EXPECT_NE(PSTR("in_root.htm"), id2.Get(NAME));
    PathString id3name = id3.Get(NAME);
    ASSERT_GE(id3name.length(), 4ul);
    EXPECT_TRUE(PSTR("in_root") == id3name.substr(0, 7));
    EXPECT_TRUE(PSTR(".htm") == id3name.substr(id3name.length() - 4));
    EXPECT_NE(PSTR("in_root.htm"), id3.Get(NAME));
    EXPECT_TRUE(PSTR("in_root.htm") == id4.Get(NAME));
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
    MutableEntry parent(&wtrans, syncable::CREATE, root_id_, PSTR("Folder"));
    ASSERT_TRUE(parent.good());
    parent.Put(syncable::IS_DIR, true);
    parent.Put(syncable::ID, parent_id_);
    parent.Put(syncable::BASE_VERSION, 5);
    MutableEntry child(&wtrans, syncable::CREATE, parent_id_, PSTR("Pete.htm"));
    ASSERT_TRUE(child.good());
    child.Put(syncable::ID, child_id_);
    child.Put(syncable::BASE_VERSION, 10);
    WriteTestDataToEntry(&wtrans, &child);
  }
  mock_server_->AddUpdateBookmark(child_id_, parent_id_, "Pete.htm", 11, 10);
  mock_server_->set_conflict_all_commits(true);
  LoopSyncShare(syncer_);
  syncable::Directory::ChildHandles children;
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    dir->GetChildHandles(&trans, parent_id_, &children);
    // We expect the conflict resolver to just clobber the entry.
    Entry child(&trans, syncable::GET_BY_ID, child_id_);
    ASSERT_TRUE(child.good());
    EXPECT_TRUE(child.Get(syncable::IS_UNSYNCED));
    EXPECT_FALSE(child.Get(syncable::IS_UNAPPLIED_UPDATE));
  }

  // Only one entry, since we just overwrite one.
  EXPECT_TRUE(1 == children.size());
  syncer_events_.clear();
}

// We got this repro case when someone was editing entries while sync was
// occuring. The entry had changed out underneath the user.
TEST_F(SyncerTest, CommitsUpdateDoesntAlterEntry) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());
  int64 test_time = 123456;
  {
    WriteTransaction wtrans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry entry(&wtrans, syncable::CREATE, root_id_, PSTR("Pete"));
    ASSERT_TRUE(entry.good());
    EXPECT_FALSE(entry.Get(ID).ServerKnows());
    entry.Put(syncable::IS_DIR, true);
    entry.Put(syncable::IS_UNSYNCED, true);
    entry.Put(syncable::MTIME, test_time);
  }
  syncer_->SyncShare();
  syncable::Id id;
  int64 version;
  int64 server_position_in_parent;
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    Entry entry(&trans, syncable::GET_BY_PARENTID_AND_NAME, trans.root_id(),
                PSTR("Pete"));
    ASSERT_TRUE(entry.good());
    id = entry.Get(ID);
    EXPECT_TRUE(id.ServerKnows());
    version = entry.Get(BASE_VERSION);
    server_position_in_parent = entry.Get(SERVER_POSITION_IN_PARENT);
  }
  mock_server_->AddUpdateDirectory(id, root_id_, "Pete", version, 10);
  mock_server_->SetLastUpdatePosition(server_position_in_parent);
  syncer_->SyncShare();
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
  {
    WriteTransaction wtrans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry parent(&wtrans, CREATE, root_id_, PSTR("Folder"));
    ASSERT_TRUE(parent.good());
    parent.Put(IS_DIR, true);
    parent.Put(IS_UNSYNCED, true);
    MutableEntry child(&wtrans, CREATE, parent.Get(ID), PSTR("test.htm"));
    ASSERT_TRUE(child.good());
    WriteTestDataToEntry(&wtrans, &child);
  }
  mock_server_->AddUpdateDirectory(parent_id_, root_id_, "Folder", 10, 10);
  mock_server_->AddUpdateBookmark(child_id_, parent_id_, "test.htm", 10, 10);
  mock_server_->set_conflict_all_commits(true);
  syncer_->SyncShare();
  syncer_->SyncShare();
  syncer_->SyncShare();
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    Directory::ChildHandles children;
    dir->GetChildHandles(&trans, root_id_, &children);
    EXPECT_TRUE(1 == children.size());
    dir->GetChildHandles(&trans, parent_id_, &children);
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
    MutableEntry entry(&trans, CREATE, trans.root_id(), PSTR("bob"));
    entry.Put(IS_UNSYNCED, true);
    entry.Put(IS_DEL, true);
  }
  syncer_->SyncShare();
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
    MutableEntry entry(&trans, CREATE, trans.root_id(), PSTR("bob"));
    entry.Put(ID, ids_.FromNumber(20));
    entry.Put(BASE_VERSION, 1);
    entry.Put(SERVER_VERSION, 1);
    entry.Put(SERVER_PARENT_ID, ids_.FromNumber(9999));  // Bad parent.
    entry.Put(IS_UNSYNCED, true);
    entry.Put(IS_UNAPPLIED_UPDATE, true);
    entry.Put(IS_DEL, false);
  }
  syncer_->SyncShare(state_.get());
  syncer_->SyncShare(state_.get());
  SyncerStatus status(NULL, state_.get());
  EXPECT_TRUE(0 == status.conflicting_updates());
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
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry entry(&trans, CREATE, trans.root_id(), PSTR("existing"));
    ASSERT_TRUE(entry.good());
    entry.Put(IS_DIR, true);
    entry.Put(IS_UNSYNCED, true);
  }
  syncer_->SyncShare(state_.get());
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry newfolder(&trans, CREATE, trans.root_id(), PSTR("new"));
    ASSERT_TRUE(newfolder.good());
    newfolder.Put(IS_DIR, true);
    newfolder.Put(IS_UNSYNCED, true);

    MutableEntry existing(&trans, GET_BY_PATH, PSTR("existing"));
    ASSERT_TRUE(existing.good());
    existing.Put(PARENT_ID, newfolder.Get(ID));
    existing.Put(IS_UNSYNCED, true);
    EXPECT_TRUE(existing.Get(ID).ServerKnows());

    newfolder.Put(IS_DEL, true);
    existing.Put(IS_DEL, true);
  }
  syncer_->SyncShare(state_.get());
  SyncerStatus status(NULL, state_.get());
  EXPECT_TRUE(0 == status.error_commits());
  EXPECT_TRUE(0 == status.conflicting_commits());
  EXPECT_TRUE(0 == status.BlockedItemsSize());
}

// TODO(sync): Is this test useful anymore?
TEST_F(SyncerTest, DeletingEntryWithLocalEdits) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());
  mock_server_->AddUpdateDirectory(1, 0, "bob", 1, 10);
  syncer_->SyncShare();
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry newfolder(&trans, CREATE, ids_.FromNumber(1), PSTR("local"));
    ASSERT_TRUE(newfolder.good());
    newfolder.Put(IS_UNSYNCED, true);
  }
  mock_server_->AddUpdateDirectory(1, 0, "bob", 2, 20);
  mock_server_->SetLastUpdateDeleted();
  syncer_->SyncShare(SYNCER_BEGIN, APPLY_UPDATES);
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    Entry entry_by_path(&trans, syncable::GET_BY_PATH,
        PathString(PSTR("bob")) + kPathSeparator + PSTR("local"));
    ASSERT_TRUE(entry_by_path.good());
  }
}

TEST_F(SyncerTest, FolderSwapUpdate) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());
  mock_server_->AddUpdateDirectory(7801, 0, "bob", 1, 10);
  mock_server_->AddUpdateDirectory(1024, 0, "fred", 1, 10);
  syncer_->SyncShare();
  mock_server_->AddUpdateDirectory(1024, 0, "bob", 2, 20);
  mock_server_->AddUpdateDirectory(7801, 0, "fred", 2, 20);
  syncer_->SyncShare();
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    Entry id1(&trans, GET_BY_ID, ids_.FromNumber(7801));
    ASSERT_TRUE(id1.good());
    EXPECT_TRUE(PSTR("fred") == id1.Get(NAME));
    EXPECT_TRUE(root_id_ == id1.Get(PARENT_ID));
    Entry id2(&trans, GET_BY_ID, ids_.FromNumber(1024));
    ASSERT_TRUE(id2.good());
    EXPECT_TRUE(PSTR("bob") == id2.Get(NAME));
    EXPECT_TRUE(root_id_ == id2.Get(PARENT_ID));
  }
  syncer_events_.clear();
}

TEST_F(SyncerTest, CorruptUpdateBadFolderSwapUpdate) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());
  mock_server_->AddUpdateDirectory(7801, 0, "bob", 1, 10);
  mock_server_->AddUpdateDirectory(1024, 0, "fred", 1, 10);
  mock_server_->AddUpdateDirectory(4096, 0, "alice", 1, 10);
  syncer_->SyncShare();
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    Entry id1(&trans, GET_BY_ID, ids_.FromNumber(7801));
    ASSERT_TRUE(id1.good());
    EXPECT_TRUE(PSTR("bob") == id1.Get(NAME));
    EXPECT_TRUE(root_id_ == id1.Get(PARENT_ID));
    Entry id2(&trans, GET_BY_ID, ids_.FromNumber(1024));
    ASSERT_TRUE(id2.good());
    EXPECT_TRUE(PSTR("fred") == id2.Get(NAME));
    EXPECT_TRUE(root_id_ == id2.Get(PARENT_ID));
    Entry id3(&trans, GET_BY_ID, ids_.FromNumber(4096));
    ASSERT_TRUE(id3.good());
    EXPECT_TRUE(PSTR("alice") == id3.Get(NAME));
    EXPECT_TRUE(root_id_ == id3.Get(PARENT_ID));
  }
  mock_server_->AddUpdateDirectory(1024, 0, "bob", 2, 20);
  mock_server_->AddUpdateDirectory(7801, 0, "fred", 2, 20);
  mock_server_->AddUpdateDirectory(4096, 0, "bob", 2, 20);
  syncer_->SyncShare();
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    Entry id1(&trans, GET_BY_ID, ids_.FromNumber(7801));
    ASSERT_TRUE(id1.good());
    EXPECT_TRUE(PSTR("bob") == id1.Get(NAME));
    EXPECT_TRUE(root_id_ == id1.Get(PARENT_ID));
    Entry id2(&trans, GET_BY_ID, ids_.FromNumber(1024));
    ASSERT_TRUE(id2.good());
    EXPECT_TRUE(PSTR("fred") == id2.Get(NAME));
    EXPECT_TRUE(root_id_ == id2.Get(PARENT_ID));
    Entry id3(&trans, GET_BY_ID, ids_.FromNumber(4096));
    ASSERT_TRUE(id3.good());
    EXPECT_TRUE(PSTR("alice") == id3.Get(NAME));
    EXPECT_TRUE(root_id_ == id3.Get(PARENT_ID));
  }
  syncer_events_.clear();
}

// TODO(chron): New set of folder swap commit tests that don't rely on
// transactional commits.
TEST_F(SyncerTest, DISABLED_FolderSwapCommit) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());
  mock_server_->AddUpdateDirectory(7801, 0, "bob", 1, 10);
  mock_server_->AddUpdateDirectory(1024, 0, "fred", 1, 10);
  syncer_->SyncShare();
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry id1(&trans, GET_BY_ID, ids_.FromNumber(7801));
    MutableEntry id2(&trans, GET_BY_ID, ids_.FromNumber(1024));
    ASSERT_TRUE(id1.good());
    ASSERT_TRUE(id2.good());
    EXPECT_FALSE(id1.Put(NAME, PSTR("fred")));
    EXPECT_TRUE(id1.Put(NAME, PSTR("temp")));
    EXPECT_TRUE(id2.Put(NAME, PSTR("bob")));
    EXPECT_TRUE(id1.Put(NAME, PSTR("fred")));
    id1.Put(IS_UNSYNCED, true);
    id2.Put(IS_UNSYNCED, true);
  }
  mock_server_->set_conflict_all_commits(true);
  syncer_->SyncShare();
  ASSERT_TRUE(2 == mock_server_->commit_messages().size());
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    Entry id1(&trans, GET_BY_ID, ids_.FromNumber(7801));
    ASSERT_TRUE(id1.good());
    EXPECT_TRUE(PSTR("fred") == id1.Get(NAME));
    EXPECT_TRUE(root_id_ == id1.Get(PARENT_ID));
    EXPECT_FALSE(id1.Get(IS_UNSYNCED));
    Entry id2(&trans, GET_BY_ID, ids_.FromNumber(1024));
    ASSERT_TRUE(id2.good());
    EXPECT_TRUE(PSTR("bob") == id2.Get(NAME));
    EXPECT_TRUE(root_id_ == id2.Get(PARENT_ID));
    EXPECT_FALSE(id2.Get(IS_UNSYNCED));
  }
  syncer_events_.clear();
}

// TODO(chron): New set of folder swap commit tests that don't rely on
// transactional commits.
TEST_F(SyncerTest, DISABLED_DualFolderSwapCommit) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());
  mock_server_->AddUpdateDirectory(1, 0, "bob", 1, 10);
  mock_server_->AddUpdateDirectory(2, 0, "fred", 1, 10);
  mock_server_->AddUpdateDirectory(3, 0, "sue", 1, 10);
  mock_server_->AddUpdateDirectory(4, 0, "greg", 1, 10);
  syncer_->SyncShare();
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry id1(&trans, GET_BY_ID, ids_.FromNumber(1));
    MutableEntry id2(&trans, GET_BY_ID, ids_.FromNumber(2));
    MutableEntry id3(&trans, GET_BY_ID, ids_.FromNumber(3));
    MutableEntry id4(&trans, GET_BY_ID, ids_.FromNumber(4));
    ASSERT_TRUE(id1.good());
    ASSERT_TRUE(id2.good());
    ASSERT_TRUE(id3.good());
    ASSERT_TRUE(id4.good());
    EXPECT_FALSE(id1.Put(NAME, PSTR("fred")));
    EXPECT_TRUE(id1.Put(NAME, PSTR("temp")));
    EXPECT_TRUE(id2.Put(NAME, PSTR("bob")));
    EXPECT_TRUE(id1.Put(NAME, PSTR("fred")));
    EXPECT_FALSE(id3.Put(NAME, PSTR("greg")));
    EXPECT_TRUE(id3.Put(NAME, PSTR("temp")));
    EXPECT_TRUE(id4.Put(NAME, PSTR("sue")));
    EXPECT_TRUE(id3.Put(NAME, PSTR("greg")));
    id1.Put(IS_UNSYNCED, true);
    id2.Put(IS_UNSYNCED, true);
    id3.Put(IS_UNSYNCED, true);
    id4.Put(IS_UNSYNCED, true);
  }
  mock_server_->set_conflict_all_commits(true);
  syncer_->SyncShare();
  ASSERT_TRUE(4 == mock_server_->commit_messages().size());
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    Entry id1(&trans, GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(id1.good());
    EXPECT_TRUE(PSTR("fred") == id1.Get(NAME));
    EXPECT_TRUE(root_id_ == id1.Get(PARENT_ID));
    EXPECT_FALSE(id1.Get(IS_UNSYNCED));
    Entry id2(&trans, GET_BY_ID, ids_.FromNumber(2));
    ASSERT_TRUE(id2.good());
    EXPECT_TRUE(PSTR("bob") == id2.Get(NAME));
    EXPECT_TRUE(root_id_ == id2.Get(PARENT_ID));
    EXPECT_FALSE(id2.Get(IS_UNSYNCED));
    Entry id3(&trans, GET_BY_ID, ids_.FromNumber(3));
    ASSERT_TRUE(id3.good());
    EXPECT_TRUE(PSTR("greg") == id3.Get(NAME));
    EXPECT_TRUE(root_id_ == id3.Get(PARENT_ID));
    EXPECT_FALSE(id3.Get(IS_UNSYNCED));
    Entry id4(&trans, GET_BY_ID, ids_.FromNumber(4));
    ASSERT_TRUE(id4.good());
    EXPECT_TRUE(PSTR("sue") == id4.Get(NAME));
    EXPECT_TRUE(root_id_ == id4.Get(PARENT_ID));
    EXPECT_FALSE(id4.Get(IS_UNSYNCED));
  }
  syncer_events_.clear();
}

// TODO(chron): New set of folder swap commit tests that don't rely on
// transactional commits.
TEST_F(SyncerTest, DISABLED_TripleFolderRotateCommit) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());
  mock_server_->AddUpdateDirectory(1, 0, "bob", 1, 10);
  mock_server_->AddUpdateDirectory(2, 0, "fred", 1, 10);
  mock_server_->AddUpdateDirectory(3, 0, "sue", 1, 10);
  syncer_->SyncShare();
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry id1(&trans, GET_BY_ID, ids_.FromNumber(1));
    MutableEntry id2(&trans, GET_BY_ID, ids_.FromNumber(2));
    MutableEntry id3(&trans, GET_BY_ID, ids_.FromNumber(3));
    ASSERT_TRUE(id1.good());
    ASSERT_TRUE(id2.good());
    ASSERT_TRUE(id3.good());
    EXPECT_FALSE(id1.Put(NAME, PSTR("sue")));
    EXPECT_TRUE(id1.Put(NAME, PSTR("temp")));
    EXPECT_TRUE(id2.Put(NAME, PSTR("bob")));
    EXPECT_TRUE(id3.Put(NAME, PSTR("fred")));
    EXPECT_TRUE(id1.Put(NAME, PSTR("sue")));
    id1.Put(IS_UNSYNCED, true);
    id2.Put(IS_UNSYNCED, true);
    id3.Put(IS_UNSYNCED, true);
  }
  mock_server_->set_conflict_all_commits(true);
  syncer_->SyncShare();
  ASSERT_TRUE(2 == mock_server_->commit_messages().size());
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    Entry id1(&trans, GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(id1.good());
    EXPECT_TRUE(PSTR("sue") == id1.Get(NAME));
    EXPECT_TRUE(root_id_ == id1.Get(PARENT_ID));
    EXPECT_FALSE(id1.Get(IS_UNSYNCED));
    Entry id2(&trans, GET_BY_ID, ids_.FromNumber(2));
    ASSERT_TRUE(id2.good());
    EXPECT_TRUE(PSTR("bob") == id2.Get(NAME));
    EXPECT_TRUE(root_id_ == id2.Get(PARENT_ID));
    EXPECT_FALSE(id2.Get(IS_UNSYNCED));
    Entry id3(&trans, GET_BY_ID, ids_.FromNumber(3));
    ASSERT_TRUE(id3.good());
    EXPECT_TRUE(PSTR("fred") == id3.Get(NAME));
    EXPECT_TRUE(root_id_ == id3.Get(PARENT_ID));
    EXPECT_FALSE(id3.Get(IS_UNSYNCED));
  }
  syncer_events_.clear();
}

// TODO(chron): New set of folder swap commit tests that don't rely on
// transactional commits.
TEST_F(SyncerTest, DISABLED_ServerAndClientSwap) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());
  mock_server_->AddUpdateDirectory(1, 0, "bob", 1, 10);
  mock_server_->AddUpdateDirectory(2, 0, "fred", 1, 10);
  mock_server_->AddUpdateDirectory(3, 0, "sue", 1, 10);
  mock_server_->AddUpdateDirectory(4, 0, "greg", 1, 10);
  syncer_->SyncShare();
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry id1(&trans, GET_BY_ID, ids_.FromNumber(1));
    MutableEntry id2(&trans, GET_BY_ID, ids_.FromNumber(2));
    ASSERT_TRUE(id1.good());
    ASSERT_TRUE(id2.good());
    EXPECT_FALSE(id1.Put(NAME, PSTR("fred")));
    EXPECT_TRUE(id1.Put(NAME, PSTR("temp")));
    EXPECT_TRUE(id2.Put(NAME, PSTR("bob")));
    EXPECT_TRUE(id1.Put(NAME, PSTR("fred")));
    id1.Put(IS_UNSYNCED, true);
    id2.Put(IS_UNSYNCED, true);
  }
  mock_server_->set_conflict_all_commits(true);
  mock_server_->AddUpdateDirectory(3, 0, "greg", 2, 20);
  mock_server_->AddUpdateDirectory(4, 0, "sue", 2, 20);
  syncer_->SyncShare();
  ASSERT_TRUE(2 == mock_server_->commit_messages().size());
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    Entry id1(&trans, GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(id1.good());
    EXPECT_TRUE(PSTR("fred") == id1.Get(NAME));
    EXPECT_TRUE(root_id_ == id1.Get(PARENT_ID));
    EXPECT_FALSE(id1.Get(IS_UNSYNCED));
    Entry id2(&trans, GET_BY_ID, ids_.FromNumber(2));
    ASSERT_TRUE(id2.good());
    EXPECT_TRUE(PSTR("bob") == id2.Get(NAME));
    EXPECT_TRUE(root_id_ == id2.Get(PARENT_ID));
    EXPECT_FALSE(id2.Get(IS_UNSYNCED));
    Entry id3(&trans, GET_BY_ID, ids_.FromNumber(3));
    ASSERT_TRUE(id3.good());
    EXPECT_TRUE(PSTR("greg") == id3.Get(NAME));
    EXPECT_TRUE(root_id_ == id3.Get(PARENT_ID));
    EXPECT_FALSE(id3.Get(IS_UNSYNCED));
    Entry id4(&trans, GET_BY_ID, ids_.FromNumber(4));
    ASSERT_TRUE(id4.good());
    EXPECT_TRUE(PSTR("sue") == id4.Get(NAME));
    EXPECT_TRUE(root_id_ == id4.Get(PARENT_ID));
    EXPECT_FALSE(id4.Get(IS_UNSYNCED));
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
      PathString name(nameutf8.begin(), nameutf8.end());
      MutableEntry e(&trans, CREATE, trans.root_id(), name);
      e.Put(IS_UNSYNCED, true);
      e.Put(IS_DIR, true);
    }
  }
  uint32 num_loops = 0;
  while (syncer_->SyncShare()) {
    num_loops++;
    ASSERT_LT(num_loops, max_batches * 2);
  }
  EXPECT_GE(mock_server_->commit_messages().size(), max_batches);
}

TEST_F(SyncerTest, HugeConflict) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  PathString name = PSTR("f");
  int item_count = 30;  // We should be able to do 300 or 3000 w/o issue.
  CHECK(dir.good());
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    syncable::Id last_id = trans.root_id();
    for (int i = 0; i < item_count ; i++) {
      MutableEntry e(&trans, CREATE, last_id, name);
      e.Put(IS_UNSYNCED, true);
      e.Put(IS_DIR, true);
      last_id = e.Get(ID);
    }
  }
  syncer_->SyncShare();
  CHECK(dir.good());
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry e(&trans, GET_BY_PARENTID_AND_NAME, root_id_, name);
    syncable::Id in_root = e.Get(ID);
    syncable::Id last_id = e.Get(ID);
    for (int i = 0; i < item_count - 1 ; i++) {
      MutableEntry e(&trans, GET_BY_PARENTID_AND_NAME, last_id, name);
      ASSERT_TRUE(e.good());
      mock_server_->AddUpdateDirectory(in_root, root_id_, "BOB", 2, 20);
      mock_server_->SetLastUpdateDeleted();
      if (0 == i)
        e.Put(IS_UNSYNCED, true);
      last_id = e.Get(ID);
    }
  }
  mock_server_->set_conflict_all_commits(true);
  syncer_->SyncShare();
  syncer_->SyncShare();
  syncer_->SyncShare();
  CHECK(dir.good());
}

TEST_F(SyncerTest, CaseChangeNameClashConflict) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());
  mock_server_->AddUpdateDirectory(1, 0, "bob", 1, 10);
  syncer_->SyncShare();
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry e(&trans, GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(e.good());
    e.Put(IS_UNSYNCED, true);
  }
  mock_server_->set_conflict_all_commits(true);
  mock_server_->AddUpdateDirectory(1, 0, "BOB", 2, 20);
  syncer_->SyncShare();  // USED TO CAUSE AN ASSERT
  syncer_events_.clear();
}

TEST_F(SyncerTest, UnsyncedItemAndUpdate) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());
  mock_server_->AddUpdateDirectory(1, 0, "bob", 1, 10);
  syncer_->SyncShare();
  mock_server_->set_conflict_all_commits(true);
  mock_server_->AddUpdateDirectory(2, 0, "bob", 2, 20);
  syncer_->SyncShare();  // USED TO CAUSE AN ASSERT
  syncer_events_.clear();
}

TEST_F(SyncerTest, FolderMergeWithChildNameClash) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());
  syncable::Id local_folder_id, root_id;
  mock_server_->AddUpdateDirectory(parent_id_, root_id_, "Folder2", 10, 10);
  mock_server_->AddUpdateBookmark(child_id_, parent_id_, "Bookmark", 10, 10);
  syncer_->SyncShare();
  int64 local_folder_handle;
  {
    WriteTransaction wtrans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry parent(&wtrans, CREATE, root_id_, PSTR("Folder"));
    ASSERT_TRUE(parent.good());
    local_folder_id = parent.Get(ID);
    local_folder_handle = parent.Get(META_HANDLE);
    parent.Put(IS_DIR, true);
    parent.Put(IS_UNSYNCED, true);
    MutableEntry child(&wtrans, CREATE, parent.Get(ID), PSTR("Bookmark"));
    ASSERT_TRUE(child.good());
    WriteTestDataToEntry(&wtrans, &child);
  }
  mock_server_->AddUpdateDirectory(parent_id_, root_id_, "Folder", 20, 20);
  mock_server_->set_conflict_all_commits(true);
  LoopSyncShare(syncer_);
  LoopSyncShare(syncer_);
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    Directory::ChildHandles children;
    dir->GetChildHandles(&trans, root_id_, &children);
    ASSERT_TRUE(2 == children.size());
    Entry parent(&trans, GET_BY_ID, parent_id_);
    ASSERT_TRUE(parent.good());
    EXPECT_TRUE(parent.Get(NAME) == PSTR("Folder"));
    if (local_folder_handle == children[0]) {
      EXPECT_TRUE(children[1] == parent.Get(META_HANDLE));
    } else {
      EXPECT_TRUE(children[0] == parent.Get(META_HANDLE));
      EXPECT_TRUE(children[1] == local_folder_handle);
    }
    dir->GetChildHandles(&trans, local_folder_id, &children);
    EXPECT_TRUE(1 == children.size());
    dir->GetChildHandles(&trans, parent_id_, &children);
    EXPECT_TRUE(1 == children.size());
    Directory::UnappliedUpdateMetaHandles unapplied;
    dir->GetUnappliedUpdateMetaHandles(&trans, &unapplied);
    EXPECT_TRUE(0 == unapplied.size());
    syncable::Directory::UnsyncedMetaHandles unsynced;
    dir->GetUnsyncedMetaHandles(&trans, &unsynced);
    EXPECT_TRUE(2 == unsynced.size());
  }
  mock_server_->set_conflict_all_commits(false);
  syncer_->SyncShare();
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    syncable::Directory::UnsyncedMetaHandles unsynced;
    dir->GetUnsyncedMetaHandles(&trans, &unsynced);
    EXPECT_TRUE(0 == unsynced.size());
  }
  syncer_events_.clear();
}

TEST_F(SyncerTest, NewEntryAndAlteredServerEntrySharePath) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());
  mock_server_->AddUpdateBookmark(1, 0, "Foo.htm", 10, 10);
  syncer_->SyncShare();
  int64 local_folder_handle;
  syncable::Id local_folder_id;
  {
    WriteTransaction wtrans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry new_entry(&wtrans, CREATE, wtrans.root_id(), PSTR("Bar.htm"));
    ASSERT_TRUE(new_entry.good());
    local_folder_id = new_entry.Get(ID);
    local_folder_handle = new_entry.Get(META_HANDLE);
    new_entry.Put(IS_UNSYNCED, true);
    MutableEntry old(&wtrans, GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(old.good());
    WriteTestDataToEntry(&wtrans, &old);
  }
  mock_server_->AddUpdateBookmark(1, 0, "Bar.htm", 20, 20);
  mock_server_->set_conflict_all_commits(true);
  syncer_->SyncShare();
  syncer_events_.clear();
}

// Circular links should be resolved by the server.
TEST_F(SyncerTest, SiblingDirectoriesBecomeCircular) {
  // we don't currently resolve this. This test ensures we don't.
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());
  mock_server_->AddUpdateDirectory(1, 0, "A", 10, 10);
  mock_server_->AddUpdateDirectory(2, 0, "B", 10, 10);
  syncer_->SyncShare();
  {
    WriteTransaction wtrans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry A(&wtrans, GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(A.good());
    A.Put(IS_UNSYNCED, true);
    ASSERT_TRUE(A.Put(PARENT_ID, ids_.FromNumber(2)));
    ASSERT_TRUE(A.Put(NAME, PSTR("B")));
  }
  mock_server_->AddUpdateDirectory(2, 1, "A", 20, 20);
  mock_server_->set_conflict_all_commits(true);
  syncer_->SyncShare();
  syncer_events_.clear();
  {
    WriteTransaction wtrans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry A(&wtrans, GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(A.good());
    MutableEntry B(&wtrans, GET_BY_ID, ids_.FromNumber(2));
    ASSERT_TRUE(B.good());
    EXPECT_TRUE(A.Get(NAME) == PSTR("B"));
    EXPECT_TRUE(B.Get(NAME) == PSTR("B"));
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
  syncer_->SyncShare();
  {
    WriteTransaction wtrans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry A(&wtrans, GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(A.good());
    A.Put(IS_UNSYNCED, true);
    A.Put(IS_UNAPPLIED_UPDATE, true);
    A.Put(SERVER_NAME, PSTR("B"));
    MutableEntry B(&wtrans, GET_BY_ID, ids_.FromNumber(2));
    ASSERT_TRUE(B.good());
    B.Put(IS_UNAPPLIED_UPDATE, true);
    B.Put(SERVER_NAME, PSTR("A"));
  }
  syncer_->SyncShare();
  syncer_events_.clear();
}

TEST_F(SyncerTest, SwapEntryNames) {
  // Simple transaction test.
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());
  mock_server_->AddUpdateDirectory(1, 0, "A", 10, 10);
  mock_server_->AddUpdateDirectory(2, 0, "B", 10, 10);
  mock_server_->set_conflict_all_commits(true);
  syncer_->SyncShare();
  {
    WriteTransaction wtrans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry A(&wtrans, GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(A.good());
    A.Put(IS_UNSYNCED, true);
    MutableEntry B(&wtrans, GET_BY_ID, ids_.FromNumber(2));
    ASSERT_TRUE(B.good());
    B.Put(IS_UNSYNCED, true);
    ASSERT_TRUE(A.Put(NAME, PSTR("C")));
    ASSERT_TRUE(B.Put(NAME, PSTR("A")));
    ASSERT_TRUE(A.Put(NAME, PSTR("B")));
  }
  syncer_->SyncShare();
  syncer_events_.clear();
}

TEST_F(SyncerTest, DualDeletionWithNewItemNameClash) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());
  mock_server_->AddUpdateDirectory(1, 0, "A", 10, 10);
  mock_server_->AddUpdateBookmark(2, 0, "B", 10, 10);
  mock_server_->set_conflict_all_commits(true);
  syncer_->SyncShare();
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry B(&trans, GET_BY_ID, ids_.FromNumber(2));
    ASSERT_TRUE(B.good());
    WriteTestDataToEntry(&trans, &B);
    B.Put(IS_DEL, true);
  }
  mock_server_->AddUpdateBookmark(2, 0, "A", 11, 11);
  mock_server_->SetLastUpdateDeleted();
  syncer_->SyncShare();
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
  syncer_->SyncShare();
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry bob(&trans, GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(bob.good());
    bob.Put(IS_UNSYNCED, true);
    bob.Put(PARENT_ID, ids_.FromNumber(2));
  }
  mock_server_->AddUpdateDirectory(2, 1, "fred", 2, 20);
  mock_server_->set_conflict_all_commits(true);
  syncer_->SyncShare();
  syncer_->SyncShare();
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
  mock_server_->AddUpdateBookmark(1, 0, "bob", 1, 10);
  syncer_->SyncShare();
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry bob(&trans, GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(bob.good());
    WriteTestDataToEntry(&trans, &bob);
  }
  mock_server_->AddUpdateBookmark(1, 0, "bob", 2, 10);
  mock_server_->SetLastUpdateDeleted();
  mock_server_->set_conflict_all_commits(true);
  syncer_->SyncShare();
  syncer_->SyncShare();
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    Entry bob(&trans, GET_BY_PARENTID_AND_NAME, trans.root_id(), PSTR("bob"));
    ASSERT_TRUE(bob.good());
    EXPECT_TRUE(bob.Get(IS_UNSYNCED));
    EXPECT_FALSE(bob.Get(ID).ServerKnows());
    EXPECT_FALSE(bob.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_FALSE(bob.Get(IS_DEL));
  }
  syncer_events_.clear();
}

// This test is disabled because we actually enforce the opposite behavior in:
// ConflictResolverMergesLocalDeleteAndServerUpdate for bookmarks.
TEST_F(SyncerTest, DISABLED_ResolveWeDeletedTheyWrote) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());
  mock_server_->AddUpdateBookmark(1, 0, "bob", 1, 10);
  syncer_->SyncShare();
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry bob(&trans, GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(bob.good());
    bob.Put(IS_UNSYNCED, true);
    bob.Put(IS_DEL, true);
  }
  mock_server_->AddUpdateBookmark(1, 0, "bob", 2, 10);
  mock_server_->set_conflict_all_commits(true);
  syncer_->SyncShare();
  syncer_->SyncShare();
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    Entry bob(&trans, GET_BY_PARENTID_AND_NAME, trans.root_id(), PSTR("bob"));
    ASSERT_TRUE(bob.good());
    EXPECT_TRUE(bob.Get(ID) == ids_.FromNumber(1));
    EXPECT_FALSE(bob.Get(IS_UNSYNCED));
    EXPECT_FALSE(bob.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_FALSE(bob.Get(IS_DEL));
  }
  syncer_events_.clear();
}

TEST_F(SyncerTest, ServerDeletingFolderWeHaveMovedSomethingInto) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());
  mock_server_->AddUpdateDirectory(1, 0, "bob", 1, 10);
  mock_server_->AddUpdateDirectory(2, 0, "fred", 1, 10);
  syncer_->SyncShare();
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry bob(&trans, GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(bob.good());
    bob.Put(IS_UNSYNCED, true);
    bob.Put(PARENT_ID, ids_.FromNumber(2));
  }
  mock_server_->AddUpdateDirectory(2, 0, "fred", 2, 20);
  mock_server_->SetLastUpdateDeleted();
  mock_server_->set_conflict_all_commits(true);
  syncer_->SyncShare();
  syncer_->SyncShare();
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    Entry bob(&trans, GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(bob.good());
    Entry fred(&trans, GET_BY_PARENTID_AND_NAME, trans.root_id(), PSTR("fred"));
    ASSERT_TRUE(fred.good());
    EXPECT_TRUE(fred.Get(IS_UNSYNCED));
    EXPECT_TRUE(bob.Get(IS_UNSYNCED));
    EXPECT_TRUE(bob.Get(PARENT_ID) == fred.Get(ID));
    EXPECT_FALSE(fred.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_FALSE(bob.Get(IS_UNAPPLIED_UPDATE));
  }
  syncer_events_.clear();
}

// TODO(ncarter): This test is bogus, but it actually seems to hit an
// interesting case the 4th time SyncShare is called.
TEST_F(SyncerTest, DISABLED_ServerDeletingFolderWeHaveAnOpenEntryIn) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());
  mock_server_->AddUpdateBookmark(1, 0, "bob", 1, 10);
  mock_server_->AddUpdateDirectory(2, 0, "fred", 1, 10);
  syncer_->SyncShare(state_.get());
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry bob(&trans, GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(bob.good());
    bob.Put(IS_UNSYNCED, true);
    WriteTestDataToEntry(&trans, &bob);
  }
  syncer_->SyncShare(state_.get());
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
  syncer_->SyncShare(state_.get());
  syncer_->SyncShare(state_.get());
  syncer_->SyncShare(state_.get());
  syncer_->SyncShare(state_.get());
  syncer_->SyncShare(state_.get());
  syncer_->SyncShare(state_.get());
  syncer_->SyncShare(state_.get());
  syncer_->SyncShare(state_.get());
  EXPECT_TRUE(0 == syncer_events_.size());
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    Entry bob(&trans, GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(bob.good());
    Entry fred(&trans, GET_BY_PARENTID_AND_NAME, trans.root_id(), PSTR("fred"));
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
  mock_server_->AddUpdateDirectory(1, 0, "bob", 1, 10);
  mock_server_->AddUpdateDirectory(2, 0, "fred", 1, 10);
  syncer_->SyncShare();
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry bob(&trans, GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(bob.good());
    bob.Put(IS_UNSYNCED, true);
    bob.Put(PARENT_ID, ids_.FromNumber(2));
  }
  mock_server_->AddUpdateDirectory(2, 0, "fred", 2, 20);
  mock_server_->SetLastUpdateDeleted();
  mock_server_->set_conflict_all_commits(true);
  syncer_->SyncShare();
  syncer_->SyncShare();
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    Entry bob(&trans, GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(bob.good());
    Entry fred(&trans, GET_BY_PATH, PSTR("fred"));
    ASSERT_TRUE(fred.good());
    EXPECT_TRUE(fred.Get(IS_UNSYNCED));
    EXPECT_FALSE(fred.Get(ID).ServerKnows());
    EXPECT_TRUE(bob.Get(IS_UNSYNCED));
    EXPECT_TRUE(bob.Get(PARENT_ID) == fred.Get(ID));
    EXPECT_TRUE(fred.Get(PARENT_ID) == root_id_);
    EXPECT_FALSE(fred.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_FALSE(bob.Get(IS_UNAPPLIED_UPDATE));
  }
  syncer_events_.clear();
}

namespace {

int move_bob_count;

bool MoveBobIntoID2(Directory* dir) {
  if (--move_bob_count > 0)
    return false;
  if (move_bob_count == 0) {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    Entry alice(&trans, GET_BY_ID, TestIdFactory::FromNumber(2));
    CHECK(alice.good());
    CHECK(!alice.Get(IS_DEL));
    MutableEntry bob(&trans, GET_BY_ID, TestIdFactory::FromNumber(1));
    CHECK(bob.good());
    bob.Put(IS_UNSYNCED, true);
    bob.Put(PARENT_ID, alice.Get(ID));
    return true;
  }
  return false;
}

}  // namespace

TEST_F(SyncerTest,
       WeMovedSomethingIntoAFolderServerHasDeletedAndWeRenamed) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());
  mock_server_->AddUpdateDirectory(1, 0, "bob", 1, 10);
  mock_server_->AddUpdateDirectory(2, 0, "fred", 1, 10);
  syncer_->SyncShare();
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry fred(&trans, GET_BY_ID, ids_.FromNumber(2));
    ASSERT_TRUE(fred.good());
    fred.Put(IS_UNSYNCED, true);
    fred.Put(NAME, PSTR("Alice"));
  }
  mock_server_->AddUpdateDirectory(2, 0, "fred", 2, 20);
  mock_server_->SetLastUpdateDeleted();
  mock_server_->set_conflict_all_commits(true);
  // This test is a little brittle. We want to move the item into the folder
  // such that we think we're dealing with a simple conflict, but in reality
  // it's actually a conflict set.
  move_bob_count = 2;
  mock_server_->SetMidCommitCallbackFunction(MoveBobIntoID2);
  syncer_->SyncShare();
  syncer_->SyncShare();
  syncer_->SyncShare();
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    Entry bob(&trans, GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(bob.good());
    Entry alice(&trans, GET_BY_PATH, PSTR("Alice"));
    ASSERT_TRUE(alice.good());
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
  mock_server_->AddUpdateDirectory(1, 0, "bob", 1, 10);
  mock_server_->AddUpdateDirectory(2, 0, "fred", 1, 10);
  syncer_->SyncShare();
  syncable::Id new_item_id;
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry bob(&trans, GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(bob.good());
    bob.Put(IS_UNSYNCED, true);
    bob.Put(PARENT_ID, ids_.FromNumber(2));
    MutableEntry new_item(&trans, CREATE, ids_.FromNumber(2), PSTR("new_item"));
    WriteTestDataToEntry(&trans, &new_item);
    new_item_id = new_item.Get(ID);
  }
  mock_server_->AddUpdateDirectory(2, 0, "fred", 2, 20);
  mock_server_->SetLastUpdateDeleted();
  mock_server_->set_conflict_all_commits(true);
  syncer_->SyncShare();
  syncer_->SyncShare();
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    Entry bob(&trans, GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(bob.good());
    Entry fred(&trans, GET_BY_PATH, PSTR("fred"));
    ASSERT_TRUE(fred.good());
    PathChar path[] = {'f', 'r', 'e', 'd', *kPathSeparator,
                       'n', 'e', 'w', '_', 'i', 't', 'e', 'm', 0};
    Entry new_item(&trans, GET_BY_PATH, path);
    EXPECT_TRUE(new_item.good());
    EXPECT_TRUE(fred.Get(IS_UNSYNCED));
    EXPECT_FALSE(fred.Get(ID).ServerKnows());
    EXPECT_TRUE(bob.Get(IS_UNSYNCED));
    EXPECT_TRUE(bob.Get(PARENT_ID) == fred.Get(ID));
    EXPECT_TRUE(fred.Get(PARENT_ID) == root_id_);
    EXPECT_FALSE(fred.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_FALSE(bob.Get(IS_UNAPPLIED_UPDATE));
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
  syncer_->SyncShare();
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
  syncer_->SyncShare();
  syncer_->SyncShare();
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

namespace {

int countown_till_delete = 0;

void DeleteSusanInRoot(Directory* dir) {
  ASSERT_GT(countown_till_delete, 0);
  if (0 != --countown_till_delete)
    return;
  WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
  MutableEntry susan(&trans, GET_BY_PATH, PSTR("susan"));
  Directory::ChildHandles children;
  dir->GetChildHandles(&trans, susan.Get(ID), &children);
  ASSERT_TRUE(0 == children.size());
  susan.Put(IS_DEL, true);
  susan.Put(IS_UNSYNCED, true);
}

}  // namespace

TEST_F(SyncerTest, NewServerItemInAFolderHierarchyWeHaveDeleted3) {
  // Same as 2, except we deleted the folder the set is in between set building
  // and conflict resolution.
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
  countown_till_delete = 2;
  syncer_->pre_conflict_resolution_function_ = DeleteSusanInRoot;
  syncer_->SyncShare();
  syncer_->SyncShare();
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
    EXPECT_FALSE(susan.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_TRUE(fred.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_FALSE(bob.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_FALSE(joe.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_TRUE(susan.Get(IS_UNSYNCED));
    EXPECT_FALSE(fred.Get(IS_UNSYNCED));
    EXPECT_TRUE(bob.Get(IS_UNSYNCED));
    EXPECT_TRUE(joe.Get(IS_UNSYNCED));
  }
  EXPECT_TRUE(0 == countown_till_delete);
  syncer_->pre_conflict_resolution_function_ = 0;
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
  mock_server_->AddUpdateDirectory(1, 0, "bob", 1, 10);
  mock_server_->AddUpdateDirectory(2, 0, "fred", 1, 10);
  mock_server_->AddUpdateDirectory(3, 2, "alice", 1, 10);
  syncer_->SyncShare();
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry bob(&trans, GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(bob.good());
    bob.Put(IS_UNSYNCED, true);
    bob.Put(PARENT_ID, ids_.FromNumber(3));  // Move into alice.
  }
  mock_server_->AddUpdateDirectory(2, 0, "fred", 2, 20);
  mock_server_->SetLastUpdateDeleted();
  mock_server_->AddUpdateDirectory(3, 0, "alice", 2, 20);
  mock_server_->SetLastUpdateDeleted();
  mock_server_->set_conflict_all_commits(true);
  syncer_->SyncShare();
  syncer_->SyncShare();
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    Entry bob(&trans, GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(bob.good());
    Entry fred(&trans, GET_BY_PATH, PSTR("fred"));
    ASSERT_TRUE(fred.good());
    PathChar path[] = {'f', 'r', 'e', 'd', *kPathSeparator,
                       'a', 'l', 'i', 'c', 'e', 0};
    Entry alice(&trans, GET_BY_PATH, path);
    ASSERT_TRUE(alice.good());
    EXPECT_TRUE(fred.Get(IS_UNSYNCED));
    EXPECT_TRUE(alice.Get(IS_UNSYNCED));
    EXPECT_TRUE(bob.Get(IS_UNSYNCED));
    EXPECT_FALSE(fred.Get(ID).ServerKnows());
    EXPECT_FALSE(alice.Get(ID).ServerKnows());
    EXPECT_TRUE(alice.Get(PARENT_ID) == fred.Get(ID));
    EXPECT_TRUE(bob.Get(PARENT_ID) == alice.Get(ID));
    EXPECT_TRUE(fred.Get(PARENT_ID) == root_id_);
    EXPECT_FALSE(fred.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_FALSE(bob.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_FALSE(alice.Get(IS_UNAPPLIED_UPDATE));
  }
  syncer_events_.clear();
}

TEST_F(SyncerTest, WeMovedSomethingIntoAFolderHierarchyServerHasDeleted2) {
  // The difference here is that the hierarchy's not in the root. We have
  // another entry that shouldn't be touched.
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());
  mock_server_->AddUpdateDirectory(1, 0, "bob", 1, 10);
  mock_server_->AddUpdateDirectory(4, 0, "susan", 1, 10);
  mock_server_->AddUpdateDirectory(2, 4, "fred", 1, 10);
  mock_server_->AddUpdateDirectory(3, 2, "alice", 1, 10);
  syncer_->SyncShare();
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry bob(&trans, GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(bob.good());
    bob.Put(IS_UNSYNCED, true);
    bob.Put(PARENT_ID, ids_.FromNumber(3));  // Move into alice.
  }
  mock_server_->AddUpdateDirectory(2, 0, "fred", 2, 20);
  mock_server_->SetLastUpdateDeleted();
  mock_server_->AddUpdateDirectory(3, 0, "alice", 2, 20);
  mock_server_->SetLastUpdateDeleted();
  mock_server_->set_conflict_all_commits(true);
  syncer_->SyncShare();
  syncer_->SyncShare();
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    Entry bob(&trans, GET_BY_ID, ids_.FromNumber(1));
    ASSERT_TRUE(bob.good());
    PathChar path[] = {'s', 'u', 's', 'a', 'n', *kPathSeparator,
                       'f', 'r', 'e', 'd', 0};
    Entry fred(&trans, GET_BY_PATH, path);
    ASSERT_TRUE(fred.good());
    PathChar path2[] = {'s', 'u', 's', 'a', 'n', *kPathSeparator,
                        'f', 'r', 'e', 'd', *kPathSeparator,
                        'a', 'l', 'i', 'c', 'e', 0};
    Entry alice(&trans, GET_BY_PATH, path2);
    ASSERT_TRUE(alice.good());
    Entry susan(&trans, GET_BY_ID, ids_.FromNumber(4));
    ASSERT_TRUE(susan.good());
    Entry susan_by_path(&trans, GET_BY_PATH, PSTR("susan"));
    ASSERT_TRUE(susan.good());
    EXPECT_FALSE(susan.Get(IS_UNSYNCED));
    EXPECT_TRUE(fred.Get(IS_UNSYNCED));
    EXPECT_TRUE(alice.Get(IS_UNSYNCED));
    EXPECT_TRUE(bob.Get(IS_UNSYNCED));
    EXPECT_FALSE(fred.Get(ID).ServerKnows());
    EXPECT_FALSE(alice.Get(ID).ServerKnows());
    EXPECT_TRUE(alice.Get(PARENT_ID) == fred.Get(ID));
    EXPECT_TRUE(bob.Get(PARENT_ID) == alice.Get(ID));
    EXPECT_TRUE(fred.Get(PARENT_ID) == susan.Get(ID));
    EXPECT_TRUE(susan.Get(PARENT_ID) == root_id_);
    EXPECT_FALSE(fred.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_FALSE(bob.Get(IS_UNAPPLIED_UPDATE));
    EXPECT_FALSE(alice.Get(IS_UNAPPLIED_UPDATE));
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
    MutableEntry folder(&trans, CREATE, trans.root_id(), PSTR("bob"));
    ASSERT_TRUE(folder.good());
    folder.Put(IS_UNSYNCED, true);
    folder.Put(IS_DIR, true);
    MutableEntry folder2(&trans, CREATE, trans.root_id(), PSTR("fred"));
    ASSERT_TRUE(folder2.good());
    folder2.Put(IS_UNSYNCED, false);
    folder2.Put(IS_DIR, true);
    folder2.Put(BASE_VERSION, 3);
    folder2.Put(ID, syncable::Id::CreateFromServerId("mock_server:10000"));
  }
  mock_server_->set_next_new_id(10000);
  EXPECT_TRUE(1 == dir->unsynced_entity_count());
  syncer_->SyncShare();  // we get back a bad id in here (should never happen).
  EXPECT_TRUE(1 == dir->unsynced_entity_count());
  syncer_->SyncShare();  // another bad id in here.
  EXPECT_TRUE(0 == dir->unsynced_entity_count());
  syncer_events_.clear();
}

// This test is not very useful anymore. It used to trigger a more interesting
// condition.
TEST_F(SyncerTest, SimpleConflictOnAnEntry) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry bob(&trans, CREATE, trans.root_id(), PSTR("bob"));
    ASSERT_TRUE(bob.good());
    bob.Put(IS_UNSYNCED, true);
    WriteTestDataToEntry(&trans, &bob);
  }
  syncer_->SyncShare();
  syncable::Id bobid;
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry bob(&trans, GET_BY_PATH, PSTR("bob"));
    ASSERT_TRUE(bob.good());
    EXPECT_FALSE(bob.Get(IS_UNSYNCED));
    bob.Put(IS_UNSYNCED, true);
    bobid = bob.Get(ID);
  }
  mock_server_->AddUpdateBookmark(1, 0, "jim", 2, 20);
  mock_server_->set_conflict_all_commits(true);
  SyncRepeatedlyToTriggerConflictResolution(state_.get());
  syncer_events_.clear();
}

TEST_F(SyncerTest, DeletedEntryWithBadParentInLoopCalculation) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  ASSERT_TRUE(dir.good());
  mock_server_->AddUpdateDirectory(1, 0, "bob", 1, 10);
  syncer_->SyncShare();
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
  syncer_->SyncShare();
  syncer_->SyncShare();
}

TEST_F(SyncerTest, ConflictResolverMergeOverwritesLocalEntry) {
  // This test would die because it would rename a entry to a name that was
  // taken in the namespace
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());

  ConflictSet conflict_set;
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);

    MutableEntry local_deleted(&trans, CREATE, trans.root_id(), PSTR("name"));
    local_deleted.Put(ID, ids_.FromNumber(1));
    local_deleted.Put(BASE_VERSION, 1);
    local_deleted.Put(IS_DEL, true);
    local_deleted.Put(IS_UNSYNCED, true);

    MutableEntry in_the_way(&trans, CREATE, trans.root_id(), PSTR("name"));
    in_the_way.Put(ID, ids_.FromNumber(2));
    in_the_way.Put(BASE_VERSION, 1);

    MutableEntry update(&trans, CREATE_NEW_UPDATE_ITEM, ids_.FromNumber(3));
    update.Put(BASE_VERSION, 1);
    update.Put(SERVER_NAME, PSTR("name"));
    update.Put(PARENT_ID, ids_.FromNumber(0));
    update.Put(IS_UNAPPLIED_UPDATE, true);

    conflict_set.push_back(ids_.FromNumber(1));
    conflict_set.push_back(ids_.FromNumber(3));
  }
  {
    SyncCycleState cycle_state;
    SyncerSession session(&cycle_state, state_.get());
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    syncer_->conflict_resolver()->ProcessConflictSet(&trans, &conflict_set, 50,
                                                     &session);
  }
}

TEST_F(SyncerTest, ConflictResolverMergesLocalDeleteAndServerUpdate) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());

  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);

    MutableEntry local_deleted(&trans, CREATE, trans.root_id(), PSTR("name"));
    local_deleted.Put(ID, ids_.FromNumber(1));
    local_deleted.Put(BASE_VERSION, 1);
    local_deleted.Put(IS_DEL, true);
    local_deleted.Put(IS_DIR, false);
    local_deleted.Put(IS_UNSYNCED, true);
    local_deleted.Put(IS_BOOKMARK_OBJECT, true);
  }

  mock_server_->AddUpdateBookmark(ids_.FromNumber(1), root_id_, "name", 10, 10);

  // We don't care about actually committing, just the resolution.
  mock_server_->set_conflict_all_commits(true);
  syncer_->SyncShare();

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

    MutableEntry local_deleted(&trans, CREATE, trans.root_id(), PSTR("name"));
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
  syncer_->SyncShare();

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
  SyncProcessState c;
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
  SyncProcessState identical_set;
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
  syncer_->SyncShare();
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry entry(&trans, CREATE, trans.root_id(), PSTR("Copy of base"));
    WriteTestDataToEntry(&trans, &entry);
  }
  mock_server_->AddUpdateBookmark(1, 0, "Copy of base", 50, 50);
  SyncRepeatedlyToTriggerConflictResolution(state_.get());
}

// In this test a long changelog contains a child at the start of the changelog
// and a parent at the end. While these updates are in progress the client would
// appear stuck.
TEST_F(SyncerTest, LongChangelistCreatesFakeOrphanedEntries) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());
  const int DEPTH = 400;
  // First we an item in a folder in the root. However the folder won't come
  // till much later.
  mock_server_->AddUpdateDirectory(99999, 1, "stuck", 1, 1);
  mock_server_->SetChangesRemaining(DEPTH - 1);
  syncer_->SyncShare(state_.get());

  // Very long changelist. We should never be stuck.
  for (int i = 0; i < DEPTH; i++) {
    mock_server_->SetNewTimestamp(i);
    mock_server_->SetChangesRemaining(DEPTH - i);
    syncer_->SyncShare(state_.get());
    EXPECT_FALSE(SyncerStuck(state_.get()));
  }
  // And finally the folder.
  mock_server_->AddUpdateDirectory(1, 0, "folder", 1, 1);
  mock_server_->SetChangesRemaining(0);
  LoopSyncShare(syncer_);
  LoopSyncShare(syncer_);
  // Check that everything's as expected after the commit.
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    Entry entry(&trans, GET_BY_PATH, PSTR("folder"));
    ASSERT_TRUE(entry.good());
    Entry child(&trans, GET_BY_PARENTID_AND_NAME, entry.Get(ID), PSTR("stuck"));
    EXPECT_TRUE(child.good());
  }
}

TEST_F(SyncerTest, DontMergeTwoExistingItems) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  EXPECT_TRUE(dir.good());
  mock_server_->set_conflict_all_commits(true);
  mock_server_->AddUpdateBookmark(1, 0, "base", 10, 10);
  mock_server_->AddUpdateBookmark(2, 0, "base2", 10, 10);
  syncer_->SyncShare();
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry entry(&trans, GET_BY_ID, ids_.FromNumber(2));
    ASSERT_TRUE(entry.good());
    EXPECT_TRUE(entry.Put(NAME, PSTR("Copy of base")));
    entry.Put(IS_UNSYNCED, true);
  }
  mock_server_->AddUpdateBookmark(1, 0, "Copy of base", 50, 50);
  SyncRepeatedlyToTriggerConflictResolution(state_.get());
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
    EXPECT_NE(entry1.Get(NAME), entry2.Get(NAME));
  }
}

TEST_F(SyncerTest, TestUndeleteUpdate) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  EXPECT_TRUE(dir.good());
  mock_server_->set_conflict_all_commits(true);
  mock_server_->AddUpdateDirectory(1, 0, "foo", 1, 1);
  mock_server_->AddUpdateDirectory(2, 1, "bar", 1, 2);
  syncer_->SyncShare();
  mock_server_->AddUpdateDirectory(2, 1, "bar", 2, 3);
  mock_server_->SetLastUpdateDeleted();
  syncer_->SyncShare();
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    Entry entry(&trans, GET_BY_ID, ids_.FromNumber(2));
    ASSERT_TRUE(entry.good());
    EXPECT_TRUE(entry.Get(IS_DEL));
  }
  mock_server_->AddUpdateDirectory(1, 0, "foo", 2, 4);
  mock_server_->SetLastUpdateDeleted();
  syncer_->SyncShare();
  // This used to be rejected as it's an undeletion. Now, it results in moving
  // the delete path aside.
  mock_server_->AddUpdateDirectory(2, 1, "bar", 3, 5);
  syncer_->SyncShare();
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
  syncer_->SyncShare();
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry entry(&trans, GET_BY_ID, ids_.FromNumber(2));
    ASSERT_TRUE(entry.good());
    EXPECT_TRUE(entry.Put(PARENT_ID, ids_.FromNumber(1)));
    EXPECT_TRUE(entry.Put(IS_UNSYNCED, true));
  }
  syncer_->SyncShare();
  // We use the same sync ts as before so our times match up.
  mock_server_->AddUpdateDirectory(2, 1, ":::", 2, 2);
  syncer_->SyncShare();
}

TEST_F(SyncerTest, QuicklyMergeDualCreatedHierarchy) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  EXPECT_TRUE(dir.good());
  mock_server_->set_conflict_all_commits(true);
  int depth = 10;
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    syncable::Id parent = root_id_;
    for (int i = 0 ; i < depth ; ++i) {
      MutableEntry entry(&trans, CREATE, parent, PSTR("folder"));
      entry.Put(IS_DIR, true);
      entry.Put(IS_UNSYNCED, true);
      parent = entry.Get(ID);
    }
  }
  for (int i = 0 ; i < depth ; ++i) {
    mock_server_->AddUpdateDirectory(i + 1, i, "folder", 1, 1);
  }
  syncer_->SyncShare(state_.get());
  syncer_->SyncShare(state_.get());
  SyncerStatus status(NULL, state_.get());
  EXPECT_LT(status.consecutive_problem_commits(), 5);
  EXPECT_TRUE(0 == dir->unsynced_entity_count());
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
  syncer_->SyncShare();
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
  mock_server_->AddUpdateDirectory("in_root_id", kRootId,
                                   "in_root_name", 2, 2);
  mock_server_->AddUpdateDirectory("in_in_root_id", "in_root_id",
                                   "in_in_root_name", 3, 3);
  syncer_->SyncShare();
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    // Entry will have been dropped.
    Entry by_path(&trans, GET_BY_PATH, PSTR("in_root_name"));
    EXPECT_TRUE(by_path.good());
    Entry by_path2(&trans, GET_BY_PATH, PSTR("in_root_name") +
                                        PathString(kPathSeparator) +
                                        PSTR("in_in_root_name"));
    EXPECT_TRUE(by_path2.good());
  }
}

TEST_F(SyncerTest, DirectoryCommitTest) {
  syncable::Id in_root, in_dir;
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());
  {
    WriteTransaction wtrans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry parent(&wtrans, syncable::CREATE, root_id_, PSTR("foo"));
    ASSERT_TRUE(parent.good());
    parent.Put(syncable::IS_UNSYNCED, true);
    parent.Put(syncable::IS_DIR, true);
    in_root = parent.Get(syncable::ID);
    MutableEntry child(&wtrans, syncable::CREATE, parent.Get(ID), PSTR("bar"));
    ASSERT_TRUE(child.good());
    child.Put(syncable::IS_UNSYNCED, true);
    child.Put(syncable::IS_DIR, true);
    in_dir = parent.Get(syncable::ID);
  }
  syncer_->SyncShare();
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    Entry by_path(&trans, GET_BY_PATH, PSTR("foo"));
    ASSERT_TRUE(by_path.good());
    EXPECT_NE(by_path.Get(syncable::ID), in_root);
    Entry by_path2(&trans, GET_BY_PATH, PSTR("foo") +
                                        PathString(kPathSeparator) +
                                        PSTR("bar"));
    ASSERT_TRUE(by_path2.good());
    EXPECT_NE(by_path2.Get(syncable::ID), in_dir);
  }
}

namespace {

void CheckEntryVersion(syncable::DirectoryManager* dirmgr, PathString name) {
  ScopedDirLookup dir(dirmgr, name);
  ASSERT_TRUE(dir.good());
  ReadTransaction trans(dir, __FILE__, __LINE__);
  Entry entry(&trans, GET_BY_PATH, PSTR("foo"));
  ASSERT_TRUE(entry.good());
  EXPECT_TRUE(entry.Get(BASE_VERSION) == 1);
}

}  // namespace

TEST_F(SyncerTest, ConflictSetSizeReducedToOne) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());
  mock_server_->AddUpdateBookmark(2, 0, "in_root", 1, 1);
  syncer_->SyncShare();
  {
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry oentry(&trans, GET_BY_PATH, PSTR("in_root"));
    ASSERT_TRUE(oentry.good());
    oentry.Put(NAME, PSTR("old_in_root"));
    WriteTestDataToEntry(&trans, &oentry);
    MutableEntry entry(&trans, CREATE, trans.root_id(), PSTR("in_root"));
    ASSERT_TRUE(entry.good());
    WriteTestDataToEntry(&trans, &entry);
  }
  mock_server_->set_conflict_all_commits(true);
  // This SyncShare call used to result in a CHECK failure.
  syncer_->SyncShare();
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
  syncer_->SyncShare();

  EXPECT_TRUE(last_client_command_.has_set_sync_poll_interval());
  EXPECT_TRUE(last_client_command_.has_set_sync_long_poll_interval());
  EXPECT_TRUE(8 == last_client_command_.set_sync_poll_interval());
  EXPECT_TRUE(800 == last_client_command_.set_sync_long_poll_interval());

  command = mock_server_->GetNextClientCommand();
  command->set_set_sync_poll_interval(180);
  command->set_set_sync_long_poll_interval(190);
  mock_server_->AddUpdateDirectory(1, 0, "in_root", 1, 1);
  syncer_->SyncShare();

  EXPECT_TRUE(last_client_command_.has_set_sync_poll_interval());
  EXPECT_TRUE(last_client_command_.has_set_sync_long_poll_interval());
  EXPECT_TRUE(180 == last_client_command_.set_sync_poll_interval());
  EXPECT_TRUE(190 == last_client_command_.set_sync_long_poll_interval());
}

TEST_F(SyncerTest, EnsureWeSendUpOldParent) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());
  mock_server_->AddUpdateDirectory(1, 0, "folder_one", 1, 1);
  mock_server_->AddUpdateDirectory(2, 0, "folder_two", 1, 1);
  syncer_->SyncShare();
  {
    // A moved entry should send an old parent.
    WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry entry(&trans, GET_BY_PATH, PSTR("folder_one"));
    ASSERT_TRUE(entry.good());
    entry.Put(PARENT_ID, ids_.FromNumber(2));
    entry.Put(IS_UNSYNCED, true);
    // A new entry should send no parent.
    MutableEntry create(&trans, CREATE, trans.root_id(), PSTR("new_folder"));
    create.Put(IS_UNSYNCED, true);
  }
  syncer_->SyncShare();
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
  const PathString name(PSTR("ringo's dang orang ran rings around my o-ring"));

  // Try writing max int64 to the version fields of a meta entry.
  {
    WriteTransaction wtrans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry entry(&wtrans, syncable::CREATE, wtrans.root_id(), name);
    ASSERT_TRUE(entry.good());
    entry.Put(syncable::BASE_VERSION, really_big_int);
    entry.Put(syncable::SERVER_VERSION, really_big_int);
    entry.Put(syncable::ID, syncable::Id::CreateFromServerId("ID"));
  }
  // Now read it back out and make sure the value is max int64.
  ReadTransaction rtrans(dir, __FILE__, __LINE__);
  Entry entry(&rtrans, syncable::GET_BY_PATH, name);
  ASSERT_TRUE(entry.good());
  EXPECT_TRUE(really_big_int == entry.Get(syncable::BASE_VERSION));
}

TEST_F(SyncerTest, TestDSStoreDirectorySyncsNormally) {
  syncable::Id item_id = parent_id_;
  mock_server_->AddUpdateDirectory(item_id,
      root_id_, ".DS_Store", 1, 1);
  syncer_->SyncShare();
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  CHECK(dir.good());
  ReadTransaction trans(dir, __FILE__, __LINE__);
  Entry ds_dir(&trans, GET_BY_PATH, PSTR(".DS_Store"));
  ASSERT_TRUE(ds_dir.good());
}

TEST_F(SyncerTest, TestSimpleUndelete) {
  Id id = ids_.MakeServer("undeletion item"), root = ids_.root();
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  EXPECT_TRUE(dir.good());
  mock_server_->set_conflict_all_commits(true);
  // Let there be an entry from the server.
  mock_server_->AddUpdateBookmark(id, root, "foo", 1, 10);
  syncer_->SyncShare();
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
  syncer_->SyncShare();
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
  syncer_->SyncShare();
  // Update from server confirming deletion.
  mock_server_->AddUpdateBookmark(id, root, "foo", 2, 11);
  mock_server_->SetLastUpdateDeleted();
  syncer_->SyncShare();
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
  syncer_->SyncShare();
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
  Id id = ids_.MakeServer("undeletion item"), root = ids_.root();
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  EXPECT_TRUE(dir.good());
  // Let there be a entry, from the server.
  mock_server_->set_conflict_all_commits(true);
  mock_server_->AddUpdateBookmark(id, root, "foo", 1, 10);
  syncer_->SyncShare();
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
  syncer_->SyncShare();
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
  syncer_->SyncShare();
  // Say we do not get an update from server confirming deletion. Undelete
  // from server
  mock_server_->AddUpdateBookmark(id, root, "foo", 2, 12);
  syncer_->SyncShare();
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
  Id root = ids_.root();
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  EXPECT_TRUE(dir.good());
  // Duplicate! expect path clashing!
  mock_server_->set_conflict_all_commits(true);
  mock_server_->AddUpdateBookmark(id1, root, "foo", 1, 10);
  mock_server_->AddUpdateBookmark(id2, root, "foo", 1, 10);
  syncer_->SyncShare();
  mock_server_->AddUpdateBookmark(id2, root, "foo2", 1, 10);
  syncer_->SyncShare();  // Now just don't explode.
}

TEST_F(SyncerTest, CopySyncProcessState) {
  scoped_ptr<SyncProcessState> b;
  {
    SyncProcessState a;
    a.MergeSets(ids_.FromNumber(1), ids_.FromNumber(2));
    a.MergeSets(ids_.FromNumber(2), ids_.FromNumber(3));
    a.MergeSets(ids_.FromNumber(4), ids_.FromNumber(5));
    EXPECT_TRUE(a.ConflictSetsSize() == 2);
    {
      SyncProcessState b = a;
      b = b;
      EXPECT_TRUE(b.ConflictSetsSize() == 2);
    }
    EXPECT_TRUE(a.ConflictSetsSize() == 2);
    a.MergeSets(ids_.FromNumber(3), ids_.FromNumber(4));
    EXPECT_TRUE(a.ConflictSetsSize() == 1);
    b.reset(new SyncProcessState(a));
  }
  EXPECT_TRUE(b->ConflictSetsSize() == 1);
}

TEST_F(SyncerTest, SingletonTagUpdates) {
  ScopedDirLookup dir(syncdb_.manager(), syncdb_.name());
  EXPECT_TRUE(dir.good());
  // As a hurdle, introduce an item whose name is the same as the tag value
  // we'll use later.
  int64 hurdle_handle = CreateUnsyncedDirectory(PSTR("bob"), "id_bob");
  {
    ReadTransaction trans(dir, __FILE__, __LINE__);
    Entry hurdle(&trans, GET_BY_HANDLE, hurdle_handle);
    ASSERT_TRUE(hurdle.good());
    ASSERT_TRUE(!hurdle.Get(IS_DEL));
    ASSERT_TRUE(hurdle.Get(SINGLETON_TAG).empty());
    ASSERT_TRUE(hurdle.GetName().value() == PSTR("bob"));

    // Try to lookup by the tagname.  These should fail.
    Entry tag_alpha(&trans, GET_BY_TAG, PSTR("alpha"));
    EXPECT_FALSE(tag_alpha.good());
    Entry tag_bob(&trans, GET_BY_TAG, PSTR("bob"));
    EXPECT_FALSE(tag_bob.good());
  }

  // Now download some tagged items as updates.
  mock_server_->AddUpdateDirectory(1, 0, "update1", 1, 10);
  mock_server_->SetLastUpdateSingletonTag("alpha");
  mock_server_->AddUpdateDirectory(2, 0, "update2", 2, 20);
  mock_server_->SetLastUpdateSingletonTag("bob");
  syncer_->SyncShare();

  {
    ReadTransaction trans(dir, __FILE__, __LINE__);

    // The new items should be applied as new entries, and we should be able
    // to look them up by their tag values.
    Entry tag_alpha(&trans, GET_BY_TAG, PSTR("alpha"));
    ASSERT_TRUE(tag_alpha.good());
    ASSERT_TRUE(!tag_alpha.Get(IS_DEL));
    ASSERT_TRUE(tag_alpha.Get(SINGLETON_TAG) == PSTR("alpha"));
    ASSERT_TRUE(tag_alpha.GetName().value() == PSTR("update1"));
    Entry tag_bob(&trans, GET_BY_TAG, PSTR("bob"));
    ASSERT_TRUE(tag_bob.good());
    ASSERT_TRUE(!tag_bob.Get(IS_DEL));
    ASSERT_TRUE(tag_bob.Get(SINGLETON_TAG) == PSTR("bob"));
    ASSERT_TRUE(tag_bob.GetName().value() == PSTR("update2"));
    // The old item should be unchanged.
    Entry hurdle(&trans, GET_BY_HANDLE, hurdle_handle);
    ASSERT_TRUE(hurdle.good());
    ASSERT_TRUE(!hurdle.Get(IS_DEL));
    ASSERT_TRUE(hurdle.Get(SINGLETON_TAG).empty());
    ASSERT_TRUE(hurdle.GetName().value() == PSTR("bob"));
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

  syncer_->SyncShare();
  ExpectLocalItemsInServerOrder();
}

TEST_F(SyncerPositionUpdateTest, InOrderNegative) {
  // Test negative position values, but in increasing order.
  AddRootItemWithPosition(-400);
  AddRootItemWithPosition(-201);
  AddRootItemWithPosition(-200);
  AddRootItemWithPosition(-150);
  AddRootItemWithPosition(100);

  syncer_->SyncShare();
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

  syncer_->SyncShare();
  ExpectLocalItemsInServerOrder();
}

TEST_F(SyncerPositionUpdateTest, RandomOrderInBatches) {
  // Mix it all up, interleaving position values, and try multiple batches of
  // updates.
  AddRootItemWithPosition(400);
  AddRootItemWithPosition(201);
  AddRootItemWithPosition(-400);
  AddRootItemWithPosition(100);

  syncer_->SyncShare();
  ExpectLocalItemsInServerOrder();

  AddRootItemWithPosition(-150);
  AddRootItemWithPosition(-200);
  AddRootItemWithPosition(200);
  AddRootItemWithPosition(-201);

  syncer_->SyncShare();
  ExpectLocalItemsInServerOrder();

  AddRootItemWithPosition(-144);

  syncer_->SyncShare();
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
  syncer_->SyncShare();
  ExpectLocalOrderIsByServerId();
}

TEST_F(SyncerPositionTiebreakingTest, LowHighMid) {
  Add(low_id_);
  Add(high_id_);
  Add(mid_id_);
  syncer_->SyncShare();
  ExpectLocalOrderIsByServerId();
}

TEST_F(SyncerPositionTiebreakingTest, HighMidLow) {
  Add(high_id_);
  Add(mid_id_);
  Add(low_id_);
  syncer_->SyncShare();
  ExpectLocalOrderIsByServerId();
}

TEST_F(SyncerPositionTiebreakingTest, HighLowMid) {
  Add(high_id_);
  Add(low_id_);
  Add(mid_id_);
  syncer_->SyncShare();
  ExpectLocalOrderIsByServerId();
}

TEST_F(SyncerPositionTiebreakingTest, MidHighLow) {
  Add(mid_id_);
  Add(high_id_);
  Add(low_id_);
  syncer_->SyncShare();
  ExpectLocalOrderIsByServerId();
}

TEST_F(SyncerPositionTiebreakingTest, MidLowHigh) {
  Add(mid_id_);
  Add(low_id_);
  Add(high_id_);
  syncer_->SyncShare();
  ExpectLocalOrderIsByServerId();
}

const SyncerTest::CommitOrderingTest
SyncerTest::CommitOrderingTest::LAST_COMMIT_ITEM = {-1, TestIdFactory::root()};

}  // namespace browser_sync
