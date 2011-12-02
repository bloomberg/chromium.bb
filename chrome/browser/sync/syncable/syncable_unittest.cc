// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/syncable/syncable.h"

#include <string>

#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/scoped_temp_dir.h"
#include "base/stringprintf.h"
#include "base/synchronization/condition_variable.h"
#include "base/threading/platform_thread.h"
#include "base/values.h"
#include "chrome/browser/sync/engine/syncproto.h"
#include "chrome/browser/sync/protocol/bookmark_specifics.pb.h"
#include "chrome/browser/sync/syncable/directory_backing_store.h"
#include "chrome/browser/sync/syncable/directory_change_delegate.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/browser/sync/test/engine/test_id_factory.h"
#include "chrome/browser/sync/test/engine/test_syncable_utils.h"
#include "chrome/browser/sync/test/null_directory_change_delegate.h"
#include "chrome/browser/sync/test/null_transaction_observer.h"
#include "chrome/test/base/values_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/sqlite/sqlite3.h"

using browser_sync::TestIdFactory;
using test::ExpectDictBooleanValue;
using test::ExpectDictStringValue;

namespace syncable {

class SyncableKernelTest : public testing::Test {};

// TODO(akalin): Add unit tests for EntryKernel::ContainsString().

TEST_F(SyncableKernelTest, ToValue) {
  EntryKernel kernel;
  scoped_ptr<DictionaryValue> value(kernel.ToValue());
  if (value.get()) {
    // Not much to check without repeating the ToValue() code.
    EXPECT_TRUE(value->HasKey("isDirty"));
    // The extra +1 is for "isDirty".
    EXPECT_EQ(BIT_TEMPS_END - BEGIN_FIELDS + 1,
              static_cast<int>(value->size()));
  } else {
    ADD_FAILURE();
  }
}

namespace {
void PutDataAsBookmarkFavicon(WriteTransaction* wtrans,
                              MutableEntry* e,
                              const char* bytes,
                              size_t bytes_length) {
  sync_pb::EntitySpecifics specifics;
  specifics.MutableExtension(sync_pb::bookmark)->set_url("http://demo/");
  specifics.MutableExtension(sync_pb::bookmark)->set_favicon(bytes,
      bytes_length);
  e->Put(SPECIFICS, specifics);
}

void ExpectDataFromBookmarkFaviconEquals(BaseTransaction* trans,
                                         Entry* e,
                                         const char* bytes,
                                         size_t bytes_length) {
  ASSERT_TRUE(e->good());
  ASSERT_TRUE(e->Get(SPECIFICS).HasExtension(sync_pb::bookmark));
  ASSERT_EQ("http://demo/",
      e->Get(SPECIFICS).GetExtension(sync_pb::bookmark).url());
  ASSERT_EQ(std::string(bytes, bytes_length),
      e->Get(SPECIFICS).GetExtension(sync_pb::bookmark).favicon());
}
}  // namespace

class SyncableGeneralTest : public testing::Test {
 public:
  virtual void SetUp() {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_path_ = temp_dir_.path().Append(
        FILE_PATH_LITERAL("SyncableTest.sqlite3"));
  }

  virtual void TearDown() {
  }
 protected:
  MessageLoop message_loop_;
  ScopedTempDir temp_dir_;
  NullDirectoryChangeDelegate delegate_;
  FilePath db_path_;
};

TEST_F(SyncableGeneralTest, General) {
  Directory dir;
  dir.Open(db_path_, "SimpleTest", &delegate_, NullTransactionObserver());

  int64 root_metahandle;
  {
    ReadTransaction rtrans(FROM_HERE, &dir);
    Entry e(&rtrans, GET_BY_ID, rtrans.root_id());
    ASSERT_TRUE(e.good());
    root_metahandle = e.Get(META_HANDLE);
  }

  int64 written_metahandle;
  const Id id = TestIdFactory::FromNumber(99);
  std::string name = "Jeff";
  // Test simple read operations on an empty DB.
  {
    ReadTransaction rtrans(FROM_HERE, &dir);
    Entry e(&rtrans, GET_BY_ID, id);
    ASSERT_FALSE(e.good());  // Hasn't been written yet.

    Directory::ChildHandles child_handles;
    dir.GetChildHandlesById(&rtrans, rtrans.root_id(), &child_handles);
    EXPECT_TRUE(child_handles.empty());

    dir.GetChildHandlesByHandle(&rtrans, root_metahandle, &child_handles);
    EXPECT_TRUE(child_handles.empty());
  }

  // Test creating a new meta entry.
  {
    WriteTransaction wtrans(FROM_HERE, UNITTEST, &dir);
    MutableEntry me(&wtrans, CREATE, wtrans.root_id(), name);
    ASSERT_TRUE(me.good());
    me.Put(ID, id);
    me.Put(BASE_VERSION, 1);
    written_metahandle = me.Get(META_HANDLE);
  }

  // Test GetChildHandles* after something is now in the DB.
  // Also check that GET_BY_ID works.
  {
    ReadTransaction rtrans(FROM_HERE, &dir);
    Entry e(&rtrans, GET_BY_ID, id);
    ASSERT_TRUE(e.good());

    Directory::ChildHandles child_handles;
    dir.GetChildHandlesById(&rtrans, rtrans.root_id(), &child_handles);
    EXPECT_EQ(1u, child_handles.size());

    for (Directory::ChildHandles::iterator i = child_handles.begin();
         i != child_handles.end(); ++i) {
      EXPECT_EQ(*i, written_metahandle);
    }

    dir.GetChildHandlesByHandle(&rtrans, root_metahandle, &child_handles);
    EXPECT_EQ(1u, child_handles.size());

    for (Directory::ChildHandles::iterator i = child_handles.begin();
         i != child_handles.end(); ++i) {
      EXPECT_EQ(*i, written_metahandle);
    }
  }

  // Test writing data to an entity. Also check that GET_BY_HANDLE works.
  static const char s[] = "Hello World.";
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, &dir);
    MutableEntry e(&trans, GET_BY_HANDLE, written_metahandle);
    ASSERT_TRUE(e.good());
    PutDataAsBookmarkFavicon(&trans, &e, s, sizeof(s));
  }

  // Test reading back the contents that we just wrote.
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, &dir);
    MutableEntry e(&trans, GET_BY_HANDLE, written_metahandle);
    ASSERT_TRUE(e.good());
    ExpectDataFromBookmarkFaviconEquals(&trans, &e, s, sizeof(s));
  }

  // Verify it exists in the folder.
  {
    ReadTransaction rtrans(FROM_HERE, &dir);
    EXPECT_EQ(1, CountEntriesWithName(&rtrans, rtrans.root_id(), name));
  }

  // Now delete it.
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, &dir);
    MutableEntry e(&trans, GET_BY_HANDLE, written_metahandle);
    e.Put(IS_DEL, true);

    EXPECT_EQ(0, CountEntriesWithName(&trans, trans.root_id(), name));
  }

  dir.SaveChanges();
}

TEST_F(SyncableGeneralTest, ChildrenOps) {
  Directory dir;
  dir.Open(db_path_, "SimpleTest", &delegate_, NullTransactionObserver());

  int64 root_metahandle;
  {
    ReadTransaction rtrans(FROM_HERE, &dir);
    Entry e(&rtrans, GET_BY_ID, rtrans.root_id());
    ASSERT_TRUE(e.good());
    root_metahandle = e.Get(META_HANDLE);
  }

  int64 written_metahandle;
  const Id id = TestIdFactory::FromNumber(99);
  std::string name = "Jeff";
  {
    ReadTransaction rtrans(FROM_HERE, &dir);
    Entry e(&rtrans, GET_BY_ID, id);
    ASSERT_FALSE(e.good());  // Hasn't been written yet.

    EXPECT_FALSE(dir.HasChildren(&rtrans, rtrans.root_id()));
    Id child_id;
    EXPECT_TRUE(dir.GetFirstChildId(&rtrans, rtrans.root_id(), &child_id));
    EXPECT_TRUE(child_id.IsRoot());
  }

  {
    WriteTransaction wtrans(FROM_HERE, UNITTEST, &dir);
    MutableEntry me(&wtrans, CREATE, wtrans.root_id(), name);
    ASSERT_TRUE(me.good());
    me.Put(ID, id);
    me.Put(BASE_VERSION, 1);
    written_metahandle = me.Get(META_HANDLE);
  }

  // Test children ops after something is now in the DB.
  {
    ReadTransaction rtrans(FROM_HERE, &dir);
    Entry e(&rtrans, GET_BY_ID, id);
    ASSERT_TRUE(e.good());

    Entry child(&rtrans, GET_BY_HANDLE, written_metahandle);
    ASSERT_TRUE(child.good());

    EXPECT_TRUE(dir.HasChildren(&rtrans, rtrans.root_id()));
    Id child_id;
    EXPECT_TRUE(dir.GetFirstChildId(&rtrans, rtrans.root_id(), &child_id));
    EXPECT_EQ(e.Get(ID), child_id);
  }

  {
    WriteTransaction wtrans(FROM_HERE, UNITTEST, &dir);
    MutableEntry me(&wtrans, GET_BY_HANDLE, written_metahandle);
    ASSERT_TRUE(me.good());
    me.Put(IS_DEL, true);
  }

  // Test children ops after the children have been deleted.
  {
    ReadTransaction rtrans(FROM_HERE, &dir);
    Entry e(&rtrans, GET_BY_ID, id);
    ASSERT_TRUE(e.good());

    EXPECT_FALSE(dir.HasChildren(&rtrans, rtrans.root_id()));
    Id child_id;
    EXPECT_TRUE(dir.GetFirstChildId(&rtrans, rtrans.root_id(), &child_id));
    EXPECT_TRUE(child_id.IsRoot());
  }

  dir.SaveChanges();
}

