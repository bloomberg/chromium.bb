// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/syncable/syncable.h"

#include "build/build_config.h"

#include <sys/types.h>

#include <iostream>
#include <limits>
#include <string>

// TODO(ncarter): Winnow down the OS-specific includes from the test
// file.
#if defined(OS_WIN)
#include <tchar.h>
#include <atlbase.h>
#include <process.h>
#endif  // defined(OS_WIN)

#if !defined(OS_WIN)
#define MAX_PATH PATH_MAX
#include <ostream>
#include <stdio.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/times.h>
#endif  // !defined(OS_WIN)

#include "base/at_exit.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/platform_thread.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/sync/syncable/directory_backing_store.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/browser/sync/util/closure.h"
#include "chrome/browser/sync/util/event_sys-inl.h"
#include "chrome/browser/sync/util/path_helpers.h"
#include "chrome/browser/sync/util/query_helpers.h"
#include "chrome/test/sync/engine/test_id_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/sqlite/preprocessed/sqlite3.h"

using browser_sync::TestIdFactory;
using std::cout;
using std::endl;
using std::string;

namespace syncable {

// A lot of these tests were written expecting to be able to read and write
// object data on entries.  However, the design has changed.
void PutDataAsExtendedAttribute(WriteTransaction* wtrans,
                                MutableEntry* e,
                                const char* bytes,
                                size_t bytes_length) {
  ExtendedAttributeKey key(e->Get(META_HANDLE), PSTR("DATA"));
  MutableExtendedAttribute attr(wtrans, CREATE, key);
  Blob bytes_blob(bytes, bytes + bytes_length);
  attr.mutable_value()->swap(bytes_blob);
}

void ExpectDataFromExtendedAttributeEquals(BaseTransaction* trans,
                                           Entry* e,
                                           const char* bytes,
                                           size_t bytes_length) {
  Blob expected_value(bytes, bytes + bytes_length);
  ExtendedAttributeKey key(e->Get(META_HANDLE), PSTR("DATA"));
  ExtendedAttribute attr(trans, GET_BY_HANDLE, key);
  EXPECT_FALSE(attr.is_deleted());
  EXPECT_EQ(expected_value, attr.value());
}


TEST(Syncable, General) {
  remove("SimpleTest.sqlite3");
  Directory dir;
  FilePath test_db(FILE_PATH_LITERAL("SimpleTest.sqlite3"));
  dir.Open(test_db, PSTR("SimpleTest"));
  bool entry_exists = false;
  int64 metahandle;
  const Id id = TestIdFactory::FromNumber(99);
  // Test simple read operations.
  {
    ReadTransaction rtrans(&dir, __FILE__, __LINE__);
    Entry e(&rtrans, GET_BY_ID, id);
    if (e.good()) {
      entry_exists = true;
      metahandle = e.Get(META_HANDLE);
    }
    Directory::ChildHandles child_handles;
    dir.GetChildHandles(&rtrans, rtrans.root_id(), &child_handles);
    for (Directory::ChildHandles::iterator i = child_handles.begin();
         i != child_handles.end(); ++i)
      cout << *i << endl;

    Entry e2(&rtrans, GET_BY_PATH, PSTR("/Hello\\World/"));
  }

  // Test creating a new meta entry.
  {
    WriteTransaction wtrans(&dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry me(&wtrans, CREATE, wtrans.root_id(), PSTR("Jeff"));
    ASSERT_TRUE(entry_exists ? !me.good() : me.good());
    if (me.good()) {
      me.Put(ID, id);
      me.Put(BASE_VERSION, 1);
      metahandle = me.Get(META_HANDLE);
    }
  }

  // Test writing data to an entity.
  static const char s[] = "Hello World.";
  {
    WriteTransaction trans(&dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry e(&trans, GET_BY_PATH,
                   PathString(kPathSeparator) + PSTR("Jeff"));
    ASSERT_TRUE(e.good());
    PutDataAsExtendedAttribute(&trans, &e, s, sizeof(s));
  }

  // Test reading back the name contents that we just wrote.
  {
    WriteTransaction trans(&dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry e(&trans, GET_BY_PATH,
                   PathString(kPathSeparator) + PSTR("Jeff"));
    ASSERT_TRUE(e.good());
    ExpectDataFromExtendedAttributeEquals(&trans, &e, s, sizeof(s));
  }

  // Now delete it.
  {
    WriteTransaction trans(&dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry e(&trans, CREATE, trans.root_id(), PSTR("New File"));
    e.Put(IS_DEL, true);
  }

  dir.SaveChanges();
}

TEST(Syncable, NameClassTest) {
  const PathString foo(PSTR("foo"));
  const PathString bar(PSTR("bar"));

  Name name1(foo);
  EXPECT_EQ(name1.value(), foo);
  EXPECT_EQ(name1.db_value(), foo);
  EXPECT_FALSE(name1.HasBeenSanitized());
  EXPECT_TRUE(name1.GetUnsanitizedName().empty());

  Name name2(foo, foo);
  EXPECT_EQ(name2.value(), foo);
  EXPECT_EQ(name2.db_value(), foo);
  EXPECT_FALSE(name2.HasBeenSanitized());
  EXPECT_TRUE(name2.GetUnsanitizedName().empty());

  Name name3(foo, bar);
  EXPECT_EQ(name3.value(), bar);
  EXPECT_EQ(name3.db_value(), foo);
  EXPECT_TRUE(name3.HasBeenSanitized());
  EXPECT_EQ(name3.GetUnsanitizedName(), bar);

  EXPECT_TRUE(name1 == name2);
  EXPECT_FALSE(name1 != name2);
  EXPECT_FALSE(name2 == name3);
  EXPECT_TRUE(name2 != name3);
}

namespace {

// A Directory whose backing store always fails SaveChanges by returning false.
class TestUnsaveableDirectory : public Directory {
 public:
  class UnsaveableBackingStore : public DirectoryBackingStore {
   public:
     UnsaveableBackingStore(const PathString& dir_name,
                            const FilePath& backing_filepath)
         : DirectoryBackingStore(dir_name, backing_filepath) { }
     virtual bool SaveChanges(const Directory::SaveChangesSnapshot& snapshot) {
       return false;
     }
  };
  virtual DirectoryBackingStore* CreateBackingStore(
      const PathString& dir_name, const FilePath& backing_filepath) {
    return new UnsaveableBackingStore(dir_name, backing_filepath);
  }
};

// Test suite for syncable::Directory.
class SyncableDirectoryTest : public testing::Test {
 protected:
  static const FilePath::CharType kFilePath[];
  static const PathString kName;
  static const Id kId;

  // SetUp() is called before each test case is run.
  // The sqlite3 DB is deleted before each test is run.
  virtual void SetUp() {
    file_path_ = FilePath(kFilePath);
    file_util::Delete(file_path_, true);
    dir_.reset(new Directory());
    ASSERT_TRUE(dir_.get());
    ASSERT_TRUE(OPENED == dir_->Open(file_path_, kName));
    ASSERT_TRUE(dir_->good());
  }

  virtual void TearDown() {
    // This also closes file handles.
    dir_->SaveChanges();
    dir_.reset();
    file_util::Delete(file_path_, true);
  }

  scoped_ptr<Directory> dir_;
  FilePath file_path_;

  // Creates an empty entry and sets the ID field to the default kId.
  void CreateEntry(const PathString &entryname) {
    CreateEntry(entryname, kId);
  }

  // Creates an empty entry and sets the ID field to id.
  void CreateEntry(const PathString &entryname, const int id) {
    CreateEntry(entryname, TestIdFactory::FromNumber(id));
  }
  void CreateEntry(const PathString &entryname, Id id) {
    WriteTransaction wtrans(dir_.get(), UNITTEST, __FILE__, __LINE__);
    MutableEntry me(&wtrans, CREATE, wtrans.root_id(), entryname);
    ASSERT_TRUE(me.good());
    me.Put(ID, id);
    me.Put(IS_UNSYNCED, true);
  }

  void ValidateEntry(BaseTransaction* trans, int64 id, bool check_name,
      PathString name, int64 base_version, int64 server_version, bool is_del);
  void CreateAndCheck(WriteTransaction* trans, int64 parent_id, int64 id,
      PathString name, PathString server_name, int64 version,
      bool set_server_fields, bool is_dir, bool add_to_lru, int64 *meta_handle);
};

const FilePath::CharType SyncableDirectoryTest::kFilePath[] =
    FILE_PATH_LITERAL("Test.sqlite3");
const PathString SyncableDirectoryTest::kName(PSTR("Foo"));
const Id SyncableDirectoryTest::kId(TestIdFactory::FromNumber(-99));

TEST_F(SyncableDirectoryTest, TestBasicLookupNonExistantID) {
  ReadTransaction rtrans(dir_.get(), __FILE__, __LINE__);
  Entry e(&rtrans, GET_BY_ID, kId);
  ASSERT_FALSE(e.good());
}

TEST_F(SyncableDirectoryTest, TestBasicLookupValidID) {
  CreateEntry(PSTR("rtc"));
  ReadTransaction rtrans(dir_.get(), __FILE__, __LINE__);
  Entry e(&rtrans, GET_BY_ID, kId);
  ASSERT_TRUE(e.good());
}

TEST_F(SyncableDirectoryTest, TestBasicCaseSensitivity) {
  PathString name = PSTR("RYAN");
  PathString conflicting_name = PSTR("ryan");
  CreateEntry(name);

  WriteTransaction wtrans(dir_.get(), UNITTEST, __FILE__, __LINE__);
  MutableEntry me(&wtrans, CREATE, wtrans.root_id(), conflicting_name);
  ASSERT_FALSE(me.good());
}

TEST_F(SyncableDirectoryTest, TestDelete) {
  PathString name = PSTR("peanut butter jelly time");
  WriteTransaction trans(dir_.get(), UNITTEST, __FILE__, __LINE__);
  MutableEntry e1(&trans, CREATE, trans.root_id(), name);
  ASSERT_TRUE(e1.good());
  ASSERT_TRUE(e1.Put(IS_DEL, true));
  MutableEntry e2(&trans, CREATE, trans.root_id(), name);
  ASSERT_TRUE(e2.good());
  ASSERT_TRUE(e2.Put(IS_DEL, true));
  MutableEntry e3(&trans, CREATE, trans.root_id(), name);
  ASSERT_TRUE(e3.good());
  ASSERT_TRUE(e3.Put(IS_DEL, true));

  ASSERT_TRUE(e3.Put(IS_DEL, false));
  ASSERT_FALSE(e1.Put(IS_DEL, false));
  ASSERT_FALSE(e2.Put(IS_DEL, false));
  ASSERT_TRUE(e3.Put(IS_DEL, true));

  ASSERT_TRUE(e1.Put(IS_DEL, false));
  ASSERT_FALSE(e2.Put(IS_DEL, false));
  ASSERT_FALSE(e3.Put(IS_DEL, false));
  ASSERT_TRUE(e1.Put(IS_DEL, true));
}

TEST_F(SyncableDirectoryTest, TestGetUnsynced) {
  Directory::UnsyncedMetaHandles handles;
  int64 handle1, handle2;
  {
    WriteTransaction trans(dir_.get(), UNITTEST, __FILE__, __LINE__);

    dir_->GetUnsyncedMetaHandles(&trans, &handles);
    ASSERT_TRUE(0 == handles.size());

    MutableEntry e1(&trans, CREATE, trans.root_id(), PSTR("abba"));
    ASSERT_TRUE(e1.good());
    handle1 = e1.Get(META_HANDLE);
    e1.Put(BASE_VERSION, 1);
    e1.Put(IS_DIR, true);
    e1.Put(ID, TestIdFactory::FromNumber(101));

    MutableEntry e2(&trans, CREATE, e1.Get(ID), PSTR("bread"));
    ASSERT_TRUE(e2.good());
    handle2 = e2.Get(META_HANDLE);
    e2.Put(BASE_VERSION, 1);
    e2.Put(ID, TestIdFactory::FromNumber(102));
  }
  dir_->SaveChanges();
  {
    WriteTransaction trans(dir_.get(), UNITTEST, __FILE__, __LINE__);

    dir_->GetUnsyncedMetaHandles(&trans, &handles);
    ASSERT_TRUE(0 == handles.size());

    MutableEntry e3(&trans, GET_BY_HANDLE, handle1);
    ASSERT_TRUE(e3.good());
    e3.Put(IS_UNSYNCED, true);
  }
  dir_->SaveChanges();
  {
    WriteTransaction trans(dir_.get(), UNITTEST, __FILE__, __LINE__);
    dir_->GetUnsyncedMetaHandles(&trans, &handles);
    ASSERT_TRUE(1 == handles.size());
    ASSERT_TRUE(handle1 == handles[0]);

    MutableEntry e4(&trans, GET_BY_HANDLE, handle2);
    ASSERT_TRUE(e4.good());
    e4.Put(IS_UNSYNCED, true);
  }
  dir_->SaveChanges();
  {
    WriteTransaction trans(dir_.get(), UNITTEST, __FILE__, __LINE__);
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
    WriteTransaction trans(dir_.get(), UNITTEST, __FILE__, __LINE__);
    dir_->GetUnsyncedMetaHandles(&trans, &handles);
    ASSERT_TRUE(1 == handles.size());
    ASSERT_TRUE(handle2 == handles[0]);
  }
}

TEST_F(SyncableDirectoryTest, TestGetUnappliedUpdates) {
  Directory::UnappliedUpdateMetaHandles handles;
  int64 handle1, handle2;
  {
    WriteTransaction trans(dir_.get(), UNITTEST, __FILE__, __LINE__);

    dir_->GetUnappliedUpdateMetaHandles(&trans, &handles);
    ASSERT_TRUE(0 == handles.size());

    MutableEntry e1(&trans, CREATE, trans.root_id(), PSTR("abba"));
    ASSERT_TRUE(e1.good());
    handle1 = e1.Get(META_HANDLE);
    e1.Put(IS_UNAPPLIED_UPDATE, false);
    e1.Put(BASE_VERSION, 1);
    e1.Put(ID, TestIdFactory::FromNumber(101));
    e1.Put(IS_DIR, true);

    MutableEntry e2(&trans, CREATE, e1.Get(ID), PSTR("bread"));
    ASSERT_TRUE(e2.good());
    handle2 = e2.Get(META_HANDLE);
    e2.Put(IS_UNAPPLIED_UPDATE, false);
    e2.Put(BASE_VERSION, 1);
    e2.Put(ID, TestIdFactory::FromNumber(102));
  }
  dir_->SaveChanges();
  {
    WriteTransaction trans(dir_.get(), UNITTEST, __FILE__, __LINE__);

    dir_->GetUnappliedUpdateMetaHandles(&trans, &handles);
    ASSERT_TRUE(0 == handles.size());

    MutableEntry e3(&trans, GET_BY_HANDLE, handle1);
    ASSERT_TRUE(e3.good());
    e3.Put(IS_UNAPPLIED_UPDATE, true);
  }
  dir_->SaveChanges();
  {
    WriteTransaction trans(dir_.get(), UNITTEST, __FILE__, __LINE__);
    dir_->GetUnappliedUpdateMetaHandles(&trans, &handles);
    ASSERT_TRUE(1 == handles.size());
    ASSERT_TRUE(handle1 == handles[0]);

    MutableEntry e4(&trans, GET_BY_HANDLE, handle2);
    ASSERT_TRUE(e4.good());
    e4.Put(IS_UNAPPLIED_UPDATE, true);
  }
  dir_->SaveChanges();
  {
    WriteTransaction trans(dir_.get(), UNITTEST, __FILE__, __LINE__);
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
    WriteTransaction trans(dir_.get(), UNITTEST, __FILE__, __LINE__);
    dir_->GetUnappliedUpdateMetaHandles(&trans, &handles);
    ASSERT_TRUE(1 == handles.size());
    ASSERT_TRUE(handle2 == handles[0]);
  }
}


TEST_F(SyncableDirectoryTest, DeleteBug_531383) {
  // Try to evoke a check failure...
  TestIdFactory id_factory;
  int64 grandchild_handle, twin_handle;
  {
    WriteTransaction wtrans(dir_.get(), UNITTEST, __FILE__, __LINE__);
    MutableEntry parent(&wtrans, CREATE, id_factory.root(), PSTR("Bob"));
    ASSERT_TRUE(parent.good());
    parent.Put(IS_DIR, true);
    parent.Put(ID, id_factory.NewServerId());
    parent.Put(BASE_VERSION, 1);
    MutableEntry child(&wtrans, CREATE, parent.Get(ID), PSTR("Bob"));
    ASSERT_TRUE(child.good());
    child.Put(IS_DIR, true);
    child.Put(ID, id_factory.NewServerId());
    child.Put(BASE_VERSION, 1);
    MutableEntry grandchild(&wtrans, CREATE, child.Get(ID), PSTR("Bob"));
    ASSERT_TRUE(grandchild.good());
    grandchild.Put(ID, id_factory.NewServerId());
    grandchild.Put(BASE_VERSION, 1);
    ASSERT_TRUE(grandchild.Put(IS_DEL, true));
    MutableEntry twin(&wtrans, CREATE, child.Get(ID), PSTR("Bob"));
    ASSERT_TRUE(twin.good());
    ASSERT_TRUE(twin.Put(IS_DEL, true));
    ASSERT_TRUE(grandchild.Put(IS_DEL, false));
    ASSERT_FALSE(twin.Put(IS_DEL, false));
    grandchild_handle = grandchild.Get(META_HANDLE);
    twin_handle = twin.Get(META_HANDLE);
  }
  dir_->SaveChanges();
  {
    WriteTransaction wtrans(dir_.get(), UNITTEST, __FILE__, __LINE__);
    MutableEntry grandchild(&wtrans, GET_BY_HANDLE, grandchild_handle);
    grandchild.Put(IS_DEL, true);  // Used to CHECK fail here.
  }
}

static inline bool IsLegalNewParent(const Entry& a, const Entry& b) {
  return IsLegalNewParent(a.trans(), a.Get(ID), b.Get(ID));
}

TEST_F(SyncableDirectoryTest, TestIsLegalNewParent) {
  TestIdFactory id_factory;
  WriteTransaction wtrans(dir_.get(), UNITTEST, __FILE__, __LINE__);
  Entry root(&wtrans, GET_BY_ID, id_factory.root());
  ASSERT_TRUE(root.good());
  MutableEntry parent(&wtrans, CREATE, root.Get(ID), PSTR("Bob"));
  ASSERT_TRUE(parent.good());
  parent.Put(IS_DIR, true);
  parent.Put(ID, id_factory.NewServerId());
  parent.Put(BASE_VERSION, 1);
  MutableEntry child(&wtrans, CREATE, parent.Get(ID), PSTR("Bob"));
  ASSERT_TRUE(child.good());
  child.Put(IS_DIR, true);
  child.Put(ID, id_factory.NewServerId());
  child.Put(BASE_VERSION, 1);
  MutableEntry grandchild(&wtrans, CREATE, child.Get(ID), PSTR("Bob"));
  ASSERT_TRUE(grandchild.good());
  grandchild.Put(ID, id_factory.NewServerId());
  grandchild.Put(BASE_VERSION, 1);

  MutableEntry parent2(&wtrans, CREATE, root.Get(ID), PSTR("Pete"));
  ASSERT_TRUE(parent2.good());
  parent2.Put(IS_DIR, true);
  parent2.Put(ID, id_factory.NewServerId());
  parent2.Put(BASE_VERSION, 1);
  MutableEntry child2(&wtrans, CREATE, parent2.Get(ID), PSTR("Pete"));
  ASSERT_TRUE(child2.good());
  child2.Put(IS_DIR, true);
  child2.Put(ID, id_factory.NewServerId());
  child2.Put(BASE_VERSION, 1);
  MutableEntry grandchild2(&wtrans, CREATE, child2.Get(ID), PSTR("Pete"));
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

TEST_F(SyncableDirectoryTest, TestFindEntryInFolder) {
  // Create a subdir and an entry.
  int64 entry_handle;
  {
    WriteTransaction trans(dir_.get(), UNITTEST, __FILE__, __LINE__);
    MutableEntry folder(&trans, CREATE, trans.root_id(), PSTR("folder"));
    ASSERT_TRUE(folder.good());
    EXPECT_TRUE(folder.Put(IS_DIR, true));
    EXPECT_TRUE(folder.Put(IS_UNSYNCED, true));
    MutableEntry entry(&trans, CREATE, folder.Get(ID), PSTR("entry"));
    ASSERT_TRUE(entry.good());
    entry_handle = entry.Get(META_HANDLE);
    entry.Put(IS_UNSYNCED, true);
  }

  // Make sure we can find the entry in the folder.
  {
    ReadTransaction trans(dir_.get(), __FILE__, __LINE__);
    Entry entry(&trans, GET_BY_PATH, PathString(kPathSeparator) +
                                    PSTR("folder") +
                                    kPathSeparator + PSTR("entry"));
    ASSERT_TRUE(entry.good());
    ASSERT_TRUE(entry.Get(META_HANDLE) == entry_handle);
  }
}

TEST_F(SyncableDirectoryTest, TestGetByParentIdAndName) {
  PathString name = PSTR("Bob");
  Id id = TestIdFactory::MakeServer("ID for Bob");
  {
    WriteTransaction wtrans(dir_.get(), UNITTEST, __FILE__, __LINE__);
    MutableEntry entry(&wtrans, CREATE, wtrans.root_id() /*entry id*/, name);
    ASSERT_TRUE(entry.good());
    entry.Put(IS_DIR, true);
    entry.Put(ID, id);
    entry.Put(BASE_VERSION, 1);
    entry.Put(IS_UNSYNCED, true);
  }
  {
    WriteTransaction wtrans(dir_.get(), UNITTEST, __FILE__, __LINE__);
    MutableEntry entry(&wtrans, GET_BY_PARENTID_AND_NAME, wtrans.root_id(),
                       name);
    ASSERT_TRUE(entry.good());
    ASSERT_TRUE(id == entry.Get(ID));
  }
  {
    ReadTransaction trans(dir_.get(), __FILE__, __LINE__);
    Entry entry(&trans, GET_BY_PARENTID_AND_NAME, trans.root_id(), name);
    ASSERT_TRUE(entry.good());
    ASSERT_TRUE(id == entry.Get(ID));
  }
}

TEST_F(SyncableDirectoryTest, TestParentIDIndexUpdate) {
  WriteTransaction wt(dir_.get(), UNITTEST, __FILE__, __LINE__);
  MutableEntry folder(&wt, CREATE, wt.root_id(), PSTR("oldname"));
  folder.Put(NAME, PSTR("newname"));
  folder.Put(IS_UNSYNCED, true);
  Entry entry(&wt, GET_BY_PATH, PSTR("newname"));
  ASSERT_TRUE(entry.good());
}

TEST_F(SyncableDirectoryTest, TestNoReindexDeletedItems) {
  WriteTransaction trans(dir_.get(), UNITTEST, __FILE__, __LINE__);
  MutableEntry folder(&trans, CREATE, trans.root_id(), PSTR("folder"));
  ASSERT_TRUE(folder.good());
  ASSERT_TRUE(folder.Put(IS_DIR, true));
  ASSERT_TRUE(folder.Put(IS_DEL, true));
  Entry gone(&trans, GET_BY_PARENTID_AND_NAME, trans.root_id(), PSTR("folder"));
  ASSERT_FALSE(gone.good());
  ASSERT_TRUE(folder.PutParentIdAndName(trans.root_id(),
                                        Name(PSTR("new_name"))));
}

TEST_F(SyncableDirectoryTest, TestCaseChangeRename) {
  WriteTransaction trans(dir_.get(), UNITTEST, __FILE__, __LINE__);
  MutableEntry folder(&trans, CREATE, trans.root_id(), PSTR("CaseChange"));
  ASSERT_TRUE(folder.good());
  EXPECT_TRUE(folder.PutParentIdAndName(trans.root_id(),
                                        Name(PSTR("CASECHANGE"))));
  EXPECT_TRUE(folder.Put(IS_DEL, true));
}

TEST_F(SyncableDirectoryTest, TestShareInfo) {
  dir_->set_last_sync_timestamp(100);
  dir_->set_store_birthday("Jan 31st");
  {
    ReadTransaction trans(dir_.get(), __FILE__, __LINE__);
    EXPECT_EQ(100, dir_->last_sync_timestamp());
    EXPECT_EQ("Jan 31st", dir_->store_birthday());
  }
  dir_->set_last_sync_timestamp(200);
  dir_->set_store_birthday("April 10th");
  dir_->SaveChanges();
  {
    ReadTransaction trans(dir_.get(), __FILE__, __LINE__);
    EXPECT_EQ(200, dir_->last_sync_timestamp());
    EXPECT_EQ("April 10th", dir_->store_birthday());
  }
}

TEST_F(SyncableDirectoryTest, TestSimpleFieldsPreservedDuringSaveChanges) {
  Id id = TestIdFactory::FromNumber(1);
  EntryKernel create_pre_save, update_pre_save;
  EntryKernel create_post_save, update_post_save;
  {
    WriteTransaction trans(dir_.get(), UNITTEST, __FILE__, __LINE__);
    MutableEntry create(&trans, CREATE, trans.root_id(), PSTR("Create"));
    MutableEntry update(&trans, CREATE_NEW_UPDATE_ITEM, id);
    create.Put(IS_UNSYNCED, true);
    update.Put(IS_UNAPPLIED_UPDATE, true);
    create_pre_save = create.GetKernelCopy();
    update_pre_save = update.GetKernelCopy();
  }
  dir_->SaveChanges();
  {
    ReadTransaction trans(dir_.get(), __FILE__, __LINE__);
    Entry create(&trans, GET_BY_PARENTID_AND_NAME, trans.root_id(),
                 PSTR("Create"));
    Entry update(&trans, GET_BY_ID, id);
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
}

TEST_F(SyncableDirectoryTest, TestSaveChangesFailure) {
  int64 handle1 = 0;
  {
    WriteTransaction trans(dir_.get(), UNITTEST, __FILE__, __LINE__);

    MutableEntry e1(&trans, CREATE, trans.root_id(), PSTR("aguilera"));
    ASSERT_TRUE(e1.good());
    handle1 = e1.Get(META_HANDLE);
    e1.Put(BASE_VERSION, 1);
    e1.Put(IS_DIR, true);
    e1.Put(ID, TestIdFactory::FromNumber(101));
  }
  ASSERT_TRUE(dir_->SaveChanges());

  dir_.reset(new TestUnsaveableDirectory());
  ASSERT_TRUE(dir_.get());
  ASSERT_TRUE(OPENED == dir_->Open(FilePath(kFilePath), kName));
  ASSERT_TRUE(dir_->good());
  int64 handle2 = 0;
  {
    WriteTransaction trans(dir_.get(), UNITTEST, __FILE__, __LINE__);

    MutableEntry aguilera(&trans, GET_BY_HANDLE, handle1);
    ASSERT_TRUE(aguilera.good());
    aguilera.Put(NAME, PSTR("christina"));
    ASSERT_TRUE(aguilera.GetKernelCopy().dirty[NAME]);

    MutableEntry kids_on_block(&trans, CREATE, trans.root_id(), PSTR("kids"));
    ASSERT_TRUE(kids_on_block.good());
    handle2 = kids_on_block.Get(META_HANDLE);
    kids_on_block.Put(BASE_VERSION, 1);
    kids_on_block.Put(IS_DIR, true);
    kids_on_block.Put(ID, TestIdFactory::FromNumber(102));
    EXPECT_TRUE(kids_on_block.Get(IS_NEW));
  }

  // We are using an unsaveable directory, so this can't succeed.  However,
  // the HandleSaveChangesFailure code path should have been triggered.
  ASSERT_FALSE(dir_->SaveChanges());

  // Make sure things were rolled back and the world is as it was before call.
  {
     ReadTransaction trans(dir_.get(), __FILE__, __LINE__);
     Entry e1(&trans, GET_BY_HANDLE, handle1);
     ASSERT_TRUE(e1.good());
     const EntryKernel& aguilera = e1.GetKernelCopy();
     Entry kids_on_block(&trans, GET_BY_HANDLE, handle2);
     ASSERT_TRUE(kids_on_block.good());

     EXPECT_TRUE(aguilera.dirty[NAME]);
     EXPECT_TRUE(kids_on_block.Get(IS_NEW));
  }
}


void SyncableDirectoryTest::ValidateEntry(BaseTransaction* trans, int64 id,
    bool check_name, PathString name, int64 base_version, int64 server_version,
    bool is_del) {
  Entry e(trans, GET_BY_ID, TestIdFactory::FromNumber(id));
  ASSERT_TRUE(e.good());
  if (check_name)
    ASSERT_TRUE(name == e.Get(NAME));
  ASSERT_TRUE(base_version == e.Get(BASE_VERSION));
  ASSERT_TRUE(server_version == e.Get(SERVER_VERSION));
  ASSERT_TRUE(is_del == e.Get(IS_DEL));
}

TEST(SyncableDirectoryManager, TestFileRelease) {
  DirectoryManager dm(FilePath(FILE_PATH_LITERAL(".")));
  ASSERT_TRUE(dm.Open(PSTR("ScopeTest")));
  {
    ScopedDirLookup(&dm, PSTR("ScopeTest"));
  }
  dm.Close(PSTR("ScopeTest"));
  ASSERT_TRUE(file_util::Delete(dm.GetSyncDataDatabasePath(), true));
}

class ThreadOpenTestDelegate : public PlatformThread::Delegate {
  public:
   explicit ThreadOpenTestDelegate(DirectoryManager* dm)
       : directory_manager_(dm) {}
   DirectoryManager* const directory_manager_;

  private:
   // PlatformThread::Delegate methods:
   virtual void ThreadMain() {
    CHECK(directory_manager_->Open(PSTR("Open")));
   }

  DISALLOW_COPY_AND_ASSIGN(ThreadOpenTestDelegate);
};

TEST(SyncableDirectoryManager, ThreadOpenTest) {
  DirectoryManager dm(FilePath(FILE_PATH_LITERAL(".")));
  PlatformThreadHandle thread_handle;
  ThreadOpenTestDelegate test_delegate(&dm);
  ASSERT_TRUE(PlatformThread::Create(0, &test_delegate, &thread_handle));
  PlatformThread::Join(thread_handle);
  {
    ScopedDirLookup dir(&dm, PSTR("Open"));
    ASSERT_TRUE(dir.good());
  }
  dm.Close(PSTR("Open"));
  ScopedDirLookup dir(&dm, PSTR("Open"));
  ASSERT_FALSE(dir.good());
}

struct Step {
  Step() : condvar(&mutex), number(0) {}

  Lock mutex;
  ConditionVariable condvar;
  int number;
  int64 metahandle;
};

class ThreadBugDelegate : public PlatformThread::Delegate {
  public:
   // a role is 0 or 1, meaning this thread does the odd or event steps.
   ThreadBugDelegate(int role, Step* step, DirectoryManager* dirman)
       : role_(role), step_(step), directory_manager_(dirman) {}

  protected:
   const int role_;
   Step* const step_;
   DirectoryManager* const directory_manager_;

   // PlatformThread::Delegate methods:
   virtual void ThreadMain() {
     const PathString dirname = PSTR("ThreadBug1");
     AutoLock scoped_lock(step_->mutex);

     while (step_->number < 3) {
       while (step_->number % 2 != role_) {
         step_->condvar.Wait();
       }
       switch (step_->number) {
      case 0:
        directory_manager_->Open(dirname);
        break;
      case 1:
        {
          directory_manager_->Close(dirname);
          directory_manager_->Open(dirname);
          ScopedDirLookup dir(directory_manager_, dirname);
          CHECK(dir.good());
          WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
          MutableEntry me(&trans, CREATE, trans.root_id(), PSTR("Jeff"));
          step_->metahandle = me.Get(META_HANDLE);
          me.Put(IS_UNSYNCED, true);
        }
        break;
      case 2:
        {
          ScopedDirLookup dir(directory_manager_, dirname);
          CHECK(dir.good());
          ReadTransaction trans(dir, __FILE__, __LINE__);
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

TEST(SyncableDirectoryManager, ThreadBug1) {
  Step step;
  step.number = 0;
  DirectoryManager dirman(FilePath(FILE_PATH_LITERAL(".")));
  ThreadBugDelegate thread_delegate_1(0, &step, &dirman);
  ThreadBugDelegate thread_delegate_2(1, &step, &dirman);

  PlatformThreadHandle thread_handle_1;
  PlatformThreadHandle thread_handle_2;

  ASSERT_TRUE(PlatformThread::Create(0, &thread_delegate_1, &thread_handle_1));
  ASSERT_TRUE(PlatformThread::Create(0, &thread_delegate_2, &thread_handle_2));

  PlatformThread::Join(thread_handle_1);
  PlatformThread::Join(thread_handle_2);
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
    const char test_bytes[] = "test data";
    const PathString dirname = PSTR("DirectoryKernelStalenessBug");
    AutoLock scoped_lock(step_->mutex);

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
          directory_manager_->Open(dirname);
          ScopedDirLookup dir(directory_manager_, dirname);
          CHECK(dir.good());
          WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
          MutableEntry me(&trans, CREATE, trans.root_id(), PSTR("Jeff"));
          me.Put(BASE_VERSION, 1);
          me.Put(ID, TestIdFactory::FromNumber(100));
          PutDataAsExtendedAttribute(&trans, &me, test_bytes,
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
          directory_manager_->Open(dirname);
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
          ReadTransaction trans(dir, __FILE__, __LINE__);
          Entry e(&trans, GET_BY_PATH, PSTR("Jeff"));
          ExpectDataFromExtendedAttributeEquals(&trans, &e, test_bytes,
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

TEST(SyncableDirectoryManager, DirectoryKernelStalenessBug) {
  Step step;

  DirectoryManager dirman(FilePath(FILE_PATH_LITERAL(".")));
  DirectoryKernelStalenessBugDelegate thread_delegate_1(0, &step, &dirman);
  DirectoryKernelStalenessBugDelegate thread_delegate_2(1, &step, &dirman);

  PlatformThreadHandle thread_handle_1;
  PlatformThreadHandle thread_handle_2;

  ASSERT_TRUE(PlatformThread::Create(0, &thread_delegate_1, &thread_handle_1));
  ASSERT_TRUE(PlatformThread::Create(0, &thread_delegate_2, &thread_handle_2));

  PlatformThread::Join(thread_handle_1);
  PlatformThread::Join(thread_handle_2);
}

class StressTransactionsDelegate : public PlatformThread::Delegate {
  public:
   StressTransactionsDelegate(DirectoryManager* dm, PathString dirname,
                                      int thread_number)
       : directory_manager_(dm), dirname_(dirname),
         thread_number_(thread_number) {}

  private:
   DirectoryManager* const directory_manager_;
   PathString dirname_;
   const int thread_number_;

   // PlatformThread::Delegate methods:
   virtual void ThreadMain() {
     ScopedDirLookup dir(directory_manager_, dirname_);
     CHECK(dir.good());
     int entry_count = 0;
     PathString path_name;

     for (int i = 0; i < 20; ++i) {
       const int rand_action = rand() % 10;
       if (rand_action < 4 && !path_name.empty()) {
         ReadTransaction trans(dir, __FILE__, __LINE__);
         Entry e(&trans, GET_BY_PARENTID_AND_NAME, trans.root_id(), path_name);
         PlatformThread::Sleep(rand() % 10);
         CHECK(e.good());
       } else {
         string unique_name = StringPrintf("%d.%d", thread_number_,
           entry_count++);
         path_name.assign(unique_name.begin(), unique_name.end());
         WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
         MutableEntry e(&trans, CREATE, trans.root_id(), path_name);
         CHECK(e.good());
         PlatformThread::Sleep(rand() % 20);
         e.Put(IS_UNSYNCED, true);
         if (e.Put(ID, TestIdFactory::FromNumber(rand())) &&
           e.Get(ID).ServerKnows() && !e.Get(ID).IsRoot())
           e.Put(BASE_VERSION, 1);
       }
    }
   }

  DISALLOW_COPY_AND_ASSIGN(StressTransactionsDelegate);
};

TEST(SyncableDirectory, StressTransactions) {
  DirectoryManager dirman(FilePath(FILE_PATH_LITERAL(".")));
  PathString dirname = PSTR("stress");
  file_util::Delete(dirman.GetSyncDataDatabasePath(), true);
  dirman.Open(dirname);

  const int kThreadCount = 7;
  PlatformThreadHandle threads[kThreadCount];
  scoped_ptr<StressTransactionsDelegate> thread_delegates[kThreadCount];

  for (int i = 0; i < kThreadCount; ++i) {
    thread_delegates[i].reset(
        new StressTransactionsDelegate(&dirman, dirname, i));
    ASSERT_TRUE(
        PlatformThread::Create(0, thread_delegates[i].get(), &threads[i]));
  }

  for (int i = 0; i < kThreadCount; ++i) {
    PlatformThread::Join(threads[i]);
  }

  dirman.Close(dirname);
  file_util::Delete(dirman.GetSyncDataDatabasePath(), true);
}

TEST(Syncable, ComparePathNames) {
  struct {
    char a;
    char b;
    int expected_result;
  } tests[] = {
    { 'A', 'A', 0 },
    { 'A', 'a', 0 },
    { 'a', 'A', 0 },
    { 'a', 'a', 0 },
    { 'A', 'B', -1 },
    { 'A', 'b', -1 },
    { 'a', 'B', -1 },
    { 'a', 'b', -1 },
    { 'B', 'A', 1 },
    { 'B', 'a', 1 },
    { 'b', 'A', 1 },
    { 'b', 'a', 1 } };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    PathString a(1, tests[i].a);
    PathString b(1, tests[i].b);
    const int result = ComparePathNames(a, b);
    if (result != tests[i].expected_result) {
      ADD_FAILURE() << "ComparePathNames(" << tests[i].a << ", " << tests[i].b
       << ") returned " << result << "; expected "
       << tests[i].expected_result;
    }
  }
}

#if defined(OS_WIN)
TEST(Syncable, PathNameMatch) {
  // basic stuff, not too many otherwise we're testing the os.
  EXPECT_TRUE(PathNameMatch(PSTR("bob"), PSTR("bob")));
  EXPECT_FALSE(PathNameMatch(PSTR("bob"), PSTR("fred")));
  // Test our ; extension.
  EXPECT_TRUE(PathNameMatch(PSTR("bo;b"), PSTR("bo;b")));
  EXPECT_TRUE(PathNameMatch(PSTR("bo;b"), PSTR("bo*")));
  EXPECT_FALSE(PathNameMatch(PSTR("bo;b"), PSTR("co;b")));
  EXPECT_FALSE(PathNameMatch(PSTR("bo;b"), PSTR("co*")));
  // Test our fixes for prepended spaces.
  EXPECT_TRUE(PathNameMatch(PSTR("  bob"), PSTR("  bo*")));
  EXPECT_TRUE(PathNameMatch(PSTR("  bob"), PSTR("  bob")));
  EXPECT_FALSE(PathNameMatch(PSTR("bob"), PSTR("  bob")));
  EXPECT_FALSE(PathNameMatch(PSTR("  bob"), PSTR("bob")));
  // Combo test.
  EXPECT_TRUE(PathNameMatch(PSTR("  b;ob"), PSTR("  b;o*")));
  EXPECT_TRUE(PathNameMatch(PSTR("  b;ob"), PSTR("  b;ob")));
  EXPECT_FALSE(PathNameMatch(PSTR("b;ob"), PSTR("  b;ob")));
  EXPECT_FALSE(PathNameMatch(PSTR("  b;ob"), PSTR("b;ob")));
  // other whitespace should give no matches.
  EXPECT_FALSE(PathNameMatch(PSTR("bob"), PSTR("\tbob")));
}
#endif  // defined(OS_WIN)

void FakeSync(MutableEntry* e, const char* fake_id) {
  e->Put(IS_UNSYNCED, false);
  e->Put(BASE_VERSION, 2);
  e->Put(ID, Id::CreateFromServerId(fake_id));
}

TEST_F(SyncableDirectoryTest, Bug1509232) {
  const PathString a = PSTR("alpha");

  CreateEntry(a, dir_.get()->NextId());
  {
    WriteTransaction trans(dir_.get(), UNITTEST, __FILE__, __LINE__);
    MutableEntry e(&trans, GET_BY_PATH, a);
    ASSERT_TRUE(e.good());
    ExtendedAttributeKey key(e.Get(META_HANDLE), PSTR("resourcefork"));
    MutableExtendedAttribute ext(&trans, CREATE, key);
    ASSERT_TRUE(ext.good());
    const char value[] = "stuff";
    Blob value_blob(value, value + arraysize(value));
    ext.mutable_value()->swap(value_blob);
    ext.delete_attribute();
  }
  // This call to SaveChanges used to CHECK fail.
  dir_.get()->SaveChanges();
}

}  // namespace
}  // namespace syncable
