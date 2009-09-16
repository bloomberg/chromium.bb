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
#include <strstream>
#include <ostream>
#include <stdio.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/times.h>
#endif  // !defined(OS_WIN)

#include "base/at_exit.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/sync/syncable/directory_backing_store.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/browser/sync/util/character_set_converters.h"
#include "chrome/browser/sync/util/closure.h"
#include "chrome/browser/sync/util/compat_file.h"
#include "chrome/browser/sync/util/event_sys-inl.h"
#include "chrome/browser/sync/util/path_helpers.h"
#include "chrome/browser/sync/util/pthread_helpers.h"
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
  dir.Open(PSTR("SimpleTest.sqlite3"), PSTR("SimpleTest"));
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
                            const PathString& backing_filepath)
         : DirectoryBackingStore(dir_name, backing_filepath) { }
     virtual bool SaveChanges(const Directory::SaveChangesSnapshot& snapshot) {
       return false;
     }
  };
  virtual DirectoryBackingStore* CreateBackingStore(
      const PathString& dir_name, const PathString& backing_filepath) {
    return new UnsaveableBackingStore(dir_name, backing_filepath);
  }
};

// Test suite for syncable::Directory.
class SyncableDirectoryTest : public testing::Test {
 protected:
  static const PathString kFilePath;
  static const PathString kName;
  static const PathChar* kSqlite3File;
  static const Id kId;

  // SetUp() is called before each test case is run.
  // The sqlite3 DB is deleted before each test is run.
  virtual void SetUp() {
    PathRemove(PathString(kSqlite3File));
    dir_.reset(new Directory());
    ASSERT_TRUE(dir_.get());
    ASSERT_EQ(OPENED, dir_->Open(kFilePath, kName));
    ASSERT_TRUE(dir_->good());
  }

  virtual void TearDown() {
    // This also closes file handles.
    dir_->SaveChanges();
    dir_.reset();
    PathRemove(PathString(kSqlite3File));
  }