TEST_F(SyncableGeneralTest, ClientIndexRebuildsProperly) {
  int64 written_metahandle;
  TestIdFactory factory;
  const Id id = factory.NewServerId();
  std::string name = "cheesepuffs";
  std::string tag = "dietcoke";

  // Test creating a new meta entry.
  {
    Directory dir;
    dir.Open(db_path_, "IndexTest", &delegate_, NullTransactionObserver());
    {
      WriteTransaction wtrans(FROM_HERE, UNITTEST, &dir);
      MutableEntry me(&wtrans, CREATE, wtrans.root_id(), name);
      ASSERT_TRUE(me.good());
      me.Put(ID, id);
      me.Put(BASE_VERSION, 1);
      me.Put(UNIQUE_CLIENT_TAG, tag);
      written_metahandle = me.Get(META_HANDLE);
    }
    dir.SaveChanges();
  }

  // The DB was closed. Now reopen it. This will cause index regeneration.
  {
    Directory dir;
    dir.Open(db_path_, "IndexTest", &delegate_, NullTransactionObserver());

    ReadTransaction trans(FROM_HERE, &dir);
    Entry me(&trans, GET_BY_CLIENT_TAG, tag);
    ASSERT_TRUE(me.good());
    EXPECT_EQ(me.Get(ID), id);
    EXPECT_EQ(me.Get(BASE_VERSION), 1);
    EXPECT_EQ(me.Get(UNIQUE_CLIENT_TAG), tag);
    EXPECT_EQ(me.Get(META_HANDLE), written_metahandle);
  }
}

TEST_F(SyncableGeneralTest, ClientIndexRebuildsDeletedProperly) {
  TestIdFactory factory;
  const Id id = factory.NewServerId();
  std::string tag = "dietcoke";

  // Test creating a deleted, unsynced, server meta entry.
  {
    Directory dir;
    dir.Open(db_path_, "IndexTest", &delegate_, NullTransactionObserver());
    {
      WriteTransaction wtrans(FROM_HERE, UNITTEST, &dir);
      MutableEntry me(&wtrans, CREATE, wtrans.root_id(), "deleted");
      ASSERT_TRUE(me.good());
      me.Put(ID, id);
      me.Put(BASE_VERSION, 1);
      me.Put(UNIQUE_CLIENT_TAG, tag);
      me.Put(IS_DEL, true);
      me.Put(IS_UNSYNCED, true);  // Or it might be purged.
    }
    dir.SaveChanges();
  }

  // The DB was closed. Now reopen it. This will cause index regeneration.
  // Should still be present and valid in the client tag index.
  {
    Directory dir;
    dir.Open(db_path_, "IndexTest", &delegate_, NullTransactionObserver());

    ReadTransaction trans(FROM_HERE, &dir);
    Entry me(&trans, GET_BY_CLIENT_TAG, tag);
    ASSERT_TRUE(me.good());
    EXPECT_EQ(me.Get(ID), id);
    EXPECT_EQ(me.Get(UNIQUE_CLIENT_TAG), tag);
    EXPECT_TRUE(me.Get(IS_DEL));
    EXPECT_TRUE(me.Get(IS_UNSYNCED));
  }
}

TEST_F(SyncableGeneralTest, ToValue) {
  Directory dir;
  dir.Open(db_path_, "SimpleTest", &delegate_, NullTransactionObserver());

  const Id id = TestIdFactory::FromNumber(99);
  {
    ReadTransaction rtrans(FROM_HERE, &dir);
    Entry e(&rtrans, GET_BY_ID, id);
    EXPECT_FALSE(e.good());  // Hasn't been written yet.

    scoped_ptr<DictionaryValue> value(e.ToValue());
    ExpectDictBooleanValue(false, *value, "good");
    EXPECT_EQ(1u, value->size());
  }

  // Test creating a new meta entry.
  {
    WriteTransaction wtrans(FROM_HERE, UNITTEST, &dir);
    MutableEntry me(&wtrans, CREATE, wtrans.root_id(), "new");
    ASSERT_TRUE(me.good());
    me.Put(ID, id);
    me.Put(BASE_VERSION, 1);

    scoped_ptr<DictionaryValue> value(me.ToValue());
    ExpectDictBooleanValue(true, *value, "good");
    EXPECT_TRUE(value->HasKey("kernel"));
    ExpectDictStringValue("Unspecified", *value, "serverModelType");
    ExpectDictStringValue("Unspecified", *value, "modelType");
    ExpectDictBooleanValue(true, *value, "existsOnClientBecauseNameIsNonEmpty");
    ExpectDictBooleanValue(false, *value, "isRoot");
  }

  dir.SaveChanges();
}

// A Directory whose backing store always fails SaveChanges by returning false.
class TestUnsaveableDirectory : public Directory {
 public:
  class UnsaveableBackingStore : public DirectoryBackingStore {
   public:
     UnsaveableBackingStore(const std::string& dir_name,
                            const FilePath& backing_filepath)
         : DirectoryBackingStore(dir_name, backing_filepath) { }
     virtual bool SaveChanges(const Directory::SaveChangesSnapshot& snapshot) {
       return false;
     }
  };
  virtual DirectoryBackingStore* CreateBackingStore(
      const std::string& dir_name,
      const FilePath& backing_filepath) {
    return new UnsaveableBackingStore(dir_name, backing_filepath);
  }
};

// Test suite for syncable::Directory.
class SyncableDirectoryTest : public testing::Test {
 protected:
  MessageLoop message_loop_;
  ScopedTempDir temp_dir_;
  static const char kName[];
  static const Id kId;

  // SetUp() is called before each test case is run.
  // The sqlite3 DB is deleted before each test is run.
  virtual void SetUp() {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    file_path_ = temp_dir_.path().Append(
        FILE_PATH_LITERAL("Test.sqlite3"));
    file_util::Delete(file_path_, true);
    dir_.reset(new Directory());
    ASSERT_TRUE(dir_.get());
    ASSERT_EQ(OPENED, dir_->Open(file_path_, kName,
                                 &delegate_, NullTransactionObserver()));
    ASSERT_TRUE(dir_->good());
  }

  virtual void TearDown() {
    // This also closes file handles.
    dir_->SaveChanges();
    dir_.reset();
    file_util::Delete(file_path_, true);
  }

  void ReloadDir() {
    dir_.reset(new Directory());
    ASSERT_TRUE(dir_.get());
    ASSERT_EQ(OPENED, dir_->Open(file_path_, kName,
                                 &delegate_, NullTransactionObserver()));
  }

  void SaveAndReloadDir() {
    dir_->SaveChanges();
    ReloadDir();
  }

  bool IsInDirtyMetahandles(int64 metahandle) {
    return 1 == dir_->kernel_->dirty_metahandles->count(metahandle);
  }

  bool IsInMetahandlesToPurge(int64 metahandle) {
    return 1 == dir_->kernel_->metahandles_to_purge->count(metahandle);
  }

  void CheckPurgeEntriesWithTypeInSucceeded(const ModelTypeSet& types_to_purge,
                                            bool before_reload) {
    SCOPED_TRACE(testing::Message("Before reload: ") << before_reload);
    {
      ReadTransaction trans(FROM_HERE, dir_.get());
      MetahandleSet all_set;
      dir_->GetAllMetaHandles(&trans, &all_set);
      EXPECT_EQ(3U, all_set.size());
      if (before_reload)
        EXPECT_EQ(4U, dir_->kernel_->metahandles_to_purge->size());
      for (MetahandleSet::iterator iter = all_set.begin();
           iter != all_set.end(); ++iter) {
        Entry e(&trans, GET_BY_HANDLE, *iter);
        if ((types_to_purge.count(e.GetModelType()) ||
             types_to_purge.count(e.GetServerModelType()))) {
          FAIL() << "Illegal type should have been deleted.";
        }
      }
    }

    EXPECT_FALSE(dir_->initial_sync_ended_for_type(PREFERENCES));
    EXPECT_FALSE(dir_->initial_sync_ended_for_type(AUTOFILL));
    EXPECT_TRUE(dir_->initial_sync_ended_for_type(BOOKMARKS));
  }

  scoped_ptr<Directory> dir_;
  FilePath file_path_;
  NullDirectoryChangeDelegate delegate_;

  // Creates an empty entry and sets the ID field to the default kId.
  void CreateEntry(const std::string& entryname) {
    CreateEntry(entryname, kId);
  }

  // Creates an empty entry and sets the ID field to id.
  void CreateEntry(const std::string& entryname, const int id) {
    CreateEntry(entryname, TestIdFactory::FromNumber(id));
  }
  void CreateEntry(const std::string& entryname, Id id) {
    WriteTransaction wtrans(FROM_HERE, UNITTEST, dir_.get());
    MutableEntry me(&wtrans, CREATE, wtrans.root_id(), entryname);
    ASSERT_TRUE(me.good());
    me.Put(ID, id);
    me.Put(IS_UNSYNCED, true);
  }

  void ValidateEntry(BaseTransaction* trans,
                     int64 id,
                     bool check_name,
                     const std::string& name,
                     int64 base_version,
                     int64 server_version,
                     bool is_del);
};

TEST_F(SyncableDirectoryTest, TakeSnapshotGetsMetahandlesToPurge) {
  const int metas_to_create = 50;
  MetahandleSet expected_purges;
  MetahandleSet all_handles;
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir_.get());
    for (int i = 0; i < metas_to_create; i++) {
      MutableEntry e(&trans, CREATE, trans.root_id(), "foo");
      e.Put(IS_UNSYNCED, true);
      sync_pb::EntitySpecifics specs;
      if (i % 2 == 0) {
        AddDefaultExtensionValue(BOOKMARKS, &specs);
        expected_purges.insert(e.Get(META_HANDLE));
        all_handles.insert(e.Get(META_HANDLE));
      } else {
        AddDefaultExtensionValue(PREFERENCES, &specs);
        all_handles.insert(e.Get(META_HANDLE));
      }
      e.Put(SPECIFICS, specs);
      e.Put(SERVER_SPECIFICS, specs);
    }
  }

  ModelTypeSet to_purge;
  to_purge.insert(BOOKMARKS);
  dir_->PurgeEntriesWithTypeIn(to_purge);

  Directory::SaveChangesSnapshot snapshot1;
  base::AutoLock scoped_lock(dir_->kernel_->save_changes_mutex);
  dir_->TakeSnapshotForSaveChanges(&snapshot1);
  EXPECT_TRUE(expected_purges == snapshot1.metahandles_to_purge);

  to_purge.clear();
  to_purge.insert(PREFERENCES);
  dir_->PurgeEntriesWithTypeIn(to_purge);

  dir_->HandleSaveChangesFailure(snapshot1);

  Directory::SaveChangesSnapshot snapshot2;
  dir_->TakeSnapshotForSaveChanges(&snapshot2);
  EXPECT_TRUE(all_handles == snapshot2.metahandles_to_purge);
}

