// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <fstream>

#include "base/base_paths.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "chrome/installer/util/delete_tree_work_item.h"
#include "chrome/installer/util/work_item.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
  class DeleteTreeWorkItemTest : public testing::Test {
   protected:
    virtual void SetUp() {
      ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    }

    // The temporary directory used to contain the test operations.
    base::ScopedTempDir temp_dir_;
  };

  // Simple function to dump some text into a new file.
  void CreateTextFile(const std::wstring& filename,
                      const std::wstring& contents) {
    std::ofstream file;
    file.open(filename.c_str());
    ASSERT_TRUE(file.is_open());
    file << contents;
    file.close();
  }

  wchar_t text_content_1[] = L"delete me";
  wchar_t text_content_2[] = L"delete me as well";
};

// Delete a tree without key path. Everything should be deleted.
TEST_F(DeleteTreeWorkItemTest, DeleteTreeNoKeyPath) {
  // Create tree to be deleted.
  base::FilePath dir_name_delete(temp_dir_.path());
  dir_name_delete = dir_name_delete.AppendASCII("to_be_delete");
  base::CreateDirectory(dir_name_delete);
  ASSERT_TRUE(base::PathExists(dir_name_delete));

  base::FilePath dir_name_delete_1(dir_name_delete);
  dir_name_delete_1 = dir_name_delete_1.AppendASCII("1");
  base::CreateDirectory(dir_name_delete_1);
  ASSERT_TRUE(base::PathExists(dir_name_delete_1));

  base::FilePath dir_name_delete_2(dir_name_delete);
  dir_name_delete_2 = dir_name_delete_2.AppendASCII("2");
  base::CreateDirectory(dir_name_delete_2);
  ASSERT_TRUE(base::PathExists(dir_name_delete_2));

  base::FilePath file_name_delete_1(dir_name_delete_1);
  file_name_delete_1 = file_name_delete_1.AppendASCII("File_1.txt");
  CreateTextFile(file_name_delete_1.value(), text_content_1);
  ASSERT_TRUE(base::PathExists(file_name_delete_1));

  base::FilePath file_name_delete_2(dir_name_delete_2);
  file_name_delete_2 = file_name_delete_2.AppendASCII("File_2.txt");
  CreateTextFile(file_name_delete_2.value(), text_content_1);
  ASSERT_TRUE(base::PathExists(file_name_delete_2));

  // Test Do().
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  std::vector<base::FilePath> key_files;
  scoped_ptr<DeleteTreeWorkItem> work_item(
      WorkItem::CreateDeleteTreeWorkItem(dir_name_delete, temp_dir.path(),
                                         key_files));
  EXPECT_TRUE(work_item->Do());

  // everything should be gone
  EXPECT_FALSE(base::PathExists(file_name_delete_1));
  EXPECT_FALSE(base::PathExists(file_name_delete_2));
  EXPECT_FALSE(base::PathExists(dir_name_delete));

  work_item->Rollback();
  // everything should come back
  EXPECT_TRUE(base::PathExists(file_name_delete_1));
  EXPECT_TRUE(base::PathExists(file_name_delete_2));
  EXPECT_TRUE(base::PathExists(dir_name_delete));
}


// Delete a tree with keypath but not in use. Everything should be gone.
// Rollback should bring back everything
TEST_F(DeleteTreeWorkItemTest, DeleteTree) {
  // Create tree to be deleted
  base::FilePath dir_name_delete(temp_dir_.path());
  dir_name_delete = dir_name_delete.AppendASCII("to_be_delete");
  base::CreateDirectory(dir_name_delete);
  ASSERT_TRUE(base::PathExists(dir_name_delete));

  base::FilePath dir_name_delete_1(dir_name_delete);
  dir_name_delete_1 = dir_name_delete_1.AppendASCII("1");
  base::CreateDirectory(dir_name_delete_1);
  ASSERT_TRUE(base::PathExists(dir_name_delete_1));

  base::FilePath dir_name_delete_2(dir_name_delete);
  dir_name_delete_2 = dir_name_delete_2.AppendASCII("2");
  base::CreateDirectory(dir_name_delete_2);
  ASSERT_TRUE(base::PathExists(dir_name_delete_2));

  base::FilePath file_name_delete_1(dir_name_delete_1);
  file_name_delete_1 = file_name_delete_1.AppendASCII("File_1.txt");
  CreateTextFile(file_name_delete_1.value(), text_content_1);
  ASSERT_TRUE(base::PathExists(file_name_delete_1));

  base::FilePath file_name_delete_2(dir_name_delete_2);
  file_name_delete_2 = file_name_delete_2.AppendASCII("File_2.txt");
  CreateTextFile(file_name_delete_2.value(), text_content_1);
  ASSERT_TRUE(base::PathExists(file_name_delete_2));

  // test Do()
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  std::vector<base::FilePath> key_files(1, file_name_delete_1);
  scoped_ptr<DeleteTreeWorkItem> work_item(
      WorkItem::CreateDeleteTreeWorkItem(dir_name_delete, temp_dir.path(),
                                         key_files));
  EXPECT_TRUE(work_item->Do());

  // everything should be gone
  EXPECT_FALSE(base::PathExists(file_name_delete_1));
  EXPECT_FALSE(base::PathExists(file_name_delete_2));
  EXPECT_FALSE(base::PathExists(dir_name_delete));

  work_item->Rollback();
  // everything should come back
  EXPECT_TRUE(base::PathExists(file_name_delete_1));
  EXPECT_TRUE(base::PathExists(file_name_delete_2));
  EXPECT_TRUE(base::PathExists(dir_name_delete));
}

