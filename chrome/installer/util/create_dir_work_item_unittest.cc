// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "base/base_paths.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "chrome/installer/util/work_item.h"
#include "chrome/installer/util/create_dir_work_item.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
  class CreateDirWorkItemTest : public testing::Test {
   protected:
    virtual void SetUp() {
      // Name a subdirectory of the temp directory.
      ASSERT_TRUE(PathService::Get(base::DIR_TEMP, &test_dir_));
      test_dir_ = test_dir_.AppendASCII("CreateDirWorkItemTest");

      // Create a fresh, empty copy of this directory.
      file_util::Delete(test_dir_, true);
      file_util::CreateDirectoryW(test_dir_);
    }
    virtual void TearDown() {
      // Clean up test directory
      ASSERT_TRUE(file_util::Delete(test_dir_, false));
      ASSERT_FALSE(file_util::PathExists(test_dir_));
    }

    // the path to temporary directory used to contain the test operations
    FilePath test_dir_;
  };
};

TEST_F(CreateDirWorkItemTest, CreatePath) {
  FilePath parent_dir(test_dir_);
  parent_dir = parent_dir.AppendASCII("a");
  CreateDirectory(parent_dir.value().c_str(), NULL);
  ASSERT_TRUE(file_util::PathExists(parent_dir));

  FilePath top_dir_to_create(parent_dir);
  top_dir_to_create = top_dir_to_create.AppendASCII("b");

  FilePath dir_to_create(top_dir_to_create);
  dir_to_create = dir_to_create.AppendASCII("c");
  dir_to_create = dir_to_create.AppendASCII("d");

  scoped_ptr<CreateDirWorkItem> work_item(
      WorkItem::CreateCreateDirWorkItem(dir_to_create.ToWStringHack()));

  EXPECT_TRUE(work_item->Do());

  EXPECT_TRUE(file_util::PathExists(dir_to_create));

  work_item->Rollback();

  // Rollback should delete all the paths up to top_dir_to_create.
  EXPECT_FALSE(file_util::PathExists(top_dir_to_create));
  EXPECT_TRUE(file_util::PathExists(parent_dir));
}

TEST_F(CreateDirWorkItemTest, CreateExistingPath) {
  FilePath dir_to_create(test_dir_);
  dir_to_create = dir_to_create.AppendASCII("aa");
  CreateDirectory(dir_to_create.value().c_str(), NULL);
  ASSERT_TRUE(file_util::PathExists(dir_to_create));

  scoped_ptr<CreateDirWorkItem> work_item(
      WorkItem::CreateCreateDirWorkItem(dir_to_create.ToWStringHack()));

  EXPECT_TRUE(work_item->Do());

  EXPECT_TRUE(file_util::PathExists(dir_to_create));

  work_item->Rollback();

  // Rollback should not remove the path since it exists before
  // the CreateDirWorkItem is called.
  EXPECT_TRUE(file_util::PathExists(dir_to_create));
}

TEST_F(CreateDirWorkItemTest, CreateSharedPath) {
  FilePath dir_to_create_1(test_dir_);
  dir_to_create_1 = dir_to_create_1.AppendASCII("aaa");

  FilePath dir_to_create_2(dir_to_create_1);
  dir_to_create_2 = dir_to_create_2.AppendASCII("bbb");

  FilePath dir_to_create_3(dir_to_create_2);
  dir_to_create_3 = dir_to_create_3.AppendASCII("ccc");

  scoped_ptr<CreateDirWorkItem> work_item(
      WorkItem::CreateCreateDirWorkItem(dir_to_create_3.ToWStringHack()));

  EXPECT_TRUE(work_item->Do());

  EXPECT_TRUE(file_util::PathExists(dir_to_create_3));

  // Create another directory under dir_to_create_2
  FilePath dir_to_create_4(dir_to_create_2);
  dir_to_create_4 = dir_to_create_4.AppendASCII("ddd");
  CreateDirectory(dir_to_create_4.value().c_str(), NULL);
  ASSERT_TRUE(file_util::PathExists(dir_to_create_4));

  work_item->Rollback();

  // Rollback should delete dir_to_create_3.
  EXPECT_FALSE(file_util::PathExists(dir_to_create_3));

  // Rollback should not delete dir_to_create_2 as it is shared.
  EXPECT_TRUE(file_util::PathExists(dir_to_create_2));
  EXPECT_TRUE(file_util::PathExists(dir_to_create_4));
}

TEST_F(CreateDirWorkItemTest, RollbackWithMissingDir) {
  FilePath dir_to_create_1(test_dir_);
  dir_to_create_1 = dir_to_create_1.AppendASCII("aaaa");

  FilePath dir_to_create_2(dir_to_create_1);
  dir_to_create_2 = dir_to_create_2.AppendASCII("bbbb");

  FilePath dir_to_create_3(dir_to_create_2);
  dir_to_create_3 = dir_to_create_3.AppendASCII("cccc");

  scoped_ptr<CreateDirWorkItem> work_item(
      WorkItem::CreateCreateDirWorkItem(dir_to_create_3.ToWStringHack()));

  EXPECT_TRUE(work_item->Do());

  EXPECT_TRUE(file_util::PathExists(dir_to_create_3));

  RemoveDirectory(dir_to_create_3.value().c_str());
  ASSERT_FALSE(file_util::PathExists(dir_to_create_3));

  work_item->Rollback();

  // dir_to_create_3 has already been deleted, Rollback should delete
  // the rest.
  EXPECT_FALSE(file_util::PathExists(dir_to_create_1));
}