TEST_F(SyncableDirectoryTest, TakeSnapshotGetsAllDirtyHandlesTest) {
  const int metahandles_to_create = 100;
  std::vector<int64> expected_dirty_metahandles;
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir_.get());
    for (int i = 0; i < metahandles_to_create; i++) {
      MutableEntry e(&trans, CREATE, trans.root_id(), "foo");
      expected_dirty_metahandles.push_back(e.Get(META_HANDLE));
      e.Put(IS_UNSYNCED, true);
    }
  }
  // Fake SaveChanges() and make sure we got what we expected.
  {
    Directory::SaveChangesSnapshot snapshot;
    base::AutoLock scoped_lock(dir_->kernel_->save_changes_mutex);
    dir_->TakeSnapshotForSaveChanges(&snapshot);
    // Make sure there's an entry for each new metahandle.  Make sure all
    // entries are marked dirty.
    ASSERT_EQ(expected_dirty_metahandles.size(), snapshot.dirty_metas.size());
    for (EntryKernelSet::const_iterator i = snapshot.dirty_metas.begin();
        i != snapshot.dirty_metas.end(); ++i) {
      ASSERT_TRUE(i->is_dirty());
    }
    dir_->VacuumAfterSaveChanges(snapshot);
  }
  // Put a new value with existing transactions as well as adding new ones.
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir_.get());
    std::vector<int64> new_dirty_metahandles;
    for (std::vector<int64>::const_iterator i =
        expected_dirty_metahandles.begin();
        i != expected_dirty_metahandles.end(); ++i) {
        // Change existing entries to directories to dirty them.
        MutableEntry e1(&trans, GET_BY_HANDLE, *i);
        e1.Put(IS_DIR, true);
        e1.Put(IS_UNSYNCED, true);
        // Add new entries
        MutableEntry e2(&trans, CREATE, trans.root_id(), "bar");
        e2.Put(IS_UNSYNCED, true);
        new_dirty_metahandles.push_back(e2.Get(META_HANDLE));
    }
    expected_dirty_metahandles.insert(expected_dirty_metahandles.end(),
        new_dirty_metahandles.begin(), new_dirty_metahandles.end());
  }
  // Fake SaveChanges() and make sure we got what we expected.
  {
    Directory::SaveChangesSnapshot snapshot;
    base::AutoLock scoped_lock(dir_->kernel_->save_changes_mutex);
    dir_->TakeSnapshotForSaveChanges(&snapshot);
    // Make sure there's an entry for each new metahandle.  Make sure all
    // entries are marked dirty.
    EXPECT_EQ(expected_dirty_metahandles.size(), snapshot.dirty_metas.size());
    for (EntryKernelSet::const_iterator i = snapshot.dirty_metas.begin();
        i != snapshot.dirty_metas.end(); ++i) {
      EXPECT_TRUE(i->is_dirty());
    }
    dir_->VacuumAfterSaveChanges(snapshot);
  }
}

TEST_F(SyncableDirectoryTest, TestPurgeEntriesWithTypeIn) {
  sync_pb::EntitySpecifics bookmark_specs;
  sync_pb::EntitySpecifics autofill_specs;
  sync_pb::EntitySpecifics preference_specs;
  AddDefaultExtensionValue(BOOKMARKS, &bookmark_specs);
  AddDefaultExtensionValue(PREFERENCES, &preference_specs);
  AddDefaultExtensionValue(AUTOFILL, &autofill_specs);
  dir_->set_initial_sync_ended_for_type(BOOKMARKS, true);
  dir_->set_initial_sync_ended_for_type(PREFERENCES, true);
  dir_->set_initial_sync_ended_for_type(AUTOFILL, true);

  std::set<ModelType> types_to_purge;
  types_to_purge.insert(PREFERENCES);
  types_to_purge.insert(AUTOFILL);

  TestIdFactory id_factory;
  // Create some items for each type.
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir_.get());
    MutableEntry item1(&trans, CREATE, trans.root_id(), "Item");
    ASSERT_TRUE(item1.good());
    item1.Put(SPECIFICS, bookmark_specs);
    item1.Put(SERVER_SPECIFICS, bookmark_specs);
    item1.Put(IS_UNSYNCED, true);

    MutableEntry item2(&trans, CREATE_NEW_UPDATE_ITEM,
                       id_factory.NewServerId());
    ASSERT_TRUE(item2.good());
    item2.Put(SERVER_SPECIFICS, bookmark_specs);
    item2.Put(IS_UNAPPLIED_UPDATE, true);

    MutableEntry item3(&trans, CREATE, trans.root_id(), "Item");
    ASSERT_TRUE(item3.good());
    item3.Put(SPECIFICS, preference_specs);
    item3.Put(SERVER_SPECIFICS, preference_specs);
    item3.Put(IS_UNSYNCED, true);

    MutableEntry item4(&trans, CREATE_NEW_UPDATE_ITEM,
                       id_factory.NewServerId());
    ASSERT_TRUE(item4.good());
    item4.Put(SERVER_SPECIFICS, preference_specs);
    item4.Put(IS_UNAPPLIED_UPDATE, true);

    MutableEntry item5(&trans, CREATE, trans.root_id(), "Item");
    ASSERT_TRUE(item5.good());
    item5.Put(SPECIFICS, autofill_specs);
    item5.Put(SERVER_SPECIFICS, autofill_specs);
    item5.Put(IS_UNSYNCED, true);

    MutableEntry item6(&trans, CREATE_NEW_UPDATE_ITEM,
      id_factory.NewServerId());
    ASSERT_TRUE(item6.good());
    item6.Put(SERVER_SPECIFICS, autofill_specs);
    item6.Put(IS_UNAPPLIED_UPDATE, true);
  }

  dir_->SaveChanges();
  {
    ReadTransaction trans(FROM_HERE, dir_.get());
    MetahandleSet all_set;
    dir_->GetAllMetaHandles(&trans, &all_set);
    ASSERT_EQ(7U, all_set.size());
  }

  dir_->PurgeEntriesWithTypeIn(types_to_purge);

  // We first query the in-memory data, and then reload the directory (without
  // saving) to verify that disk does not still have the data.
  CheckPurgeEntriesWithTypeInSucceeded(types_to_purge, true);
  SaveAndReloadDir();
  CheckPurgeEntriesWithTypeInSucceeded(types_to_purge, false);
}

TEST_F(SyncableDirectoryTest, TakeSnapshotGetsOnlyDirtyHandlesTest) {
  const int metahandles_to_create = 100;

  // half of 2 * metahandles_to_create
  const unsigned int number_changed = 100u;
  std::vector<int64> expected_dirty_metahandles;
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir_.get());
    for (int i = 0; i < metahandles_to_create; i++) {
      MutableEntry e(&trans, CREATE, trans.root_id(), "foo");
      expected_dirty_metahandles.push_back(e.Get(META_HANDLE));
      e.Put(IS_UNSYNCED, true);
    }
  }
  dir_->SaveChanges();
  // Put a new value with existing transactions as well as adding new ones.
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir_.get());
    std::vector<int64> new_dirty_metahandles;
    for (std::vector<int64>::const_iterator i =
        expected_dirty_metahandles.begin();
        i != expected_dirty_metahandles.end(); ++i) {
        // Change existing entries to directories to dirty them.
        MutableEntry e1(&trans, GET_BY_HANDLE, *i);
        ASSERT_TRUE(e1.good());
        e1.Put(IS_DIR, true);
        e1.Put(IS_UNSYNCED, true);
        // Add new entries
        MutableEntry e2(&trans, CREATE, trans.root_id(), "bar");
        e2.Put(IS_UNSYNCED, true);
        new_dirty_metahandles.push_back(e2.Get(META_HANDLE));
    }
    expected_dirty_metahandles.insert(expected_dirty_metahandles.end(),
        new_dirty_metahandles.begin(), new_dirty_metahandles.end());
  }
  dir_->SaveChanges();
  // Don't make any changes whatsoever and ensure nothing comes back.
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir_.get());
    for (std::vector<int64>::const_iterator i =
        expected_dirty_metahandles.begin();
        i != expected_dirty_metahandles.end(); ++i) {
      MutableEntry e(&trans, GET_BY_HANDLE, *i);
      ASSERT_TRUE(e.good());
      // We aren't doing anything to dirty these entries.
    }
  }
  // Fake SaveChanges() and make sure we got what we expected.
  {
    Directory::SaveChangesSnapshot snapshot;
    base::AutoLock scoped_lock(dir_->kernel_->save_changes_mutex);
    dir_->TakeSnapshotForSaveChanges(&snapshot);
    // Make sure there are no dirty_metahandles.
    EXPECT_EQ(0u, snapshot.dirty_metas.size());
    dir_->VacuumAfterSaveChanges(snapshot);
  }
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir_.get());
    bool should_change = false;
    for (std::vector<int64>::const_iterator i =
        expected_dirty_metahandles.begin();
        i != expected_dirty_metahandles.end(); ++i) {
        // Maybe change entries by flipping IS_DIR.
        MutableEntry e(&trans, GET_BY_HANDLE, *i);
        ASSERT_TRUE(e.good());
        should_change = !should_change;
        if (should_change) {
          bool not_dir = !e.Get(IS_DIR);
          e.Put(IS_DIR, not_dir);
          e.Put(IS_UNSYNCED, true);
        }
    }
  }
  // Fake SaveChanges() and make sure we got what we expected.
  {
    Directory::SaveChangesSnapshot snapshot;
    base::AutoLock scoped_lock(dir_->kernel_->save_changes_mutex);
    dir_->TakeSnapshotForSaveChanges(&snapshot);
    // Make sure there's an entry for each changed metahandle.  Make sure all
    // entries are marked dirty.
    EXPECT_EQ(number_changed, snapshot.dirty_metas.size());
    for (EntryKernelSet::const_iterator i = snapshot.dirty_metas.begin();
        i != snapshot.dirty_metas.end(); ++i) {
      EXPECT_TRUE(i->is_dirty());
    }
    dir_->VacuumAfterSaveChanges(snapshot);
  }
}