// Delete a tree with key_path in use. Everything should still be there.
TEST_F(DeleteTreeWorkItemTest, DeleteTreeInUse) {
  // Create tree to be deleted
  base::FilePath dir_name_delete(temp_dir_.path());
  dir_name_delete = dir_name_delete.AppendASCII("to_be_delete");
  base::CreateDirectory(dir_name_delete);
  ASSERT_TRUE(base::PathExists(dir_name_delete));

  base::FilePath dir_name_delete_1(dir_name_delete);
  dir_name_delete_1 = dir_name_delete_1.AppendASCII("1");
  base::CreateDirectory(dir_name_delete_1);
  ASSERT_TRUE(base::PathExists(dir_name_delete_1));

  base::FilePath dir_name_delete_2(dir_name_delete);
  dir_name_delete_2 = dir_name_delete_2.AppendASCII("2");
  base::CreateDirectory(dir_name_delete_2);
  ASSERT_TRUE(base::PathExists(dir_name_delete_2));

  base::FilePath file_name_delete_1(dir_name_delete_1);
  file_name_delete_1 = file_name_delete_1.AppendASCII("File_1.txt");
  CreateTextFile(file_name_delete_1.value(), text_content_1);
  ASSERT_TRUE(base::PathExists(file_name_delete_1));

  base::FilePath file_name_delete_2(dir_name_delete_2);
  file_name_delete_2 = file_name_delete_2.AppendASCII("File_2.txt");
  CreateTextFile(file_name_delete_2.value(), text_content_1);
  ASSERT_TRUE(base::PathExists(file_name_delete_2));

  // Create a key path file.
  base::FilePath key_path(dir_name_delete);
  key_path = key_path.AppendASCII("key_file.exe");

  wchar_t exe_full_path_str[MAX_PATH];
  ::GetModuleFileNameW(NULL, exe_full_path_str, MAX_PATH);
  base::FilePath exe_full_path(exe_full_path_str);

  base::CopyFile(exe_full_path, key_path);
  ASSERT_TRUE(base::PathExists(key_path));

  VLOG(1) << "copy ourself from " << exe_full_path.value()
          << " to " << key_path.value();

  // Run the key path file to keep it in use.
  STARTUPINFOW si = {sizeof(si)};
  PROCESS_INFORMATION pi = {0};
  base::FilePath::StringType writable_key_path = key_path.value();
  ASSERT_TRUE(
      ::CreateProcessW(NULL, &writable_key_path[0],
                       NULL, NULL, FALSE, CREATE_NO_WINDOW | CREATE_SUSPENDED,
                       NULL, NULL, &si, &pi));

  // test Do().
  {
    base::ScopedTempDir temp_dir;
    ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

    std::vector<base::FilePath> key_paths(1, key_path);
    scoped_ptr<DeleteTreeWorkItem> work_item(
        WorkItem::CreateDeleteTreeWorkItem(dir_name_delete, temp_dir.path(),
                                           key_paths));

    // delete should fail as file in use.
    EXPECT_FALSE(work_item->Do());
  }

  // verify everything is still there.
  EXPECT_TRUE(base::PathExists(key_path));
  EXPECT_TRUE(base::PathExists(file_name_delete_1));
  EXPECT_TRUE(base::PathExists(file_name_delete_2));

  TerminateProcess(pi.hProcess, 0);
  // make sure the handle is closed.
  WaitForSingleObject(pi.hProcess, INFINITE);
}