  scoped_ptr<Directory> dir_;

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

const PathString SyncableDirectoryTest::kFilePath(PSTR("Test.sqlite3"));
const PathChar* SyncableDirectoryTest::kSqlite3File(PSTR("Test.sqlite3"));
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

TEST_F(SyncableDirectoryTest, TestGetFullPathNeverCrashes) {
  PathString dirname = PSTR("honey"),
      childname = PSTR("jelly");
  WriteTransaction trans(dir_.get(), UNITTEST, __FILE__, __LINE__);
  MutableEntry e1(&trans, CREATE, trans.root_id(), dirname);
  ASSERT_TRUE(e1.good());
  ASSERT_TRUE(e1.Put(IS_DIR, true));
  MutableEntry e2(&trans, CREATE, e1.Get(ID), childname);
  ASSERT_TRUE(e2.good());
  PathString path = GetFullPath(&trans, e2);
  ASSERT_FALSE(path.empty());
  // Give the child a parent that doesn't exist.
  e2.Put(PARENT_ID, TestIdFactory::FromNumber(42));
  path = GetFullPath(&trans, e2);
  ASSERT_TRUE(path.empty());
  // Done testing, make sure CheckTreeInvariants doesn't choke.
  e2.Put(PARENT_ID, e1.Get(ID));
  e2.Put(IS_DEL, true);
  e1.Put(IS_DEL, true);
}

TEST_F(SyncableDirectoryTest, TestGetUnsynced) {
  Directory::UnsyncedMetaHandles handles;
  int64 handle1, handle2;
  {
    WriteTransaction trans(dir_.get(), UNITTEST, __FILE__, __LINE__);

    dir_->GetUnsyncedMetaHandles(&trans, &handles);
    ASSERT_EQ(0, handles.size());

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
    ASSERT_EQ(0, handles.size());

    MutableEntry e3(&trans, GET_BY_HANDLE, handle1);
    ASSERT_TRUE(e3.good());
    e3.Put(IS_UNSYNCED, true);
  }
  dir_->SaveChanges();
  {
    WriteTransaction trans(dir_.get(), UNITTEST, __FILE__, __LINE__);
    dir_->GetUnsyncedMetaHandles(&trans, &handles);
    ASSERT_EQ(1, handles.size());
    ASSERT_TRUE(handle1 == handles[0]);

    MutableEntry e4(&trans, GET_BY_HANDLE, handle2);
    ASSERT_TRUE(e4.good());
    e4.Put(IS_UNSYNCED, true);
  }
  dir_->SaveChanges();
  {
    WriteTransaction trans(dir_.get(), UNITTEST, __FILE__, __LINE__);
    dir_->GetUnsyncedMetaHandles(&trans, &handles);
    ASSERT_EQ(2, handles.size());
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
    ASSERT_EQ(1, handles.size());
    ASSERT_TRUE(handle2 == handles[0]);
  }
}

TEST_F(SyncableDirectoryTest, TestGetUnappliedUpdates) {
  Directory::UnappliedUpdateMetaHandles handles;
  int64 handle1, handle2;
  {
    WriteTransaction trans(dir_.get(), UNITTEST, __FILE__, __LINE__);

    dir_->GetUnappliedUpdateMetaHandles(&trans, &handles);
    ASSERT_EQ(0, handles.size());

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
    ASSERT_EQ(0, handles.size());

    MutableEntry e3(&trans, GET_BY_HANDLE, handle1);
    ASSERT_TRUE(e3.good());
    e3.Put(IS_UNAPPLIED_UPDATE, true);
  }
  dir_->SaveChanges();
  {
    WriteTransaction trans(dir_.get(), UNITTEST, __FILE__, __LINE__);
    dir_->GetUnappliedUpdateMetaHandles(&trans, &handles);
    ASSERT_EQ(1, handles.size());
    ASSERT_TRUE(handle1 == handles[0]);

    MutableEntry e4(&trans, GET_BY_HANDLE, handle2);
    ASSERT_TRUE(e4.good());
    e4.Put(IS_UNAPPLIED_UPDATE, true);
  }
  dir_->SaveChanges();
  {
    WriteTransaction trans(dir_.get(), UNITTEST, __FILE__, __LINE__);
    dir_->GetUnappliedUpdateMetaHandles(&trans, &handles);
    ASSERT_EQ(2, handles.size());
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
    ASSERT_EQ(1, handles.size());
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
  //           /  \
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
    ASSERT_EQ(entry.Get(META_HANDLE), entry_handle);
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
    ASSERT_EQ(id, entry.Get(ID));
  }
  {
    ReadTransaction trans(dir_.get(), __FILE__, __LINE__);
    Entry entry(&trans, GET_BY_PARENTID_AND_NAME, trans.root_id(), name);
    ASSERT_TRUE(entry.good());
    ASSERT_EQ(id, entry.Get(ID));
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
  ASSERT_EQ(OPENED, dir_->Open(kFilePath, kName));
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
    ASSERT_EQ(name, e.Get(NAME));
  ASSERT_EQ(base_version, e.Get(BASE_VERSION));
  ASSERT_EQ(server_version, e.Get(SERVER_VERSION));
  ASSERT_EQ(is_del, e.Get(IS_DEL));
}

TEST(SyncableDirectoryManager, TestFileRelease) {
  DirectoryManager dm(PSTR("."));
  ASSERT_TRUE(dm.Open(PSTR("ScopeTest")));
  {
    ScopedDirLookup(&dm, PSTR("ScopeTest"));
  }
  dm.Close(PSTR("ScopeTest"));
  ASSERT_EQ(0, PathRemove(dm.GetSyncDataDatabasePath()));
}

static void* OpenTestThreadMain(void* arg) {
  DirectoryManager* const dm = reinterpret_cast<DirectoryManager*>(arg);
  CHECK(dm->Open(PSTR("Open")));
  return 0;
}

TEST(SyncableDirectoryManager, ThreadOpenTest) {
  DirectoryManager dm(PSTR("."));
  pthread_t thread;
  ASSERT_EQ(0, pthread_create(&thread, 0, OpenTestThreadMain, &dm));
  void* result;
  ASSERT_EQ(0, pthread_join(thread, &result));
  {
    ScopedDirLookup dir(&dm, PSTR("Open"));
    ASSERT_TRUE(dir.good());
  }
  dm.Close(PSTR("Open"));
  ScopedDirLookup dir(&dm, PSTR("Open"));
  ASSERT_FALSE(dir.good());
}

namespace ThreadBug1 {
  struct Step {
    PThreadMutex mutex;
    PThreadCondVar condvar;
    int number;
    int64 metahandle;
  };
  struct ThreadArg {
    int role;  // 0 or 1, meaning this thread does the odd or event steps.
    Step* step;
    DirectoryManager* dirman;
  };

  void* ThreadMain(void* arg) {
    ThreadArg* const args = reinterpret_cast<ThreadArg*>(arg);
    const int role = args->role;
    Step* const step = args->step;
    DirectoryManager* const dirman = args->dirman;
    const PathString dirname = PSTR("ThreadBug1");
    PThreadScopedLock<PThreadMutex> lock(&step->mutex);
    while (step->number < 3) {
      while (step->number % 2 != role)
        pthread_cond_wait(&step->condvar.condvar_, &step->mutex.mutex_);
      switch (step->number) {
      case 0:
        dirman->Open(dirname);
        break;
      case 1:
        {
          dirman->Close(dirname);
          dirman->Open(dirname);
          ScopedDirLookup dir(dirman, dirname);
          CHECK(dir.good());
          WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
          MutableEntry me(&trans, CREATE, trans.root_id(), PSTR("Jeff"));
          step->metahandle = me.Get(META_HANDLE);
          me.Put(IS_UNSYNCED, true);
        }
        break;
      case 2:
        {
          ScopedDirLookup dir(dirman, dirname);
          CHECK(dir.good());
          ReadTransaction trans(dir, __FILE__, __LINE__);
          Entry e(&trans, GET_BY_HANDLE, step->metahandle);
          CHECK(e.good());  // Failed due to ThreadBug1
        }
        dirman->Close(dirname);
        break;
      }
      step->number += 1;
      pthread_cond_signal(&step->condvar.condvar_);
    }
    return 0;
  }
}

TEST(SyncableDirectoryManager, ThreadBug1) {
  using ThreadBug1::Step;
  using ThreadBug1::ThreadArg;
  using ThreadBug1::ThreadMain;

  Step step;
  step.number = 0;
  DirectoryManager dirman(PSTR("."));
  ThreadArg arg1 = { 0, &step, &dirman };
  ThreadArg arg2 = { 1, &step, &dirman };
  pthread_t thread1, thread2;
  ASSERT_EQ(0, pthread_create(&thread1, NULL, &ThreadMain, &arg1));
  ASSERT_EQ(0, pthread_create(&thread2, NULL, &ThreadMain, &arg2));
  void* retval;
  ASSERT_EQ(0, pthread_join(thread1, &retval));
  ASSERT_EQ(0, pthread_join(thread2, &retval));
}

namespace DirectoryKernelStalenessBug {
  // The in-memory information would get out of sync because a
  // directory would be closed and re-opened, and then an old
  // Directory::Kernel with stale information would get saved to the db.
  typedef ThreadBug1::Step Step;
  typedef ThreadBug1::ThreadArg ThreadArg;

  void* ThreadMain(void* arg) {
    const char test_bytes[] = "test data";
    ThreadArg* const args = reinterpret_cast<ThreadArg*>(arg);
    const int role = args->role;
    Step* const step = args->step;
    DirectoryManager* const dirman = args->dirman;
    const PathString dirname = PSTR("DirectoryKernelStalenessBug");
    PThreadScopedLock<PThreadMutex> lock(&step->mutex);
    while (step->number < 4) {
      while (step->number % 2 != role)
        pthread_cond_wait(&step->condvar.condvar_, &step->mutex.mutex_);
      switch (step->number) {
      case 0:
        {
          // Clean up remnants of earlier test runs.
          PathRemove(dirman->GetSyncDataDatabasePath());
          // Test.
          dirman->Open(dirname);
          ScopedDirLookup dir(dirman, dirname);
          CHECK(dir.good());
          WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
          MutableEntry me(&trans, CREATE, trans.root_id(), PSTR("Jeff"));
          me.Put(BASE_VERSION, 1);
          me.Put(ID, TestIdFactory::FromNumber(100));
          PutDataAsExtendedAttribute(&trans, &me, test_bytes,
                                     sizeof(test_bytes));
        }
        {
          ScopedDirLookup dir(dirman, dirname);
          CHECK(dir.good());
          dir->SaveChanges();
        }
        dirman->CloseAllDirectories();
        break;
      case 1:
        {
          dirman->Open(dirname);
          ScopedDirLookup dir(dirman, dirname);
          CHECK(dir.good());
        }
        break;
      case 2:
        {
          ScopedDirLookup dir(dirman, dirname);
          CHECK(dir.good());
        }
        break;
      case 3:
        {
          ScopedDirLookup dir(dirman, dirname);
          CHECK(dir.good());
          ReadTransaction trans(dir, __FILE__, __LINE__);
          Entry e(&trans, GET_BY_PATH, PSTR("Jeff"));
          ExpectDataFromExtendedAttributeEquals(&trans, &e, test_bytes,
                                                sizeof(test_bytes));
        }
        // Same result as CloseAllDirectories, but more code coverage.
        dirman->Close(dirname);
        break;
      }
      step->number += 1;
      pthread_cond_signal(&step->condvar.condvar_);
    }
    return 0;
  }
}

TEST(SyncableDirectoryManager, DirectoryKernelStalenessBug) {
  using DirectoryKernelStalenessBug::Step;
  using DirectoryKernelStalenessBug::ThreadArg;
  using DirectoryKernelStalenessBug::ThreadMain;

  Step step;
  step.number = 0;
  DirectoryManager dirman(PSTR("."));
  ThreadArg arg1 = { 0, &step, &dirman };
  ThreadArg arg2 = { 1, &step, &dirman };
  pthread_t thread1, thread2;
  ASSERT_EQ(0, pthread_create(&thread1, NULL, &ThreadMain, &arg1));
  ASSERT_EQ(0, pthread_create(&thread2, NULL, &ThreadMain, &arg2));
  void* retval;
  ASSERT_EQ(0, pthread_join(thread1, &retval));
  ASSERT_EQ(0, pthread_join(thread2, &retval));
}

timespec operator + (const timespec& a, const timespec& b) {
  const long nanos = a.tv_nsec + b.tv_nsec;
  static const long nanos_per_second = 1000000000;
  timespec r = { a.tv_sec + b.tv_sec + (nanos / nanos_per_second),
                 nanos % nanos_per_second };
  return r;
}

void SleepMs(int milliseconds) {
#ifdef OS_WIN
  Sleep(milliseconds);
#else
  usleep(milliseconds * 1000);
#endif
}

namespace StressTransaction {
  struct Globals {
    DirectoryManager* dirman;
    PathString dirname;
  };

  struct ThreadArg {
    Globals* globals;
    int thread_number;
  };

  void* ThreadMain(void* arg) {
    ThreadArg* const args = reinterpret_cast<ThreadArg*>(arg);
    Globals* const globals = args->globals;
    ScopedDirLookup dir(globals->dirman, globals->dirname);
    CHECK(dir.good());
    int entry_count = 0;
    PathString path_name;
    for (int i = 0; i < 20; ++i) {
      const int rand_action = rand() % 10;
      if (rand_action < 4 && !path_name.empty()) {
        ReadTransaction trans(dir, __FILE__, __LINE__);
        Entry e(&trans, GET_BY_PARENTID_AND_NAME, trans.root_id(), path_name);
        SleepMs(rand() % 10);
        CHECK(e.good());
      } else {
        string unique_name = StringPrintf("%d.%d", args->thread_number,
                                          entry_count++);
        path_name.assign(unique_name.begin(), unique_name.end());
        WriteTransaction trans(dir, UNITTEST, __FILE__, __LINE__);
        MutableEntry e(&trans, CREATE, trans.root_id(), path_name);
        CHECK(e.good());
        SleepMs(rand() % 20);
        e.Put(IS_UNSYNCED, true);
        if (e.Put(ID, TestIdFactory::FromNumber(rand())) &&
            e.Get(ID).ServerKnows() && !e.Get(ID).IsRoot())
          e.Put(BASE_VERSION, 1);
      }
    }
    return 0;
  }
}

TEST(SyncableDirectory, StressTransactions) {
  using StressTransaction::Globals;
  using StressTransaction::ThreadArg;
  using StressTransaction::ThreadMain;

  DirectoryManager dirman(PSTR("."));
  Globals globals;
  globals.dirname = PSTR("stress");
  globals.dirman = &dirman;
  PathRemove(dirman.GetSyncDataDatabasePath());
  dirman.Open(globals.dirname);
  const int kThreadCount = 7;
  pthread_t threads[kThreadCount];
  ThreadArg thread_args[kThreadCount];
  for (int i = 0; i < kThreadCount; ++i) {
    thread_args[i].thread_number = i;
    thread_args[i].globals = &globals;
    ASSERT_EQ(0, pthread_create(threads + i, NULL, &ThreadMain,
                                thread_args + i));
  }
  void* retval;
  for (pthread_t* i = threads; i < threads + kThreadCount; ++i)
    ASSERT_EQ(0, pthread_join(*i, &retval));
  dirman.Close(globals.dirname);
  PathRemove(dirman.GetSyncDataDatabasePath());
}

static PathString UTF8ToPathStringQuick(const char* str) {
  PathString ret;
  CHECK(browser_sync::UTF8ToPathString(str, strlen(str), &ret));
  return ret;
}

// Returns number of chars used. Max possible is 4.
// This algorithm was coded from the table at
// http://en.wikipedia.org/w/index.php?title=UTF-8&oldid=153391259
// There are no endian issues.
static int UTF32ToUTF8(uint32 incode, unsigned char* out) {
  if (incode <= 0x7f) {
    out[0] = incode;
    return 1;
  }
  if (incode <= 0x7ff) {
    out[0] = 0xC0;
    out[0] |= (incode >> 6);
    out[1] = 0x80;
    out[1] |= (incode & 0x3F);
    return 2;
  }
  if (incode <= 0xFFFF) {
    if ((incode > 0xD7FF) && (incode < 0xE000))
      return 0;
    out[0] = 0xE0;
    out[0] |= (incode >> 12);
    out[1] = 0x80;
    out[1] |= (incode >> 6) & 0x3F;
    out[2] = 0x80;
    out[2] |= incode & 0x3F;
    return 3;
  }
  if (incode <= 0x10FFFF) {
    out[0] = 0xF0;
    out[0] |= incode >> 18;
    out[1] = 0x80;
    out[1] |= (incode >> 12) & 0x3F;
    out[2] = 0x80;
    out[2] |= (incode >> 6) & 0x3F;
    out[3] = 0x80;
    out[3] |= incode & 0x3F;
    return 4;
  }
  return 0;
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
  for (int i = 0; i < ARRAYSIZE(tests); ++i) {
    PathString a(1, tests[i].a);
    PathString b(1, tests[i].b);
    const int result = ComparePathNames(a, b);
    if (result != tests[i].expected_result) {
      ADD_FAILURE() << "ComparePathNames(" << tests[i].a << ", " << tests[i].b
       << ") returned " << result << "; expected "
       << tests[i].expected_result;
    }
  }

#ifndef OS_WIN
  // This table lists (to the best of my knowledge) every pair of characters
  // in unicode such that:
  // for all i: tolower(kUpperToLowerMap[i].upper) = kUpperToLowerMap[i].lower
  // This is then used to test that case-insensitive comparison of each pair
  // returns 0 (that, that they are equal). After running the test on Mac OS X
  // with the CFString API for comparision, the failing cases were commented
  // out.
  //
  // Map of upper to lower case characters taken from
  // ftp://ftp.unicode.org/Public/UNIDATA/UnicodeData.txt
  typedef struct {
    uint32 upper;  // the upper case character
    uint32 lower;  // the lower case character that upper maps to
  } UpperToLowerMapEntry;
  static const UpperToLowerMapEntry kUpperToLowerMap[] = {
    // { UPPER, lower }, { UPPER, lower }, etc...
    // some of these are commented out because they fail on some OS.
    { 0x00041, 0x00061 }, { 0x00042, 0x00062 }, { 0x00043, 0x00063 },
    { 0x00044, 0x00064 }, { 0x00045, 0x00065 }, { 0x00046, 0x00066 },
    { 0x00047, 0x00067 }, { 0x00048, 0x00068 }, { 0x00049, 0x00069 },
    { 0x0004A, 0x0006A }, { 0x0004B, 0x0006B }, { 0x0004C, 0x0006C },
    { 0x0004D, 0x0006D }, { 0x0004E, 0x0006E }, { 0x0004F, 0x0006F },
    { 0x00050, 0x00070 }, { 0x00051, 0x00071 }, { 0x00052, 0x00072 },
    { 0x00053, 0x00073 }, { 0x00054, 0x00074 }, { 0x00055, 0x00075 },
    { 0x00056, 0x00076 }, { 0x00057, 0x00077 }, { 0x00058, 0x00078 },
    { 0x00059, 0x00079 }, { 0x0005A, 0x0007A }, { 0x000C0, 0x000E0 },
    { 0x000C1, 0x000E1 }, { 0x000C2, 0x000E2 }, { 0x000C3, 0x000E3 },
    { 0x000C4, 0x000E4 }, { 0x000C5, 0x000E5 }, { 0x000C6, 0x000E6 },
    { 0x000C7, 0x000E7 }, { 0x000C8, 0x000E8 }, { 0x000C9, 0x000E9 },
    { 0x000CA, 0x000EA }, { 0x000CB, 0x000EB }, { 0x000CC, 0x000EC },
    { 0x000CD, 0x000ED }, { 0x000CE, 0x000EE }, { 0x000CF, 0x000EF },
    { 0x000D0, 0x000F0 }, { 0x000D1, 0x000F1 }, { 0x000D2, 0x000F2 },
    { 0x000D3, 0x000F3 }, { 0x000D4, 0x000F4 }, { 0x000D5, 0x000F5 },
    { 0x000D6, 0x000F6 }, { 0x000D8, 0x000F8 }, { 0x000D9, 0x000F9 },
    { 0x000DA, 0x000FA }, { 0x000DB, 0x000FB }, { 0x000DC, 0x000FC },
    { 0x000DD, 0x000FD }, { 0x000DE, 0x000FE },
    { 0x00100, 0x00101 }, { 0x00102, 0x00103 }, { 0x00104, 0x00105 },
    { 0x00106, 0x00107 }, { 0x00108, 0x00109 }, { 0x0010A, 0x0010B },
    { 0x0010C, 0x0010D }, { 0x0010E, 0x0010F }, { 0x00110, 0x00111 },
    { 0x00112, 0x00113 }, { 0x00114, 0x00115 }, { 0x00116, 0x00117 },
    { 0x00118, 0x00119 }, { 0x0011A, 0x0011B }, { 0x0011C, 0x0011D },
    { 0x0011E, 0x0011F }, { 0x00120, 0x00121 }, { 0x00122, 0x00123 },
    { 0x00124, 0x00125 }, { 0x00126, 0x00127 }, { 0x00128, 0x00129 },
    { 0x0012A, 0x0012B }, { 0x0012C, 0x0012D }, { 0x0012E, 0x0012F },
    /*{ 0x00130, 0x00069 },*/ { 0x00132, 0x00133 }, { 0x00134, 0x00135 },
    { 0x00136, 0x00137 }, { 0x00139, 0x0013A }, { 0x0013B, 0x0013C },
    { 0x0013D, 0x0013E }, { 0x0013F, 0x00140 }, { 0x00141, 0x00142 },
    { 0x00143, 0x00144 }, { 0x00145, 0x00146 }, { 0x00147, 0x00148 },
    { 0x0014A, 0x0014B }, { 0x0014C, 0x0014D }, { 0x0014E, 0x0014F },
    { 0x00150, 0x00151 }, { 0x00152, 0x00153 }, { 0x00154, 0x00155 },
    { 0x00156, 0x00157 }, { 0x00158, 0x00159 }, { 0x0015A, 0x0015B },
    { 0x0015C, 0x0015D }, { 0x0015E, 0x0015F }, { 0x00160, 0x00161 },
    { 0x00162, 0x00163 }, { 0x00164, 0x00165 }, { 0x00166, 0x00167 },
    { 0x00168, 0x00169 }, { 0x0016A, 0x0016B }, { 0x0016C, 0x0016D },
    { 0x0016E, 0x0016F }, { 0x00170, 0x00171 }, { 0x00172, 0x00173 },
    { 0x00174, 0x00175 }, { 0x00176, 0x00177 }, { 0x00178, 0x000FF },
    { 0x00179, 0x0017A }, { 0x0017B, 0x0017C }, { 0x0017D, 0x0017E },
    { 0x00181, 0x00253 }, { 0x00182, 0x00183 }, { 0x00184, 0x00185 },
    { 0x00186, 0x00254 }, { 0x00187, 0x00188 }, { 0x00189, 0x00256 },
    { 0x0018A, 0x00257 }, { 0x0018B, 0x0018C }, { 0x0018E, 0x001DD },
    { 0x0018F, 0x00259 }, { 0x00190, 0x0025B }, { 0x00191, 0x00192 },
    { 0x00193, 0x00260 }, { 0x00194, 0x00263 }, { 0x00196, 0x00269 },
    { 0x00197, 0x00268 }, { 0x00198, 0x00199 }, { 0x0019C, 0x0026F },
    { 0x0019D, 0x00272 }, { 0x0019F, 0x00275 }, { 0x001A0, 0x001A1 },
    { 0x001A2, 0x001A3 }, { 0x001A4, 0x001A5 }, { 0x001A6, 0x00280 },
    { 0x001A7, 0x001A8 }, { 0x001A9, 0x00283 }, { 0x001AC, 0x001AD },
    { 0x001AE, 0x00288 }, { 0x001AF, 0x001B0 }, { 0x001B1, 0x0028A },
    { 0x001B2, 0x0028B }, { 0x001B3, 0x001B4 }, { 0x001B5, 0x001B6 },
    { 0x001B7, 0x00292 }, { 0x001B8, 0x001B9 }, { 0x001BC, 0x001BD },
    { 0x001C4, 0x001C6 }, { 0x001C7, 0x001C9 }, { 0x001CA, 0x001CC },
    { 0x001CD, 0x001CE }, { 0x001CF, 0x001D0 }, { 0x001D1, 0x001D2 },
    { 0x001D3, 0x001D4 }, { 0x001D5, 0x001D6 }, { 0x001D7, 0x001D8 },
    { 0x001D9, 0x001DA }, { 0x001DB, 0x001DC }, { 0x001DE, 0x001DF },
    { 0x001E0, 0x001E1 }, { 0x001E2, 0x001E3 }, { 0x001E4, 0x001E5 },
    { 0x001E6, 0x001E7 }, { 0x001E8, 0x001E9 }, { 0x001EA, 0x001EB },
    { 0x001EC, 0x001ED }, { 0x001EE, 0x001EF }, { 0x001F1, 0x001F3 },
    { 0x001F4, 0x001F5 }, { 0x001F6, 0x00195 }, { 0x001F7, 0x001BF },
    { 0x001F8, 0x001F9 }, { 0x001FA, 0x001FB }, { 0x001FC, 0x001FD },
    { 0x001FE, 0x001FF }, { 0x00200, 0x00201 }, { 0x00202, 0x00203 },
    { 0x00204, 0x00205 }, { 0x00206, 0x00207 }, { 0x00208, 0x00209 },
    { 0x0020A, 0x0020B }, { 0x0020C, 0x0020D }, { 0x0020E, 0x0020F },
    { 0x00210, 0x00211 }, { 0x00212, 0x00213 }, { 0x00214, 0x00215 },
    { 0x00216, 0x00217 }, { 0x00218, 0x00219 }, { 0x0021A, 0x0021B },
    { 0x0021C, 0x0021D }, { 0x0021E, 0x0021F }, { 0x00220, 0x0019E },
    { 0x00222, 0x00223 }, { 0x00224, 0x00225 }, { 0x00226, 0x00227 },
    { 0x00228, 0x00229 }, { 0x0022A, 0x0022B }, { 0x0022C, 0x0022D },
    { 0x0022E, 0x0022F }, { 0x00230, 0x00231 }, { 0x00232, 0x00233 },
    /*{ 0x0023B, 0x0023C }, { 0x0023D, 0x0019A }, { 0x00241, 0x00294 }, */
    { 0x00386, 0x003AC }, { 0x00388, 0x003AD }, { 0x00389, 0x003AE },
    { 0x0038A, 0x003AF }, { 0x0038C, 0x003CC }, { 0x0038E, 0x003CD },
    { 0x0038F, 0x003CE }, { 0x00391, 0x003B1 }, { 0x00392, 0x003B2 },
    { 0x00393, 0x003B3 }, { 0x00394, 0x003B4 }, { 0x00395, 0x003B5 },
    { 0x00396, 0x003B6 }, { 0x00397, 0x003B7 }, { 0x00398, 0x003B8 },
    { 0x00399, 0x003B9 }, { 0x0039A, 0x003BA }, { 0x0039B, 0x003BB },
    { 0x0039C, 0x003BC }, { 0x0039D, 0x003BD }, { 0x0039E, 0x003BE },
    { 0x0039F, 0x003BF }, { 0x003A0, 0x003C0 }, { 0x003A1, 0x003C1 },
    { 0x003A3, 0x003C3 }, { 0x003A4, 0x003C4 }, { 0x003A5, 0x003C5 },
    { 0x003A6, 0x003C6 }, { 0x003A7, 0x003C7 }, { 0x003A8, 0x003C8 },
    { 0x003A9, 0x003C9 }, { 0x003AA, 0x003CA }, { 0x003AB, 0x003CB },
    { 0x003D8, 0x003D9 }, { 0x003DA, 0x003DB }, { 0x003DC, 0x003DD },
    { 0x003DE, 0x003DF }, { 0x003E0, 0x003E1 }, { 0x003E2, 0x003E3 },
    { 0x003E4, 0x003E5 }, { 0x003E6, 0x003E7 }, { 0x003E8, 0x003E9 },
    { 0x003EA, 0x003EB }, { 0x003EC, 0x003ED }, { 0x003EE, 0x003EF },
    { 0x003F4, 0x003B8 }, { 0x003F7, 0x003F8 }, { 0x003F9, 0x003F2 },
    { 0x003FA, 0x003FB }, { 0x00400, 0x00450 }, { 0x00401, 0x00451 },
    { 0x00402, 0x00452 }, { 0x00403, 0x00453 }, { 0x00404, 0x00454 },
    { 0x00405, 0x00455 }, { 0x00406, 0x00456 }, { 0x00407, 0x00457 },
    { 0x00408, 0x00458 }, { 0x00409, 0x00459 }, { 0x0040A, 0x0045A },
    { 0x0040B, 0x0045B }, { 0x0040C, 0x0045C }, { 0x0040D, 0x0045D },
    { 0x0040E, 0x0045E }, { 0x0040F, 0x0045F }, { 0x00410, 0x00430 },
    { 0x00411, 0x00431 }, { 0x00412, 0x00432 }, { 0x00413, 0x00433 },
    { 0x00414, 0x00434 }, { 0x00415, 0x00435 }, { 0x00416, 0x00436 },
    { 0x00417, 0x00437 }, { 0x00418, 0x00438 }, { 0x00419, 0x00439 },
    { 0x0041A, 0x0043A }, { 0x0041B, 0x0043B }, { 0x0041C, 0x0043C },
    { 0x0041D, 0x0043D }, { 0x0041E, 0x0043E }, { 0x0041F, 0x0043F },
    { 0x00420, 0x00440 }, { 0x00421, 0x00441 }, { 0x00422, 0x00442 },
    { 0x00423, 0x00443 }, { 0x00424, 0x00444 }, { 0x00425, 0x00445 },
    { 0x00426, 0x00446 }, { 0x00427, 0x00447 }, { 0x00428, 0x00448 },
    { 0x00429, 0x00449 }, { 0x0042A, 0x0044A }, { 0x0042B, 0x0044B },
    { 0x0042C, 0x0044C }, { 0x0042D, 0x0044D }, { 0x0042E, 0x0044E },
    { 0x0042F, 0x0044F }, { 0x00460, 0x00461 }, { 0x00462, 0x00463 },
    { 0x00464, 0x00465 }, { 0x00466, 0x00467 }, { 0x00468, 0x00469 },
    { 0x0046A, 0x0046B }, { 0x0046C, 0x0046D }, { 0x0046E, 0x0046F },
    { 0x00470, 0x00471 }, { 0x00472, 0x00473 }, { 0x00474, 0x00475 },
    { 0x00476, 0x00477 }, { 0x00478, 0x00479 }, { 0x0047A, 0x0047B },
    { 0x0047C, 0x0047D }, { 0x0047E, 0x0047F }, { 0x00480, 0x00481 },
    { 0x0048A, 0x0048B }, { 0x0048C, 0x0048D }, { 0x0048E, 0x0048F },
    { 0x00490, 0x00491 }, { 0x00492, 0x00493 }, { 0x00494, 0x00495 },
    { 0x00496, 0x00497 }, { 0x00498, 0x00499 }, { 0x0049A, 0x0049B },
    { 0x0049C, 0x0049D }, { 0x0049E, 0x0049F }, { 0x004A0, 0x004A1 },
    { 0x004A2, 0x004A3 }, { 0x004A4, 0x004A5 }, { 0x004A6, 0x004A7 },
    { 0x004A8, 0x004A9 }, { 0x004AA, 0x004AB }, { 0x004AC, 0x004AD },
    { 0x004AE, 0x004AF }, { 0x004B0, 0x004B1 }, { 0x004B2, 0x004B3 },
    { 0x004B4, 0x004B5 }, { 0x004B6, 0x004B7 }, { 0x004B8, 0x004B9 },
    { 0x004BA, 0x004BB }, { 0x004BC, 0x004BD }, { 0x004BE, 0x004BF },
    { 0x004C1, 0x004C2 }, { 0x004C3, 0x004C4 }, { 0x004C5, 0x004C6 },
    { 0x004C7, 0x004C8 }, { 0x004C9, 0x004CA }, { 0x004CB, 0x004CC },
    { 0x004CD, 0x004CE }, { 0x004D0, 0x004D1 }, { 0x004D2, 0x004D3 },
    { 0x004D4, 0x004D5 }, { 0x004D6, 0x004D7 }, { 0x004D8, 0x004D9 },
    { 0x004DA, 0x004DB }, { 0x004DC, 0x004DD }, { 0x004DE, 0x004DF },
    { 0x004E0, 0x004E1 }, { 0x004E2, 0x004E3 }, { 0x004E4, 0x004E5 },
    { 0x004E6, 0x004E7 }, { 0x004E8, 0x004E9 }, { 0x004EA, 0x004EB },
    { 0x004EC, 0x004ED }, { 0x004EE, 0x004EF }, { 0x004F0, 0x004F1 },
    { 0x004F2, 0x004F3 }, { 0x004F4, 0x004F5 }, /*{ 0x004F6, 0x004F7 }, */
    { 0x004F8, 0x004F9 }, { 0x00500, 0x00501 }, { 0x00502, 0x00503 },
    { 0x00504, 0x00505 }, { 0x00506, 0x00507 }, { 0x00508, 0x00509 },
    { 0x0050A, 0x0050B }, { 0x0050C, 0x0050D }, { 0x0050E, 0x0050F },
    { 0x00531, 0x00561 }, { 0x00532, 0x00562 }, { 0x00533, 0x00563 },
    { 0x00534, 0x00564 }, { 0x00535, 0x00565 }, { 0x00536, 0x00566 },
    { 0x00537, 0x00567 }, { 0x00538, 0x00568 }, { 0x00539, 0x00569 },
    { 0x0053A, 0x0056A }, { 0x0053B, 0x0056B }, { 0x0053C, 0x0056C },
    { 0x0053D, 0x0056D }, { 0x0053E, 0x0056E }, { 0x0053F, 0x0056F },
    { 0x00540, 0x00570 }, { 0x00541, 0x00571 }, { 0x00542, 0x00572 },
    { 0x00543, 0x00573 }, { 0x00544, 0x00574 }, { 0x00545, 0x00575 },
    { 0x00546, 0x00576 }, { 0x00547, 0x00577 }, { 0x00548, 0x00578 },
    { 0x00549, 0x00579 }, { 0x0054A, 0x0057A }, { 0x0054B, 0x0057B },
    { 0x0054C, 0x0057C }, { 0x0054D, 0x0057D }, { 0x0054E, 0x0057E },
    { 0x0054F, 0x0057F }, { 0x00550, 0x00580 }, { 0x00551, 0x00581 },
    { 0x00552, 0x00582 }, { 0x00553, 0x00583 }, { 0x00554, 0x00584 },
    { 0x00555, 0x00585 }, { 0x00556, 0x00586 }, /*{ 0x010A0, 0x02D00 },
    { 0x010A1, 0x02D01 }, { 0x010A2, 0x02D02 }, { 0x010A3, 0x02D03 },
    { 0x010A4, 0x02D04 }, { 0x010A5, 0x02D05 }, { 0x010A6, 0x02D06 },
    { 0x010A7, 0x02D07 }, { 0x010A8, 0x02D08 }, { 0x010A9, 0x02D09 },
    { 0x010AA, 0x02D0A }, { 0x010AB, 0x02D0B }, { 0x010AC, 0x02D0C },
    { 0x010AD, 0x02D0D }, { 0x010AE, 0x02D0E }, { 0x010AF, 0x02D0F },
    { 0x010B0, 0x02D10 }, { 0x010B1, 0x02D11 }, { 0x010B2, 0x02D12 },
    { 0x010B3, 0x02D13 }, { 0x010B4, 0x02D14 }, { 0x010B5, 0x02D15 },
    { 0x010B6, 0x02D16 }, { 0x010B7, 0x02D17 }, { 0x010B8, 0x02D18 },
    { 0x010B9, 0x02D19 }, { 0x010BA, 0x02D1A }, { 0x010BB, 0x02D1B },
    { 0x010BC, 0x02D1C }, { 0x010BD, 0x02D1D }, { 0x010BE, 0x02D1E },
    { 0x010BF, 0x02D1F }, { 0x010C0, 0x02D20 }, { 0x010C1, 0x02D21 },
    { 0x010C2, 0x02D22 }, { 0x010C3, 0x02D23 }, { 0x010C4, 0x02D24 },
    { 0x010C5, 0x02D25 },*/ { 0x01E00, 0x01E01 }, { 0x01E02, 0x01E03 },
    { 0x01E04, 0x01E05 }, { 0x01E06, 0x01E07 }, { 0x01E08, 0x01E09 },
    { 0x01E0A, 0x01E0B }, { 0x01E0C, 0x01E0D }, { 0x01E0E, 0x01E0F },
    { 0x01E10, 0x01E11 }, { 0x01E12, 0x01E13 }, { 0x01E14, 0x01E15 },
    { 0x01E16, 0x01E17 }, { 0x01E18, 0x01E19 }, { 0x01E1A, 0x01E1B },
    { 0x01E1C, 0x01E1D }, { 0x01E1E, 0x01E1F }, { 0x01E20, 0x01E21 },
    { 0x01E22, 0x01E23 }, { 0x01E24, 0x01E25 }, { 0x01E26, 0x01E27 },
    { 0x01E28, 0x01E29 }, { 0x01E2A, 0x01E2B }, { 0x01E2C, 0x01E2D },
    { 0x01E2E, 0x01E2F }, { 0x01E30, 0x01E31 }, { 0x01E32, 0x01E33 },
    { 0x01E34, 0x01E35 }, { 0x01E36, 0x01E37 }, { 0x01E38, 0x01E39 },
    { 0x01E3A, 0x01E3B }, { 0x01E3C, 0x01E3D }, { 0x01E3E, 0x01E3F },
    { 0x01E40, 0x01E41 }, { 0x01E42, 0x01E43 }, { 0x01E44, 0x01E45 },
    { 0x01E46, 0x01E47 }, { 0x01E48, 0x01E49 }, { 0x01E4A, 0x01E4B },
    { 0x01E4C, 0x01E4D }, { 0x01E4E, 0x01E4F }, { 0x01E50, 0x01E51 },
    { 0x01E52, 0x01E53 }, { 0x01E54, 0x01E55 }, { 0x01E56, 0x01E57 },
    { 0x01E58, 0x01E59 }, { 0x01E5A, 0x01E5B }, { 0x01E5C, 0x01E5D },
    { 0x01E5E, 0x01E5F }, { 0x01E60, 0x01E61 }, { 0x01E62, 0x01E63 },
    { 0x01E64, 0x01E65 }, { 0x01E66, 0x01E67 }, { 0x01E68, 0x01E69 },
    { 0x01E6A, 0x01E6B }, { 0x01E6C, 0x01E6D }, { 0x01E6E, 0x01E6F },
    { 0x01E70, 0x01E71 }, { 0x01E72, 0x01E73 }, { 0x01E74, 0x01E75 },
    { 0x01E76, 0x01E77 }, { 0x01E78, 0x01E79 }, { 0x01E7A, 0x01E7B },
    { 0x01E7C, 0x01E7D }, { 0x01E7E, 0x01E7F }, { 0x01E80, 0x01E81 },
    { 0x01E82, 0x01E83 }, { 0x01E84, 0x01E85 }, { 0x01E86, 0x01E87 },
    { 0x01E88, 0x01E89 }, { 0x01E8A, 0x01E8B }, { 0x01E8C, 0x01E8D },
    { 0x01E8E, 0x01E8F }, { 0x01E90, 0x01E91 }, { 0x01E92, 0x01E93 },
    { 0x01E94, 0x01E95 }, { 0x01EA0, 0x01EA1 }, { 0x01EA2, 0x01EA3 },
    { 0x01EA4, 0x01EA5 }, { 0x01EA6, 0x01EA7 }, { 0x01EA8, 0x01EA9 },
    { 0x01EAA, 0x01EAB }, { 0x01EAC, 0x01EAD }, { 0x01EAE, 0x01EAF },
    { 0x01EB0, 0x01EB1 }, { 0x01EB2, 0x01EB3 }, { 0x01EB4, 0x01EB5 },
    { 0x01EB6, 0x01EB7 }, { 0x01EB8, 0x01EB9 }, { 0x01EBA, 0x01EBB },
    { 0x01EBC, 0x01EBD }, { 0x01EBE, 0x01EBF }, { 0x01EC0, 0x01EC1 },
    { 0x01EC2, 0x01EC3 }, { 0x01EC4, 0x01EC5 }, { 0x01EC6, 0x01EC7 },
    { 0x01EC8, 0x01EC9 }, { 0x01ECA, 0x01ECB }, { 0x01ECC, 0x01ECD },
    { 0x01ECE, 0x01ECF }, { 0x01ED0, 0x01ED1 }, { 0x01ED2, 0x01ED3 },
    { 0x01ED4, 0x01ED5 }, { 0x01ED6, 0x01ED7 }, { 0x01ED8, 0x01ED9 },
    { 0x01EDA, 0x01EDB }, { 0x01EDC, 0x01EDD }, { 0x01EDE, 0x01EDF },
    { 0x01EE0, 0x01EE1 }, { 0x01EE2, 0x01EE3 }, { 0x01EE4, 0x01EE5 },
    { 0x01EE6, 0x01EE7 }, { 0x01EE8, 0x01EE9 }, { 0x01EEA, 0x01EEB },
    { 0x01EEC, 0x01EED }, { 0x01EEE, 0x01EEF }, { 0x01EF0, 0x01EF1 },
    { 0x01EF2, 0x01EF3 }, { 0x01EF4, 0x01EF5 }, { 0x01EF6, 0x01EF7 },
    { 0x01EF8, 0x01EF9 }, { 0x01F08, 0x01F00 }, { 0x01F09, 0x01F01 },
    { 0x01F0A, 0x01F02 }, { 0x01F0B, 0x01F03 }, { 0x01F0C, 0x01F04 },
    { 0x01F0D, 0x01F05 }, { 0x01F0E, 0x01F06 }, { 0x01F0F, 0x01F07 },
    { 0x01F18, 0x01F10 }, { 0x01F19, 0x01F11 }, { 0x01F1A, 0x01F12 },
    { 0x01F1B, 0x01F13 }, { 0x01F1C, 0x01F14 }, { 0x01F1D, 0x01F15 },
    { 0x01F28, 0x01F20 }, { 0x01F29, 0x01F21 }, { 0x01F2A, 0x01F22 },
    { 0x01F2B, 0x01F23 }, { 0x01F2C, 0x01F24 }, { 0x01F2D, 0x01F25 },
    { 0x01F2E, 0x01F26 }, { 0x01F2F, 0x01F27 }, { 0x01F38, 0x01F30 },
    { 0x01F39, 0x01F31 }, { 0x01F3A, 0x01F32 }, { 0x01F3B, 0x01F33 },
    { 0x01F3C, 0x01F34 }, { 0x01F3D, 0x01F35 }, { 0x01F3E, 0x01F36 },
    { 0x01F3F, 0x01F37 }, { 0x01F48, 0x01F40 }, { 0x01F49, 0x01F41 },
    { 0x01F4A, 0x01F42 }, { 0x01F4B, 0x01F43 }, { 0x01F4C, 0x01F44 },
    { 0x01F4D, 0x01F45 }, { 0x01F59, 0x01F51 }, { 0x01F5B, 0x01F53 },
    { 0x01F5D, 0x01F55 }, { 0x01F5F, 0x01F57 }, { 0x01F68, 0x01F60 },
    { 0x01F69, 0x01F61 }, { 0x01F6A, 0x01F62 }, { 0x01F6B, 0x01F63 },
    { 0x01F6C, 0x01F64 }, { 0x01F6D, 0x01F65 }, { 0x01F6E, 0x01F66 },
    { 0x01F6F, 0x01F67 }, { 0x01F88, 0x01F80 }, { 0x01F89, 0x01F81 },
    { 0x01F8A, 0x01F82 }, { 0x01F8B, 0x01F83 }, { 0x01F8C, 0x01F84 },
    { 0x01F8D, 0x01F85 }, { 0x01F8E, 0x01F86 }, { 0x01F8F, 0x01F87 },
    { 0x01F98, 0x01F90 }, { 0x01F99, 0x01F91 }, { 0x01F9A, 0x01F92 },
    { 0x01F9B, 0x01F93 }, { 0x01F9C, 0x01F94 }, { 0x01F9D, 0x01F95 },
    { 0x01F9E, 0x01F96 }, { 0x01F9F, 0x01F97 }, { 0x01FA8, 0x01FA0 },
    { 0x01FA9, 0x01FA1 }, { 0x01FAA, 0x01FA2 }, { 0x01FAB, 0x01FA3 },
    { 0x01FAC, 0x01FA4 }, { 0x01FAD, 0x01FA5 }, { 0x01FAE, 0x01FA6 },
    { 0x01FAF, 0x01FA7 }, { 0x01FB8, 0x01FB0 }, { 0x01FB9, 0x01FB1 },
    { 0x01FBA, 0x01F70 }, { 0x01FBB, 0x01F71 }, { 0x01FBC, 0x01FB3 },
    { 0x01FC8, 0x01F72 }, { 0x01FC9, 0x01F73 }, { 0x01FCA, 0x01F74 },
    { 0x01FCB, 0x01F75 }, { 0x01FCC, 0x01FC3 }, { 0x01FD8, 0x01FD0 },
    { 0x01FD9, 0x01FD1 }, { 0x01FDA, 0x01F76 }, { 0x01FDB, 0x01F77 },
    { 0x01FE8, 0x01FE0 }, { 0x01FE9, 0x01FE1 }, { 0x01FEA, 0x01F7A },
    { 0x01FEB, 0x01F7B }, { 0x01FEC, 0x01FE5 }, { 0x01FF8, 0x01F78 },
    { 0x01FF9, 0x01F79 }, { 0x01FFA, 0x01F7C }, { 0x01FFB, 0x01F7D },
    { 0x01FFC, 0x01FF3 }, { 0x02126, 0x003C9 }, { 0x0212A, 0x0006B },
    { 0x0212B, 0x000E5 }, { 0x02160, 0x02170 }, { 0x02161, 0x02171 },
    { 0x02162, 0x02172 }, { 0x02163, 0x02173 }, { 0x02164, 0x02174 },
    { 0x02165, 0x02175 }, { 0x02166, 0x02176 }, { 0x02167, 0x02177 },
    { 0x02168, 0x02178 }, { 0x02169, 0x02179 }, { 0x0216A, 0x0217A },
    { 0x0216B, 0x0217B }, { 0x0216C, 0x0217C }, { 0x0216D, 0x0217D },
    { 0x0216E, 0x0217E }, { 0x0216F, 0x0217F }, { 0x024B6, 0x024D0 },
    { 0x024B7, 0x024D1 }, { 0x024B8, 0x024D2 }, { 0x024B9, 0x024D3 },
    { 0x024BA, 0x024D4 }, { 0x024BB, 0x024D5 }, { 0x024BC, 0x024D6 },
    { 0x024BD, 0x024D7 }, { 0x024BE, 0x024D8 }, { 0x024BF, 0x024D9 },
    { 0x024C0, 0x024DA }, { 0x024C1, 0x024DB }, { 0x024C2, 0x024DC },
    { 0x024C3, 0x024DD }, { 0x024C4, 0x024DE }, { 0x024C5, 0x024DF },
    { 0x024C6, 0x024E0 }, { 0x024C7, 0x024E1 }, { 0x024C8, 0x024E2 },
    { 0x024C9, 0x024E3 }, { 0x024CA, 0x024E4 }, { 0x024CB, 0x024E5 },
    { 0x024CC, 0x024E6 }, { 0x024CD, 0x024E7 }, { 0x024CE, 0x024E8 },
    { 0x024CF, 0x024E9 }, /*{ 0x02C00, 0x02C30 }, { 0x02C01, 0x02C31 },
    { 0x02C02, 0x02C32 }, { 0x02C03, 0x02C33 }, { 0x02C04, 0x02C34 },
    { 0x02C05, 0x02C35 }, { 0x02C06, 0x02C36 }, { 0x02C07, 0x02C37 },
    { 0x02C08, 0x02C38 }, { 0x02C09, 0x02C39 }, { 0x02C0A, 0x02C3A },
    { 0x02C0B, 0x02C3B }, { 0x02C0C, 0x02C3C }, { 0x02C0D, 0x02C3D },
    { 0x02C0E, 0x02C3E }, { 0x02C0F, 0x02C3F }, { 0x02C10, 0x02C40 },
    { 0x02C11, 0x02C41 }, { 0x02C12, 0x02C42 }, { 0x02C13, 0x02C43 },
    { 0x02C14, 0x02C44 }, { 0x02C15, 0x02C45 }, { 0x02C16, 0x02C46 },
    { 0x02C17, 0x02C47 }, { 0x02C18, 0x02C48 }, { 0x02C19, 0x02C49 },
    { 0x02C1A, 0x02C4A }, { 0x02C1B, 0x02C4B }, { 0x02C1C, 0x02C4C },
    { 0x02C1D, 0x02C4D }, { 0x02C1E, 0x02C4E }, { 0x02C1F, 0x02C4F },
    { 0x02C20, 0x02C50 }, { 0x02C21, 0x02C51 }, { 0x02C22, 0x02C52 },
    { 0x02C23, 0x02C53 }, { 0x02C24, 0x02C54 }, { 0x02C25, 0x02C55 },
    { 0x02C26, 0x02C56 }, { 0x02C27, 0x02C57 }, { 0x02C28, 0x02C58 },
    { 0x02C29, 0x02C59 }, { 0x02C2A, 0x02C5A }, { 0x02C2B, 0x02C5B },
    { 0x02C2C, 0x02C5C }, { 0x02C2D, 0x02C5D }, { 0x02C2E, 0x02C5E },
    { 0x02C80, 0x02C81 }, { 0x02C82, 0x02C83 }, { 0x02C84, 0x02C85 },
    { 0x02C86, 0x02C87 }, { 0x02C88, 0x02C89 }, { 0x02C8A, 0x02C8B },
    { 0x02C8C, 0x02C8D }, { 0x02C8E, 0x02C8F }, { 0x02C90, 0x02C91 },
    { 0x02C92, 0x02C93 }, { 0x02C94, 0x02C95 }, { 0x02C96, 0x02C97 },
    { 0x02C98, 0x02C99 }, { 0x02C9A, 0x02C9B }, { 0x02C9C, 0x02C9D },
    { 0x02C9E, 0x02C9F }, { 0x02CA0, 0x02CA1 }, { 0x02CA2, 0x02CA3 },
    { 0x02CA4, 0x02CA5 }, { 0x02CA6, 0x02CA7 }, { 0x02CA8, 0x02CA9 },
    { 0x02CAA, 0x02CAB }, { 0x02CAC, 0x02CAD }, { 0x02CAE, 0x02CAF },
    { 0x02CB0, 0x02CB1 }, { 0x02CB2, 0x02CB3 }, { 0x02CB4, 0x02CB5 },
    { 0x02CB6, 0x02CB7 }, { 0x02CB8, 0x02CB9 }, { 0x02CBA, 0x02CBB },
    { 0x02CBC, 0x02CBD }, { 0x02CBE, 0x02CBF }, { 0x02CC0, 0x02CC1 },
    { 0x02CC2, 0x02CC3 }, { 0x02CC4, 0x02CC5 }, { 0x02CC6, 0x02CC7 },
    { 0x02CC8, 0x02CC9 }, { 0x02CCA, 0x02CCB }, { 0x02CCC, 0x02CCD },
    { 0x02CCE, 0x02CCF }, { 0x02CD0, 0x02CD1 }, { 0x02CD2, 0x02CD3 },
    { 0x02CD4, 0x02CD5 }, { 0x02CD6, 0x02CD7 }, { 0x02CD8, 0x02CD9 },
    { 0x02CDA, 0x02CDB }, { 0x02CDC, 0x02CDD }, { 0x02CDE, 0x02CDF },
    { 0x02CE0, 0x02CE1 }, { 0x02CE2, 0x02CE3 },*/ { 0x0FF21, 0x0FF41 },
    { 0x0FF22, 0x0FF42 }, { 0x0FF23, 0x0FF43 }, { 0x0FF24, 0x0FF44 },
    { 0x0FF25, 0x0FF45 }, { 0x0FF26, 0x0FF46 }, { 0x0FF27, 0x0FF47 },
    { 0x0FF28, 0x0FF48 }, { 0x0FF29, 0x0FF49 }, { 0x0FF2A, 0x0FF4A },
    { 0x0FF2B, 0x0FF4B }, { 0x0FF2C, 0x0FF4C }, { 0x0FF2D, 0x0FF4D },
    { 0x0FF2E, 0x0FF4E }, { 0x0FF2F, 0x0FF4F }, { 0x0FF30, 0x0FF50 },
    { 0x0FF31, 0x0FF51 }, { 0x0FF32, 0x0FF52 }, { 0x0FF33, 0x0FF53 },
    { 0x0FF34, 0x0FF54 }, { 0x0FF35, 0x0FF55 }, { 0x0FF36, 0x0FF56 },
    { 0x0FF37, 0x0FF57 }, { 0x0FF38, 0x0FF58 }, { 0x0FF39, 0x0FF59 },
    // the following commented out ones fail on OS X 10.5 Leopard
    { 0x0FF3A, 0x0FF5A }/*, { 0x10400, 0x10428 }, { 0x10401, 0x10429 },
    { 0x10402, 0x1042A }, { 0x10403, 0x1042B }, { 0x10404, 0x1042C },
    { 0x10405, 0x1042D }, { 0x10406, 0x1042E }, { 0x10407, 0x1042F },
    { 0x10408, 0x10430 }, { 0x10409, 0x10431 }, { 0x1040A, 0x10432 },
    { 0x1040B, 0x10433 }, { 0x1040C, 0x10434 }, { 0x1040D, 0x10435 },
    { 0x1040E, 0x10436 }, { 0x1040F, 0x10437 }, { 0x10410, 0x10438 },
    { 0x10411, 0x10439 }, { 0x10412, 0x1043A }, { 0x10413, 0x1043B },
    { 0x10414, 0x1043C }, { 0x10415, 0x1043D }, { 0x10416, 0x1043E },
    { 0x10417, 0x1043F }, { 0x10418, 0x10440 }, { 0x10419, 0x10441 },
    { 0x1041A, 0x10442 }, { 0x1041B, 0x10443 }, { 0x1041C, 0x10444 },
    { 0x1041D, 0x10445 }, { 0x1041E, 0x10446 }, { 0x1041F, 0x10447 },
    { 0x10420, 0x10448 }, { 0x10421, 0x10449 }, { 0x10422, 0x1044A },
    { 0x10423, 0x1044B }, { 0x10424, 0x1044C }, { 0x10425, 0x1044D },
    { 0x10426, 0x1044E }, { 0x10427, 0x1044F } */
  };
  unsigned char utf8str_upper[5];
  unsigned char utf8str_lower[5];
  for (int i = 0; i < ARRAYSIZE(kUpperToLowerMap); i++) {
    int len;
    len = UTF32ToUTF8(kUpperToLowerMap[i].upper, utf8str_upper);
    CHECK_NE(0, len);
    utf8str_upper[len] = '\0';
    len = UTF32ToUTF8(kUpperToLowerMap[i].lower, utf8str_lower);
    CHECK_NE(0, len);
    utf8str_lower[len] = '\0';
    int result = ComparePathNames(
        UTF8ToPathStringQuick(reinterpret_cast<char*>(utf8str_upper)),
        UTF8ToPathStringQuick(reinterpret_cast<char*>(utf8str_lower)));
    if (0 != result) {
      // This ugly strstream works around an issue where using << hex on the
      // stream for ADD_FAILURE produces "true" and "false" in the output.
      strstream msg;
      msg << "ComparePathNames(0x" << hex << kUpperToLowerMap[i].upper
        << ", 0x" << hex << kUpperToLowerMap[i].lower
        << ") returned " << dec << result << "; expected 0" << '\0';
      ADD_FAILURE() << msg.str();
    }
  }
#endif  // not defined OS_WIN
}

#ifdef OS_WIN
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
#endif  // OS_WIN

}  // namespace

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
    Blob value_blob(value, value + ARRAYSIZE(value));
    ext.mutable_value()->swap(value_blob);
    ext.delete_attribute();
  }
  // This call to SaveChanges used to CHECK fail.
  dir_.get()->SaveChanges();
}

}  // namespace syncable