const char SyncableDirectoryTest::kName[] = "Foo";
const Id SyncableDirectoryTest::kId(TestIdFactory::FromNumber(-99));

namespace {
TEST_F(SyncableDirectoryTest, TestBasicLookupNonExistantID) {
  ReadTransaction rtrans(FROM_HERE, dir_.get());
  Entry e(&rtrans, GET_BY_ID, kId);
  ASSERT_FALSE(e.good());
}

TEST_F(SyncableDirectoryTest, TestBasicLookupValidID) {
  CreateEntry("rtc");
  ReadTransaction rtrans(FROM_HERE, dir_.get());
  Entry e(&rtrans, GET_BY_ID, kId);
  ASSERT_TRUE(e.good());
}

TEST_F(SyncableDirectoryTest, TestDelete) {
  std::string name = "peanut butter jelly time";
  WriteTransaction trans(FROM_HERE, UNITTEST, dir_.get());
  MutableEntry e1(&trans, CREATE, trans.root_id(), name);
  ASSERT_TRUE(e1.good());
  ASSERT_TRUE(e1.Put(IS_DEL, true));
  MutableEntry e2(&trans, CREATE, trans.root_id(), name);
  ASSERT_TRUE(e2.good());
  ASSERT_TRUE(e2.Put(IS_DEL, true));
  MutableEntry e3(&trans, CREATE, trans.root_id(), name);
  ASSERT_TRUE(e3.good());
  ASSERT_TRUE(e3.Put(IS_DEL, true));

  ASSERT_TRUE(e1.Put(IS_DEL, false));
  ASSERT_TRUE(e2.Put(IS_DEL, false));
  ASSERT_TRUE(e3.Put(IS_DEL, false));

  ASSERT_TRUE(e1.Put(IS_DEL, true));
  ASSERT_TRUE(e2.Put(IS_DEL, true));
  ASSERT_TRUE(e3.Put(IS_DEL, true));
}

TEST_F(SyncableDirectoryTest, TestGetUnsynced) {
  Directory::UnsyncedMetaHandles handles;
  int64 handle1, handle2;
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir_.get());

    dir_->GetUnsyncedMetaHandles(&trans, &handles);
    ASSERT_TRUE(0 == handles.size());

    MutableEntry e1(&trans, CREATE, trans.root_id(), "abba");
    ASSERT_TRUE(e1.good());
    handle1 = e1.Get(META_HANDLE);
    e1.Put(BASE_VERSION, 1);
    e1.Put(IS_DIR, true);
    e1.Put(ID, TestIdFactory::FromNumber(101));

    MutableEntry e2(&trans, CREATE, e1.Get(ID), "bread");
    ASSERT_TRUE(e2.good());
    handle2 = e2.Get(META_HANDLE);
    e2.Put(BASE_VERSION, 1);
    e2.Put(ID, TestIdFactory::FromNumber(102));
  }
  dir_->SaveChanges();
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir_.get());

    dir_->GetUnsyncedMetaHandles(&trans, &handles);
    ASSERT_TRUE(0 == handles.size());

    MutableEntry e3(&trans, GET_BY_HANDLE, handle1);
    ASSERT_TRUE(e3.good());
    e3.Put(IS_UNSYNCED, true);
  }
  dir_->SaveChanges();
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir_.get());
    dir_->GetUnsyncedMetaHandles(&trans, &handles);
    ASSERT_TRUE(1 == handles.size());
    ASSERT_TRUE(handle1 == handles[0]);

    MutableEntry e4(&trans, GET_BY_HANDLE, handle2);
    ASSERT_TRUE(e4.good());
    e4.Put(IS_UNSYNCED, true);
  }
  dir_->SaveChanges();
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir_.get());
    dir_->GetUnsyncedMetaHandles(&trans, &handles);
    ASSERT_TRUE(2 == handles.size());
    if (handle1 == handles[0]) {
      ASSERT_TRUE(handle2 == handles[1]);
    } else {
      ASSERT_TRUE(handle2 == handles[0]);
      ASSERT_TRUE(handle1 == handles[1]);
    }

    MutableEntry e5(&trans, GET_BY_HANDLE, handle1);
    ASSERT_TRUE(e5.good());
    ASSERT_TRUE(e5.Get(IS_UNSYNCED));
    ASSERT_TRUE(e5.Put(IS_UNSYNCED, false));
    ASSERT_FALSE(e5.Get(IS_UNSYNCED));
  }
  dir_->SaveChanges();
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir_.get());
    dir_->GetUnsyncedMetaHandles(&trans, &handles);
    ASSERT_TRUE(1 == handles.size());
    ASSERT_TRUE(handle2 == handles[0]);
  }
}

TEST_F(SyncableDirectoryTest, TestGetUnappliedUpdates) {
  Directory::UnappliedUpdateMetaHandles handles;
  int64 handle1, handle2;
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir_.get());

    dir_->GetUnappliedUpdateMetaHandles(&trans, &handles);
    ASSERT_TRUE(0 == handles.size());

    MutableEntry e1(&trans, CREATE, trans.root_id(), "abba");
    ASSERT_TRUE(e1.good());
    handle1 = e1.Get(META_HANDLE);
    e1.Put(IS_UNAPPLIED_UPDATE, false);
    e1.Put(BASE_VERSION, 1);
    e1.Put(ID, TestIdFactory::FromNumber(101));
    e1.Put(IS_DIR, true);

    MutableEntry e2(&trans, CREATE, e1.Get(ID), "bread");
    ASSERT_TRUE(e2.good());
    handle2 = e2.Get(META_HANDLE);
    e2.Put(IS_UNAPPLIED_UPDATE, false);
    e2.Put(BASE_VERSION, 1);
    e2.Put(ID, TestIdFactory::FromNumber(102));
  }
  dir_->SaveChanges();
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir_.get());

    dir_->GetUnappliedUpdateMetaHandles(&trans, &handles);
    ASSERT_TRUE(0 == handles.size());

    MutableEntry e3(&trans, GET_BY_HANDLE, handle1);
    ASSERT_TRUE(e3.good());
    e3.Put(IS_UNAPPLIED_UPDATE, true);
  }
  dir_->SaveChanges();
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir_.get());
    dir_->GetUnappliedUpdateMetaHandles(&trans, &handles);
    ASSERT_TRUE(1 == handles.size());
    ASSERT_TRUE(handle1 == handles[0]);

    MutableEntry e4(&trans, GET_BY_HANDLE, handle2);
    ASSERT_TRUE(e4.good());
    e4.Put(IS_UNAPPLIED_UPDATE, true);
  }
  dir_->SaveChanges();
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir_.get());
    dir_->GetUnappliedUpdateMetaHandles(&trans, &handles);
    ASSERT_TRUE(2 == handles.size());
    if (handle1 == handles[0]) {
      ASSERT_TRUE(handle2 == handles[1]);
    } else {
      ASSERT_TRUE(handle2 == handles[0]);
      ASSERT_TRUE(handle1 == handles[1]);
    }

    MutableEntry e5(&trans, GET_BY_HANDLE, handle1);
    ASSERT_TRUE(e5.good());
    e5.Put(IS_UNAPPLIED_UPDATE, false);
  }
  dir_->SaveChanges();
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir_.get());
    dir_->GetUnappliedUpdateMetaHandles(&trans, &handles);
    ASSERT_TRUE(1 == handles.size());
    ASSERT_TRUE(handle2 == handles[0]);
  }
}


TEST_F(SyncableDirectoryTest, DeleteBug_531383) {
  // Try to evoke a check failure...
  TestIdFactory id_factory;
  int64 grandchild_handle;
  {
    WriteTransaction wtrans(FROM_HERE, UNITTEST, dir_.get());
    MutableEntry parent(&wtrans, CREATE, id_factory.root(), "Bob");
    ASSERT_TRUE(parent.good());
    parent.Put(IS_DIR, true);
    parent.Put(ID, id_factory.NewServerId());
    parent.Put(BASE_VERSION, 1);
    MutableEntry child(&wtrans, CREATE, parent.Get(ID), "Bob");
    ASSERT_TRUE(child.good());
    child.Put(IS_DIR, true);
    child.Put(ID, id_factory.NewServerId());
    child.Put(BASE_VERSION, 1);
    MutableEntry grandchild(&wtrans, CREATE, child.Get(ID), "Bob");
    ASSERT_TRUE(grandchild.good());
    grandchild.Put(ID, id_factory.NewServerId());
    grandchild.Put(BASE_VERSION, 1);
    ASSERT_TRUE(grandchild.Put(IS_DEL, true));
    MutableEntry twin(&wtrans, CREATE, child.Get(ID), "Bob");
    ASSERT_TRUE(twin.good());
    ASSERT_TRUE(twin.Put(IS_DEL, true));
    ASSERT_TRUE(grandchild.Put(IS_DEL, false));

    grandchild_handle = grandchild.Get(META_HANDLE);
  }
  dir_->SaveChanges();
  {
    WriteTransaction wtrans(FROM_HERE, UNITTEST, dir_.get());
    MutableEntry grandchild(&wtrans, GET_BY_HANDLE, grandchild_handle);
    grandchild.Put(IS_DEL, true);  // Used to CHECK fail here.
  }
}

static inline bool IsLegalNewParent(const Entry& a, const Entry& b) {
  return IsLegalNewParent(a.trans(), a.Get(ID), b.Get(ID));
}

TEST_F(SyncableDirectoryTest, TestIsLegalNewParent) {
  TestIdFactory id_factory;
  WriteTransaction wtrans(FROM_HERE, UNITTEST, dir_.get());
  Entry root(&wtrans, GET_BY_ID, id_factory.root());
  ASSERT_TRUE(root.good());
  MutableEntry parent(&wtrans, CREATE, root.Get(ID), "Bob");
  ASSERT_TRUE(parent.good());
  parent.Put(IS_DIR, true);
  parent.Put(ID, id_factory.NewServerId());
  parent.Put(BASE_VERSION, 1);
  MutableEntry child(&wtrans, CREATE, parent.Get(ID), "Bob");
  ASSERT_TRUE(child.good());
  child.Put(IS_DIR, true);
  child.Put(ID, id_factory.NewServerId());
  child.Put(BASE_VERSION, 1);
  MutableEntry grandchild(&wtrans, CREATE, child.Get(ID), "Bob");
  ASSERT_TRUE(grandchild.good());
  grandchild.Put(ID, id_factory.NewServerId());
  grandchild.Put(BASE_VERSION, 1);

  MutableEntry parent2(&wtrans, CREATE, root.Get(ID), "Pete");
  ASSERT_TRUE(parent2.good());
  parent2.Put(IS_DIR, true);
  parent2.Put(ID, id_factory.NewServerId());
  parent2.Put(BASE_VERSION, 1);
  MutableEntry child2(&wtrans, CREATE, parent2.Get(ID), "Pete");
  ASSERT_TRUE(child2.good());
  child2.Put(IS_DIR, true);
  child2.Put(ID, id_factory.NewServerId());
  child2.Put(BASE_VERSION, 1);
  MutableEntry grandchild2(&wtrans, CREATE, child2.Get(ID), "Pete");
  ASSERT_TRUE(grandchild2.good());
  grandchild2.Put(ID, id_factory.NewServerId());
  grandchild2.Put(BASE_VERSION, 1);
  // resulting tree
  //           root
  //           /  |
  //     parent    parent2
  //          |    |
  //      child    child2
  //          |    |
  // grandchild    grandchild2
  ASSERT_TRUE(IsLegalNewParent(child, root));
  ASSERT_TRUE(IsLegalNewParent(child, parent));
  ASSERT_FALSE(IsLegalNewParent(child, child));
  ASSERT_FALSE(IsLegalNewParent(child, grandchild));
  ASSERT_TRUE(IsLegalNewParent(child, parent2));
  ASSERT_TRUE(IsLegalNewParent(child, grandchild2));
  ASSERT_FALSE(IsLegalNewParent(parent, grandchild));
  ASSERT_FALSE(IsLegalNewParent(root, grandchild));
  ASSERT_FALSE(IsLegalNewParent(parent, grandchild));
}

TEST_F(SyncableDirectoryTest, TestEntryIsInFolder) {
  // Create a subdir and an entry.
  int64 entry_handle;
  syncable::Id folder_id;
  syncable::Id entry_id;
  std::string entry_name = "entry";

  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir_.get());
    MutableEntry folder(&trans, CREATE, trans.root_id(), "folder");
    ASSERT_TRUE(folder.good());
    EXPECT_TRUE(folder.Put(IS_DIR, true));
    EXPECT_TRUE(folder.Put(IS_UNSYNCED, true));
    folder_id = folder.Get(ID);

    MutableEntry entry(&trans, CREATE, folder.Get(ID), entry_name);
    ASSERT_TRUE(entry.good());
    entry_handle = entry.Get(META_HANDLE);
    entry.Put(IS_UNSYNCED, true);
    entry_id = entry.Get(ID);
  }

  // Make sure we can find the entry in the folder.
  {
    ReadTransaction trans(FROM_HERE, dir_.get());
    EXPECT_EQ(0, CountEntriesWithName(&trans, trans.root_id(), entry_name));
    EXPECT_EQ(1, CountEntriesWithName(&trans, folder_id, entry_name));

    Entry entry(&trans, GET_BY_ID, entry_id);
    ASSERT_TRUE(entry.good());
    EXPECT_EQ(entry_handle, entry.Get(META_HANDLE));
    EXPECT_TRUE(entry.Get(NON_UNIQUE_NAME) == entry_name);
    EXPECT_TRUE(entry.Get(PARENT_ID) == folder_id);
  }
}

TEST_F(SyncableDirectoryTest, TestParentIdIndexUpdate) {
  std::string child_name = "child";

  WriteTransaction wt(FROM_HERE, UNITTEST, dir_.get());
  MutableEntry parent_folder(&wt, CREATE, wt.root_id(), "folder1");
  parent_folder.Put(IS_UNSYNCED, true);
  EXPECT_TRUE(parent_folder.Put(IS_DIR, true));

  MutableEntry parent_folder2(&wt, CREATE, wt.root_id(), "folder2");
  parent_folder2.Put(IS_UNSYNCED, true);
  EXPECT_TRUE(parent_folder2.Put(IS_DIR, true));

  MutableEntry child(&wt, CREATE, parent_folder.Get(ID), child_name);
  EXPECT_TRUE(child.Put(IS_DIR, true));
  child.Put(IS_UNSYNCED, true);

  ASSERT_TRUE(child.good());

  EXPECT_EQ(0, CountEntriesWithName(&wt, wt.root_id(), child_name));
  EXPECT_EQ(parent_folder.Get(ID), child.Get(PARENT_ID));
  EXPECT_EQ(1, CountEntriesWithName(&wt, parent_folder.Get(ID), child_name));
  EXPECT_EQ(0, CountEntriesWithName(&wt, parent_folder2.Get(ID), child_name));
  child.Put(PARENT_ID, parent_folder2.Get(ID));
  EXPECT_EQ(parent_folder2.Get(ID), child.Get(PARENT_ID));
  EXPECT_EQ(0, CountEntriesWithName(&wt, parent_folder.Get(ID), child_name));
  EXPECT_EQ(1, CountEntriesWithName(&wt, parent_folder2.Get(ID), child_name));
}

TEST_F(SyncableDirectoryTest, TestNoReindexDeletedItems) {
  std::string folder_name = "folder";
  std::string new_name = "new_name";

  WriteTransaction trans(FROM_HERE, UNITTEST, dir_.get());
  MutableEntry folder(&trans, CREATE, trans.root_id(), folder_name);
  ASSERT_TRUE(folder.good());
  ASSERT_TRUE(folder.Put(IS_DIR, true));
  ASSERT_TRUE(folder.Put(IS_DEL, true));

  EXPECT_EQ(0, CountEntriesWithName(&trans, trans.root_id(), folder_name));

  MutableEntry deleted(&trans, GET_BY_ID, folder.Get(ID));
  ASSERT_TRUE(deleted.good());
  ASSERT_TRUE(deleted.Put(PARENT_ID, trans.root_id()));
  ASSERT_TRUE(deleted.Put(NON_UNIQUE_NAME, new_name));

  EXPECT_EQ(0, CountEntriesWithName(&trans, trans.root_id(), folder_name));
  EXPECT_EQ(0, CountEntriesWithName(&trans, trans.root_id(), new_name));
}

TEST_F(SyncableDirectoryTest, TestCaseChangeRename) {
  WriteTransaction trans(FROM_HERE, UNITTEST, dir_.get());
  MutableEntry folder(&trans, CREATE, trans.root_id(), "CaseChange");
  ASSERT_TRUE(folder.good());
  EXPECT_TRUE(folder.Put(PARENT_ID, trans.root_id()));
  EXPECT_TRUE(folder.Put(NON_UNIQUE_NAME, "CASECHANGE"));
  EXPECT_TRUE(folder.Put(IS_DEL, true));
}

TEST_F(SyncableDirectoryTest, TestShareInfo) {
  dir_->set_initial_sync_ended_for_type(AUTOFILL, true);
  dir_->set_store_birthday("Jan 31st");
  dir_->SetNotificationState("notification_state");
  {
    ReadTransaction trans(FROM_HERE, dir_.get());
    EXPECT_TRUE(dir_->initial_sync_ended_for_type(AUTOFILL));
    EXPECT_FALSE(dir_->initial_sync_ended_for_type(BOOKMARKS));
    EXPECT_EQ("Jan 31st", dir_->store_birthday());
    EXPECT_EQ("notification_state", dir_->GetNotificationState());
  }
  dir_->set_store_birthday("April 10th");
  dir_->SetNotificationState("notification_state2");
  dir_->SaveChanges();
  {
    ReadTransaction trans(FROM_HERE, dir_.get());
    EXPECT_TRUE(dir_->initial_sync_ended_for_type(AUTOFILL));
    EXPECT_FALSE(dir_->initial_sync_ended_for_type(BOOKMARKS));
    EXPECT_EQ("April 10th", dir_->store_birthday());
    EXPECT_EQ("notification_state2", dir_->GetNotificationState());
  }
  dir_->SetNotificationState("notification_state2");
  // Restore the directory from disk.  Make sure that nothing's changed.
  SaveAndReloadDir();
  {
    ReadTransaction trans(FROM_HERE, dir_.get());
    EXPECT_TRUE(dir_->initial_sync_ended_for_type(AUTOFILL));
    EXPECT_FALSE(dir_->initial_sync_ended_for_type(BOOKMARKS));
    EXPECT_EQ("April 10th", dir_->store_birthday());
    EXPECT_EQ("notification_state2", dir_->GetNotificationState());
  }
}

TEST_F(SyncableDirectoryTest, TestSimpleFieldsPreservedDuringSaveChanges) {
  Id update_id = TestIdFactory::FromNumber(1);
  Id create_id;
  EntryKernel create_pre_save, update_pre_save;
  EntryKernel create_post_save, update_post_save;
  std::string create_name =  "Create";

  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir_.get());
    MutableEntry create(&trans, CREATE, trans.root_id(), create_name);
    MutableEntry update(&trans, CREATE_NEW_UPDATE_ITEM, update_id);
    create.Put(IS_UNSYNCED, true);
    update.Put(IS_UNAPPLIED_UPDATE, true);
    sync_pb::EntitySpecifics specifics;
    specifics.MutableExtension(sync_pb::bookmark)->set_favicon("PNG");
    specifics.MutableExtension(sync_pb::bookmark)->set_url("http://nowhere");
    create.Put(SPECIFICS, specifics);
    create_pre_save = create.GetKernelCopy();
    update_pre_save = update.GetKernelCopy();
    create_id = create.Get(ID);
  }

  dir_->SaveChanges();
  dir_.reset(new Directory());
  ASSERT_TRUE(dir_.get());
  ASSERT_EQ(OPENED, dir_->Open(file_path_, kName,
                               &delegate_, NullTransactionObserver()));
  ASSERT_TRUE(dir_->good());

  {
    ReadTransaction trans(FROM_HERE, dir_.get());
    Entry create(&trans, GET_BY_ID, create_id);
    EXPECT_EQ(1, CountEntriesWithName(&trans, trans.root_id(), create_name));
    Entry update(&trans, GET_BY_ID, update_id);
    create_post_save = create.GetKernelCopy();
    update_post_save = update.GetKernelCopy();
  }
  int i = BEGIN_FIELDS;
  for ( ; i < INT64_FIELDS_END ; ++i) {
    EXPECT_EQ(create_pre_save.ref((Int64Field)i),
              create_post_save.ref((Int64Field)i))
              << "int64 field #" << i << " changed during save/load";
    EXPECT_EQ(update_pre_save.ref((Int64Field)i),
              update_post_save.ref((Int64Field)i))
              << "int64 field #" << i << " changed during save/load";
  }
  for ( ; i < TIME_FIELDS_END ; ++i) {
    EXPECT_EQ(create_pre_save.ref((TimeField)i),
              create_post_save.ref((TimeField)i))
              << "time field #" << i << " changed during save/load";
    EXPECT_EQ(update_pre_save.ref((TimeField)i),
              update_post_save.ref((TimeField)i))
              << "time field #" << i << " changed during save/load";
  }
  for ( ; i < ID_FIELDS_END ; ++i) {
    EXPECT_EQ(create_pre_save.ref((IdField)i),
              create_post_save.ref((IdField)i))
              << "id field #" << i << " changed during save/load";
    EXPECT_EQ(update_pre_save.ref((IdField)i),
              update_pre_save.ref((IdField)i))
              << "id field #" << i << " changed during save/load";
  }
  for ( ; i < BIT_FIELDS_END ; ++i) {
    EXPECT_EQ(create_pre_save.ref((BitField)i),
              create_post_save.ref((BitField)i))
              << "Bit field #" << i << " changed during save/load";
    EXPECT_EQ(update_pre_save.ref((BitField)i),
              update_post_save.ref((BitField)i))
              << "Bit field #" << i << " changed during save/load";
  }
  for ( ; i < STRING_FIELDS_END ; ++i) {
    EXPECT_EQ(create_pre_save.ref((StringField)i),
              create_post_save.ref((StringField)i))
              << "String field #" << i << " changed during save/load";
    EXPECT_EQ(update_pre_save.ref((StringField)i),
              update_post_save.ref((StringField)i))
              << "String field #" << i << " changed during save/load";
  }
  for ( ; i < PROTO_FIELDS_END; ++i) {
    EXPECT_EQ(create_pre_save.ref((ProtoField)i).SerializeAsString(),
              create_post_save.ref((ProtoField)i).SerializeAsString())
              << "Blob field #" << i << " changed during save/load";
    EXPECT_EQ(update_pre_save.ref((ProtoField)i).SerializeAsString(),
              update_post_save.ref((ProtoField)i).SerializeAsString())
              << "Blob field #" << i << " changed during save/load";
  }
}

TEST_F(SyncableDirectoryTest, TestSaveChangesFailure) {
  int64 handle1 = 0;
  // Set up an item using a regular, saveable directory.
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir_.get());

    MutableEntry e1(&trans, CREATE, trans.root_id(), "aguilera");
    ASSERT_TRUE(e1.good());
    EXPECT_TRUE(e1.GetKernelCopy().is_dirty());
    handle1 = e1.Get(META_HANDLE);
    e1.Put(BASE_VERSION, 1);
    e1.Put(IS_DIR, true);
    e1.Put(ID, TestIdFactory::FromNumber(101));
    EXPECT_TRUE(e1.GetKernelCopy().is_dirty());
    EXPECT_TRUE(IsInDirtyMetahandles(handle1));
  }
  ASSERT_TRUE(dir_->SaveChanges());

  // Make sure the item is no longer dirty after saving,
  // and make a modification.
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir_.get());

    MutableEntry aguilera(&trans, GET_BY_HANDLE, handle1);
    ASSERT_TRUE(aguilera.good());
    EXPECT_FALSE(aguilera.GetKernelCopy().is_dirty());
    EXPECT_EQ(aguilera.Get(NON_UNIQUE_NAME), "aguilera");
    aguilera.Put(NON_UNIQUE_NAME, "overwritten");
    EXPECT_TRUE(aguilera.GetKernelCopy().is_dirty());
    EXPECT_TRUE(IsInDirtyMetahandles(handle1));
  }
  ASSERT_TRUE(dir_->SaveChanges());

  // Now do some operations using a directory for which SaveChanges will
  // always fail.
  dir_.reset(new TestUnsaveableDirectory());
  ASSERT_TRUE(dir_.get());
  ASSERT_EQ(OPENED, dir_->Open(file_path_, kName,
                               &delegate_, NullTransactionObserver()));
  ASSERT_TRUE(dir_->good());
  int64 handle2 = 0;
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir_.get());

    MutableEntry aguilera(&trans, GET_BY_HANDLE, handle1);
    ASSERT_TRUE(aguilera.good());
    EXPECT_FALSE(aguilera.GetKernelCopy().is_dirty());
    EXPECT_EQ(aguilera.Get(NON_UNIQUE_NAME), "overwritten");
    EXPECT_FALSE(aguilera.GetKernelCopy().is_dirty());
    EXPECT_FALSE(IsInDirtyMetahandles(handle1));
    aguilera.Put(NON_UNIQUE_NAME, "christina");
    EXPECT_TRUE(aguilera.GetKernelCopy().is_dirty());
    EXPECT_TRUE(IsInDirtyMetahandles(handle1));

    // New item.
    MutableEntry kids_on_block(&trans, CREATE, trans.root_id(), "kids");
    ASSERT_TRUE(kids_on_block.good());
    handle2 = kids_on_block.Get(META_HANDLE);
    kids_on_block.Put(BASE_VERSION, 1);
    kids_on_block.Put(IS_DIR, true);
    kids_on_block.Put(ID, TestIdFactory::FromNumber(102));
    EXPECT_TRUE(kids_on_block.GetKernelCopy().is_dirty());
    EXPECT_TRUE(IsInDirtyMetahandles(handle2));
  }

  // We are using an unsaveable directory, so this can't succeed.  However,
  // the HandleSaveChangesFailure code path should have been triggered.
  ASSERT_FALSE(dir_->SaveChanges());

  // Make sure things were rolled back and the world is as it was before call.
  {
     ReadTransaction trans(FROM_HERE, dir_.get());
     Entry e1(&trans, GET_BY_HANDLE, handle1);
     ASSERT_TRUE(e1.good());
     EntryKernel aguilera = e1.GetKernelCopy();
     Entry kids(&trans, GET_BY_HANDLE, handle2);
     ASSERT_TRUE(kids.good());
     EXPECT_TRUE(kids.GetKernelCopy().is_dirty());
     EXPECT_TRUE(IsInDirtyMetahandles(handle2));
     EXPECT_TRUE(aguilera.is_dirty());
     EXPECT_TRUE(IsInDirtyMetahandles(handle1));
  }
}

TEST_F(SyncableDirectoryTest, TestSaveChangesFailureWithPurge) {
  int64 handle1 = 0;
  // Set up an item using a regular, saveable directory.
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir_.get());

    MutableEntry e1(&trans, CREATE, trans.root_id(), "aguilera");
    ASSERT_TRUE(e1.good());
    EXPECT_TRUE(e1.GetKernelCopy().is_dirty());
    handle1 = e1.Get(META_HANDLE);
    e1.Put(BASE_VERSION, 1);
    e1.Put(IS_DIR, true);
    e1.Put(ID, TestIdFactory::FromNumber(101));
    sync_pb::EntitySpecifics bookmark_specs;
    AddDefaultExtensionValue(BOOKMARKS, &bookmark_specs);
    e1.Put(SPECIFICS, bookmark_specs);
    e1.Put(SERVER_SPECIFICS, bookmark_specs);
    e1.Put(ID, TestIdFactory::FromNumber(101));
    EXPECT_TRUE(e1.GetKernelCopy().is_dirty());
    EXPECT_TRUE(IsInDirtyMetahandles(handle1));
  }
  ASSERT_TRUE(dir_->SaveChanges());

  // Now do some operations using a directory for which SaveChanges will
  // always fail.
  dir_.reset(new TestUnsaveableDirectory());
  ASSERT_TRUE(dir_.get());
  ASSERT_EQ(OPENED, dir_->Open(file_path_, kName,
                               &delegate_, NullTransactionObserver()));
  ASSERT_TRUE(dir_->good());

  ModelTypeSet set;
  set.insert(BOOKMARKS);
  dir_->PurgeEntriesWithTypeIn(set);
  EXPECT_TRUE(IsInMetahandlesToPurge(handle1));
  ASSERT_FALSE(dir_->SaveChanges());
  EXPECT_TRUE(IsInMetahandlesToPurge(handle1));
}

// Create items of each model type, and check that GetModelType and
// GetServerModelType return the right value.
TEST_F(SyncableDirectoryTest, GetModelType) {
  TestIdFactory id_factory;
  for (int i = 0; i < MODEL_TYPE_COUNT; ++i) {
    ModelType datatype = ModelTypeFromInt(i);
    SCOPED_TRACE(testing::Message("Testing model type ") << datatype);
    switch (datatype) {
      case UNSPECIFIED:
      case TOP_LEVEL_FOLDER:
        continue;  // Datatype isn't a function of Specifics.
      default:
        break;
    }
    sync_pb::EntitySpecifics specifics;
    AddDefaultExtensionValue(datatype, &specifics);

    WriteTransaction trans(FROM_HERE, UNITTEST, dir_.get());

    MutableEntry folder(&trans, CREATE, trans.root_id(), "Folder");
    ASSERT_TRUE(folder.good());
    folder.Put(ID, id_factory.NewServerId());
    folder.Put(SPECIFICS, specifics);
    folder.Put(BASE_VERSION, 1);
    folder.Put(IS_DIR, true);
    folder.Put(IS_DEL, false);
    ASSERT_EQ(datatype, folder.GetModelType());

    MutableEntry item(&trans, CREATE, trans.root_id(), "Item");
    ASSERT_TRUE(item.good());
    item.Put(ID, id_factory.NewServerId());
    item.Put(SPECIFICS, specifics);
    item.Put(BASE_VERSION, 1);
    item.Put(IS_DIR, false);
    item.Put(IS_DEL, false);
    ASSERT_EQ(datatype, item.GetModelType());

    // It's critical that deletion records retain their datatype, so that
    // they can be dispatched to the appropriate change processor.
    MutableEntry deleted_item(&trans, CREATE, trans.root_id(), "Deleted Item");
    ASSERT_TRUE(item.good());
    deleted_item.Put(ID, id_factory.NewServerId());
    deleted_item.Put(SPECIFICS, specifics);
    deleted_item.Put(BASE_VERSION, 1);
    deleted_item.Put(IS_DIR, false);
    deleted_item.Put(IS_DEL, true);
    ASSERT_EQ(datatype, deleted_item.GetModelType());

    MutableEntry server_folder(&trans, CREATE_NEW_UPDATE_ITEM,
        id_factory.NewServerId());
    ASSERT_TRUE(server_folder.good());
    server_folder.Put(SERVER_SPECIFICS, specifics);
    server_folder.Put(BASE_VERSION, 1);
    server_folder.Put(SERVER_IS_DIR, true);
    server_folder.Put(SERVER_IS_DEL, false);
    ASSERT_EQ(datatype, server_folder.GetServerModelType());

    MutableEntry server_item(&trans, CREATE_NEW_UPDATE_ITEM,
        id_factory.NewServerId());
    ASSERT_TRUE(server_item.good());
    server_item.Put(SERVER_SPECIFICS, specifics);
    server_item.Put(BASE_VERSION, 1);
    server_item.Put(SERVER_IS_DIR, false);
    server_item.Put(SERVER_IS_DEL, false);
    ASSERT_EQ(datatype, server_item.GetServerModelType());

    browser_sync::SyncEntity folder_entity;
    folder_entity.set_id(id_factory.NewServerId());
    folder_entity.set_deleted(false);
    folder_entity.set_folder(true);
    folder_entity.mutable_specifics()->CopyFrom(specifics);
    ASSERT_EQ(datatype, folder_entity.GetModelType());

    browser_sync::SyncEntity item_entity;
    item_entity.set_id(id_factory.NewServerId());
    item_entity.set_deleted(false);
    item_entity.set_folder(false);
    item_entity.mutable_specifics()->CopyFrom(specifics);
    ASSERT_EQ(datatype, item_entity.GetModelType());
  }
}

}  // namespace

void SyncableDirectoryTest::ValidateEntry(BaseTransaction* trans,
                                          int64 id,
                                          bool check_name,
                                          const std::string& name,
                                          int64 base_version,
                                          int64 server_version,
                                          bool is_del) {
  Entry e(trans, GET_BY_ID, TestIdFactory::FromNumber(id));
  ASSERT_TRUE(e.good());
  if (check_name)
    ASSERT_TRUE(name == e.Get(NON_UNIQUE_NAME));
  ASSERT_TRUE(base_version == e.Get(BASE_VERSION));
  ASSERT_TRUE(server_version == e.Get(SERVER_VERSION));
  ASSERT_TRUE(is_del == e.Get(IS_DEL));
}

namespace {

class SyncableDirectoryManager : public testing::Test {
 public:
  virtual void SetUp() {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

  virtual void TearDown() {
  }
 protected:
  MessageLoop message_loop_;
  ScopedTempDir temp_dir_;
  NullDirectoryChangeDelegate delegate_;
};

TEST_F(SyncableDirectoryManager, TestFileRelease) {
  DirectoryManager dm(FilePath(temp_dir_.path()));
  ASSERT_TRUE(dm.Open("ScopeTest", &delegate_, NullTransactionObserver()));
  {
    ScopedDirLookup(&dm, "ScopeTest");
  }
  dm.Close("ScopeTest");
  ASSERT_TRUE(file_util::Delete(dm.GetSyncDataDatabasePath(), true));
}

class ThreadOpenTestDelegate : public base::PlatformThread::Delegate {
 public:
  explicit ThreadOpenTestDelegate(DirectoryManager* dm)
      : directory_manager_(dm) {}
  DirectoryManager* const directory_manager_;
  NullDirectoryChangeDelegate delegate_;

 private:
  // PlatformThread::Delegate methods:
  virtual void ThreadMain() {
    CHECK(directory_manager_->Open(
        "Open", &delegate_, NullTransactionObserver()));
  }

  DISALLOW_COPY_AND_ASSIGN(ThreadOpenTestDelegate);
};

TEST_F(SyncableDirectoryManager, ThreadOpenTest) {
  DirectoryManager dm(FilePath(temp_dir_.path()));
  base::PlatformThreadHandle thread_handle;
  ThreadOpenTestDelegate test_delegate(&dm);
  ASSERT_TRUE(base::PlatformThread::Create(0, &test_delegate, &thread_handle));
  base::PlatformThread::Join(thread_handle);
  {
    ScopedDirLookup dir(&dm, "Open");
    ASSERT_TRUE(dir.good());
  }
  dm.Close("Open");
  ScopedDirLookup dir(&dm, "Open");
  ASSERT_FALSE(dir.good());
}

struct Step {
  Step() : condvar(&mutex), number(0) {}

  base::Lock mutex;
  base::ConditionVariable condvar;
  int number;
  int64 metahandle;
};

class ThreadBugDelegate : public base::PlatformThread::Delegate {
 public:
  // a role is 0 or 1, meaning this thread does the odd or event steps.
  ThreadBugDelegate(int role, Step* step, DirectoryManager* dirman)
      : role_(role), step_(step), directory_manager_(dirman) {}

 protected:
  const int role_;
  Step* const step_;
  NullDirectoryChangeDelegate delegate_;
  DirectoryManager* const directory_manager_;

  // PlatformThread::Delegate methods:
  virtual void ThreadMain() {
    MessageLoop message_loop;
    const std::string dirname = "ThreadBug1";
    base::AutoLock scoped_lock(step_->mutex);

    while (step_->number < 3) {
      while (step_->number % 2 != role_) {
        step_->condvar.Wait();
      }
      switch (step_->number) {
      case 0:
        directory_manager_->Open(
            dirname, &delegate_, NullTransactionObserver());
        break;
      case 1:
        {
          directory_manager_->Close(dirname);
          directory_manager_->Open(
              dirname, &delegate_, NullTransactionObserver());
          ScopedDirLookup dir(directory_manager_, dirname);
          CHECK(dir.good());
          WriteTransaction trans(FROM_HERE, UNITTEST, dir);
          MutableEntry me(&trans, CREATE, trans.root_id(), "Jeff");
          step_->metahandle = me.Get(META_HANDLE);
          me.Put(IS_UNSYNCED, true);
        }
        break;
      case 2:
        {
          ScopedDirLookup dir(directory_manager_, dirname);
          CHECK(dir.good());
          ReadTransaction trans(FROM_HERE, dir);
          Entry e(&trans, GET_BY_HANDLE, step_->metahandle);
          CHECK(e.good());  // Failed due to ThreadBug1
        }
        directory_manager_->Close(dirname);
        break;
       }
       step_->number += 1;
       step_->condvar.Signal();
    }
  }

  DISALLOW_COPY_AND_ASSIGN(ThreadBugDelegate);
};

TEST_F(SyncableDirectoryManager, ThreadBug1) {
  Step step;
  step.number = 0;
  DirectoryManager dirman(FilePath(temp_dir_.path()));
  ThreadBugDelegate thread_delegate_1(0, &step, &dirman);
  ThreadBugDelegate thread_delegate_2(1, &step, &dirman);

  base::PlatformThreadHandle thread_handle_1;
  base::PlatformThreadHandle thread_handle_2;

  ASSERT_TRUE(
      base::PlatformThread::Create(0, &thread_delegate_1, &thread_handle_1));
  ASSERT_TRUE(
      base::PlatformThread::Create(0, &thread_delegate_2, &thread_handle_2));

  base::PlatformThread::Join(thread_handle_1);
  base::PlatformThread::Join(thread_handle_2);
}


// The in-memory information would get out of sync because a
// directory would be closed and re-opened, and then an old
// Directory::Kernel with stale information would get saved to the db.
class DirectoryKernelStalenessBugDelegate : public ThreadBugDelegate {
 public:
  DirectoryKernelStalenessBugDelegate(int role, Step* step,
                                               DirectoryManager* dirman)
    : ThreadBugDelegate(role, step, dirman) {}

  virtual void ThreadMain() {
    MessageLoop message_loop;
    const char test_bytes[] = "test data";
    const std::string dirname = "DirectoryKernelStalenessBug";
    base::AutoLock scoped_lock(step_->mutex);
    const Id jeff_id = TestIdFactory::FromNumber(100);

    while (step_->number < 4) {
      while (step_->number % 2 != role_) {
        step_->condvar.Wait();
      }
      switch (step_->number) {
      case 0:
        {
          // Clean up remnants of earlier test runs.
          file_util::Delete(directory_manager_->GetSyncDataDatabasePath(),
                            true);
          // Test.
          directory_manager_->Open(
              dirname, &delegate_, NullTransactionObserver());
          ScopedDirLookup dir(directory_manager_, dirname);
          CHECK(dir.good());
          WriteTransaction trans(FROM_HERE, UNITTEST, dir);
          MutableEntry me(&trans, CREATE, trans.root_id(), "Jeff");
          me.Put(BASE_VERSION, 1);
          me.Put(ID, jeff_id);
          PutDataAsBookmarkFavicon(&trans, &me, test_bytes,
                                     sizeof(test_bytes));
        }
        {
          ScopedDirLookup dir(directory_manager_, dirname);
          CHECK(dir.good());
          dir->SaveChanges();
        }
        directory_manager_->Close(dirname);
        break;
      case 1:
        {
          directory_manager_->Open(
              dirname, &delegate_, NullTransactionObserver());
          ScopedDirLookup dir(directory_manager_, dirname);
          CHECK(dir.good());
        }
        break;
      case 2:
        {
          ScopedDirLookup dir(directory_manager_, dirname);
          CHECK(dir.good());
        }
        break;
      case 3:
        {
          ScopedDirLookup dir(directory_manager_, dirname);
          CHECK(dir.good());
          ReadTransaction trans(FROM_HERE, dir);
          Entry e(&trans, GET_BY_ID, jeff_id);
          ExpectDataFromBookmarkFaviconEquals(&trans, &e, test_bytes,
                                                sizeof(test_bytes));
        }
        // Same result as CloseAllDirectories, but more code coverage.
        directory_manager_->Close(dirname);
        break;
      }
      step_->number += 1;
      step_->condvar.Signal();
    }
  }

  DISALLOW_COPY_AND_ASSIGN(DirectoryKernelStalenessBugDelegate);
};

TEST_F(SyncableDirectoryManager, DirectoryKernelStalenessBug) {
  Step step;

  DirectoryManager dirman(FilePath(temp_dir_.path()));
  DirectoryKernelStalenessBugDelegate thread_delegate_1(0, &step, &dirman);
  DirectoryKernelStalenessBugDelegate thread_delegate_2(1, &step, &dirman);

  base::PlatformThreadHandle thread_handle_1;
  base::PlatformThreadHandle thread_handle_2;

  ASSERT_TRUE(
      base::PlatformThread::Create(0, &thread_delegate_1, &thread_handle_1));
  ASSERT_TRUE(
      base::PlatformThread::Create(0, &thread_delegate_2, &thread_handle_2));

  base::PlatformThread::Join(thread_handle_1);
  base::PlatformThread::Join(thread_handle_2);
}

class StressTransactionsDelegate : public base::PlatformThread::Delegate {
 public:
  StressTransactionsDelegate(DirectoryManager* dm,
                             const std::string& dirname,
                             int thread_number)
      : directory_manager_(dm),
        dirname_(dirname),
        thread_number_(thread_number) {}

 private:
  DirectoryManager* const directory_manager_;
  std::string dirname_;
  const int thread_number_;

  // PlatformThread::Delegate methods:
  virtual void ThreadMain() {
    ScopedDirLookup dir(directory_manager_, dirname_);
    CHECK(dir.good());
    int entry_count = 0;
    std::string path_name;

    for (int i = 0; i < 20; ++i) {
      const int rand_action = rand() % 10;
      if (rand_action < 4 && !path_name.empty()) {
        ReadTransaction trans(FROM_HERE, dir);
        CHECK(1 == CountEntriesWithName(&trans, trans.root_id(), path_name));
        base::PlatformThread::Sleep(rand() % 10);
      } else {
        std::string unique_name =
            base::StringPrintf("%d.%d", thread_number_, entry_count++);
        path_name.assign(unique_name.begin(), unique_name.end());
        WriteTransaction trans(FROM_HERE, UNITTEST, dir);
        MutableEntry e(&trans, CREATE, trans.root_id(), path_name);
        CHECK(e.good());
        base::PlatformThread::Sleep(rand() % 20);
        e.Put(IS_UNSYNCED, true);
        if (e.Put(ID, TestIdFactory::FromNumber(rand())) &&
            e.Get(ID).ServerKnows() && !e.Get(ID).IsRoot()) {
           e.Put(BASE_VERSION, 1);
        }
      }
    }
  }

  DISALLOW_COPY_AND_ASSIGN(StressTransactionsDelegate);
};

TEST(SyncableDirectory, StressTransactions) {
  MessageLoop message_loop;
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  DirectoryManager dirman(FilePath(temp_dir.path()));
  std::string dirname = "stress";
  file_util::Delete(dirman.GetSyncDataDatabasePath(), true);
  NullDirectoryChangeDelegate delegate;
  dirman.Open(dirname, &delegate, NullTransactionObserver());

  const int kThreadCount = 7;
  base::PlatformThreadHandle threads[kThreadCount];
  scoped_ptr<StressTransactionsDelegate> thread_delegates[kThreadCount];

  for (int i = 0; i < kThreadCount; ++i) {
    thread_delegates[i].reset(
        new StressTransactionsDelegate(&dirman, dirname, i));
    ASSERT_TRUE(base::PlatformThread::Create(
        0, thread_delegates[i].get(), &threads[i]));
  }

  for (int i = 0; i < kThreadCount; ++i) {
    base::PlatformThread::Join(threads[i]);
  }

  dirman.Close(dirname);
  file_util::Delete(dirman.GetSyncDataDatabasePath(), true);
}

class SyncableClientTagTest : public SyncableDirectoryTest {
 public:
  static const int kBaseVersion = 1;
  const char* test_name_;
  const char* test_tag_;

  SyncableClientTagTest() : test_name_("test_name"), test_tag_("dietcoke") {}

  bool CreateWithDefaultTag(Id id, bool deleted) {
    return CreateWithTag(test_tag_, id, deleted);
  }

  // Attempt to create an entry with a default tag.
  bool CreateWithTag(const char* tag, Id id, bool deleted) {
    WriteTransaction wtrans(FROM_HERE, UNITTEST, dir_.get());
    MutableEntry me(&wtrans, CREATE, wtrans.root_id(), test_name_);
    CHECK(me.good());
    me.Put(ID, id);
    if (id.ServerKnows()) {
      me.Put(BASE_VERSION, kBaseVersion);
    }
    me.Put(IS_DEL, deleted);
    me.Put(IS_UNSYNCED, true);
    me.Put(IS_DIR, false);
    return me.Put(UNIQUE_CLIENT_TAG, tag);
  }

  // Verify an entry exists with the default tag.
  void VerifyTag(Id id, bool deleted) {
    // Should still be present and valid in the client tag index.
    ReadTransaction trans(FROM_HERE, dir_.get());
    Entry me(&trans, GET_BY_CLIENT_TAG, test_tag_);
    CHECK(me.good());
    EXPECT_EQ(me.Get(ID), id);
    EXPECT_EQ(me.Get(UNIQUE_CLIENT_TAG), test_tag_);
    EXPECT_EQ(me.Get(IS_DEL), deleted);
    EXPECT_EQ(me.Get(IS_UNSYNCED), true);
  }

 protected:
  TestIdFactory factory_;
};

TEST_F(SyncableClientTagTest, TestClientTagClear) {
  Id server_id = factory_.NewServerId();
  EXPECT_TRUE(CreateWithDefaultTag(server_id, false));
  {
    WriteTransaction trans(FROM_HERE, UNITTEST, dir_.get());
    MutableEntry me(&trans, GET_BY_CLIENT_TAG, test_tag_);
    EXPECT_TRUE(me.good());
    me.Put(UNIQUE_CLIENT_TAG, "");
  }
  {
    ReadTransaction trans(FROM_HERE, dir_.get());
    Entry by_tag(&trans, GET_BY_CLIENT_TAG, test_tag_);
    EXPECT_FALSE(by_tag.good());

    Entry by_id(&trans, GET_BY_ID, server_id);
    EXPECT_TRUE(by_id.good());
    EXPECT_TRUE(by_id.Get(UNIQUE_CLIENT_TAG).empty());
  }
}

TEST_F(SyncableClientTagTest, TestClientTagIndexServerId) {
  Id server_id = factory_.NewServerId();
  EXPECT_TRUE(CreateWithDefaultTag(server_id, false));
  VerifyTag(server_id, false);
}

TEST_F(SyncableClientTagTest, TestClientTagIndexClientId) {
  Id client_id = factory_.NewLocalId();
  EXPECT_TRUE(CreateWithDefaultTag(client_id, false));
  VerifyTag(client_id, false);
}

TEST_F(SyncableClientTagTest, TestDeletedClientTagIndexClientId) {
  Id client_id = factory_.NewLocalId();
  EXPECT_TRUE(CreateWithDefaultTag(client_id, true));
  VerifyTag(client_id, true);
}

TEST_F(SyncableClientTagTest, TestDeletedClientTagIndexServerId) {
  Id server_id = factory_.NewServerId();
  EXPECT_TRUE(CreateWithDefaultTag(server_id, true));
  VerifyTag(server_id, true);
}

TEST_F(SyncableClientTagTest, TestClientTagIndexDuplicateServer) {
  EXPECT_TRUE(CreateWithDefaultTag(factory_.NewServerId(), true));
  EXPECT_FALSE(CreateWithDefaultTag(factory_.NewServerId(), true));
  EXPECT_FALSE(CreateWithDefaultTag(factory_.NewServerId(), false));
  EXPECT_FALSE(CreateWithDefaultTag(factory_.NewLocalId(), false));
  EXPECT_FALSE(CreateWithDefaultTag(factory_.NewLocalId(), true));
}

}  // namespace
}  // namespace syncable
