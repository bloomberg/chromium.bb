// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <fstream>

#include "base/base_paths.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "chrome/installer/util/work_item.h"
#include "chrome/installer/util/move_tree_work_item.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
  class MoveTreeWorkItemTest : public testing::Test {
   protected:
    virtual void SetUp() {
      // Name a subdirectory of the user temp directory.
      ASSERT_TRUE(PathService::Get(base::DIR_TEMP, &test_dir_));
      test_dir_ = test_dir_.AppendASCII("MoveTreeWorkItemTest");

      // Create a fresh, empty copy of this test directory.
      file_util::Delete(test_dir_, true);
      file_util::CreateDirectoryW(test_dir_);

      // Create a tempory directory under the test directory.
      temp_dir_ = test_dir_.AppendASCII("temp");
      file_util::CreateDirectoryW(temp_dir_);

      ASSERT_TRUE(file_util::PathExists(test_dir_));
      ASSERT_TRUE(file_util::PathExists(temp_dir_));
    }

    virtual void TearDown() {
      // Clean up test directory
      ASSERT_TRUE(file_util::Delete(test_dir_, true));
      ASSERT_FALSE(file_util::PathExists(test_dir_));
    }

    // the path to temporary directory used to contain the test operations
    FilePath test_dir_;
    FilePath temp_dir_;
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

  // Simple function to read text from a file.
  std::wstring ReadTextFile(const FilePath& path) {
    WCHAR contents[64];
    std::wifstream file;
    file.open(path.value().c_str());
    EXPECT_TRUE(file.is_open());
    file.getline(contents, 64);
    file.close();
    return std::wstring(contents);
  }

  wchar_t text_content_1[] = L"Gooooooooooooooooooooogle";
  wchar_t text_content_2[] = L"Overwrite Me";
};

// Move one directory from source to destination when destination does not
// exist.
TEST_F(MoveTreeWorkItemTest, MoveDirectory) {
  // Create two level deep source dir
  FilePath from_dir1(test_dir_);
  from_dir1 = from_dir1.AppendASCII("From_Dir1");
  file_util::CreateDirectory(from_dir1);
  ASSERT_TRUE(file_util::PathExists(from_dir1));

  FilePath from_dir2(from_dir1);
  from_dir2 = from_dir2.AppendASCII("From_Dir2");
  file_util::CreateDirectory(from_dir2);
  ASSERT_TRUE(file_util::PathExists(from_dir2));

  FilePath from_file(from_dir2);
  from_file = from_file.AppendASCII("From_File");
  CreateTextFile(from_file.value(), text_content_1);
  ASSERT_TRUE(file_util::PathExists(from_file));

  // Generate destination path
  FilePath to_dir(test_dir_);
  to_dir = to_dir.AppendASCII("To_Dir");
  ASSERT_FALSE(file_util::PathExists(to_dir));

  FilePath to_file(to_dir);
  to_file = to_file.AppendASCII("From_Dir2");
  to_file = to_file.AppendASCII("From_File");
  ASSERT_FALSE(file_util::PathExists(to_file));

  // test Do()
  scoped_ptr<MoveTreeWorkItem> work_item(
      WorkItem::CreateMoveTreeWorkItem(from_dir1, to_dir, temp_dir_));
  EXPECT_TRUE(work_item->Do());

  EXPECT_FALSE(file_util::PathExists(from_dir1));
  EXPECT_TRUE(file_util::PathExists(to_dir));
  EXPECT_TRUE(file_util::PathExists(to_file));

  // test rollback()
  work_item->Rollback();

  EXPECT_TRUE(file_util::PathExists(from_dir1));
  EXPECT_TRUE(file_util::PathExists(from_file));
  EXPECT_FALSE(file_util::PathExists(to_dir));
}

// Move one directory from source to destination when destination already
// exists.
TEST_F(MoveTreeWorkItemTest, MoveDirectoryDestExists) {
  // Create two level deep source dir
  FilePath from_dir1(test_dir_);
  from_dir1 = from_dir1.AppendASCII("From_Dir1");
  file_util::CreateDirectory(from_dir1);
  ASSERT_TRUE(file_util::PathExists(from_dir1));

  FilePath from_dir2(from_dir1);
  from_dir2 = from_dir2.AppendASCII("From_Dir2");
  file_util::CreateDirectory(from_dir2);
  ASSERT_TRUE(file_util::PathExists(from_dir2));

  FilePath from_file(from_dir2);
  from_file = from_file.AppendASCII("From_File");
  CreateTextFile(from_file.value(), text_content_1);
  ASSERT_TRUE(file_util::PathExists(from_file));

  // Create destination path
  FilePath to_dir(test_dir_);
  to_dir = to_dir.AppendASCII("To_Dir");
  file_util::CreateDirectory(to_dir);
  ASSERT_TRUE(file_util::PathExists(to_dir));

  FilePath orig_to_file(to_dir);
  orig_to_file = orig_to_file.AppendASCII("To_File");
  CreateTextFile(orig_to_file.value(), text_content_2);
  ASSERT_TRUE(file_util::PathExists(orig_to_file));

  FilePath new_to_file(to_dir);
  new_to_file = new_to_file.AppendASCII("From_Dir2");
  new_to_file = new_to_file.AppendASCII("From_File");
  ASSERT_FALSE(file_util::PathExists(new_to_file));

  // test Do()
  scoped_ptr<MoveTreeWorkItem> work_item(
      WorkItem::CreateMoveTreeWorkItem(from_dir1, to_dir, temp_dir_));
  EXPECT_TRUE(work_item->Do());

  EXPECT_FALSE(file_util::PathExists(from_dir1));
  EXPECT_TRUE(file_util::PathExists(to_dir));
  EXPECT_TRUE(file_util::PathExists(new_to_file));
  EXPECT_FALSE(file_util::PathExists(orig_to_file));

  // test rollback()
  work_item->Rollback();

  EXPECT_TRUE(file_util::PathExists(from_dir1));
  EXPECT_TRUE(file_util::PathExists(to_dir));
  EXPECT_FALSE(file_util::PathExists(new_to_file));
  EXPECT_TRUE(file_util::PathExists(orig_to_file));
  EXPECT_EQ(0, ReadTextFile(orig_to_file).compare(text_content_2));
  EXPECT_EQ(0, ReadTextFile(from_file).compare(text_content_1));
}

// Move one file from source to destination when destination does not
// exist.
TEST_F(MoveTreeWorkItemTest, MoveAFile) {
  // Create a file inside source dir
  FilePath from_dir(test_dir_);
  from_dir = from_dir.AppendASCII("From_Dir");
  file_util::CreateDirectory(from_dir);
  ASSERT_TRUE(file_util::PathExists(from_dir));

  FilePath from_file(from_dir);
  from_file = from_file.AppendASCII("From_File");
  CreateTextFile(from_file.value(), text_content_1);
  ASSERT_TRUE(file_util::PathExists(from_file));

  // Generate destination file name
  FilePath to_file(test_dir_);
  to_file = to_file.AppendASCII("To_File");
  ASSERT_FALSE(file_util::PathExists(to_file));

  // test Do()
  scoped_ptr<MoveTreeWorkItem> work_item(
      WorkItem::CreateMoveTreeWorkItem(from_file, to_file, temp_dir_));
  EXPECT_TRUE(work_item->Do());

  EXPECT_TRUE(file_util::PathExists(from_dir));
  EXPECT_FALSE(file_util::PathExists(from_file));
  EXPECT_TRUE(file_util::PathExists(to_file));
  EXPECT_EQ(0, ReadTextFile(to_file).compare(text_content_1));

  // test rollback()
  work_item->Rollback();

  EXPECT_TRUE(file_util::PathExists(from_dir));
  EXPECT_TRUE(file_util::PathExists(from_file));
  EXPECT_FALSE(file_util::PathExists(to_file));
  EXPECT_EQ(0, ReadTextFile(from_file).compare(text_content_1));
}

// Move one file from source to destination when destination already
// exists.
TEST_F(MoveTreeWorkItemTest, MoveFileDestExists) {
  // Create a file inside source dir
  FilePath from_dir(test_dir_);
  from_dir = from_dir.AppendASCII("From_Dir");
  file_util::CreateDirectory(from_dir);
  ASSERT_TRUE(file_util::PathExists(from_dir));

  FilePath from_file(from_dir);
  from_file = from_file.AppendASCII("From_File");
  CreateTextFile(from_file.value(), text_content_1);
  ASSERT_TRUE(file_util::PathExists(from_file));

  // Create destination path
  FilePath to_dir(test_dir_);
  to_dir = to_dir.AppendASCII("To_Dir");
  file_util::CreateDirectory(to_dir);
  ASSERT_TRUE(file_util::PathExists(to_dir));

  FilePath to_file(to_dir);
  to_file = to_file.AppendASCII("To_File");
  CreateTextFile(to_file.value(), text_content_2);
  ASSERT_TRUE(file_util::PathExists(to_file));

  // test Do()
  scoped_ptr<MoveTreeWorkItem> work_item(
      WorkItem::CreateMoveTreeWorkItem(from_file, to_dir, temp_dir_));
  EXPECT_TRUE(work_item->Do());

  EXPECT_TRUE(file_util::PathExists(from_dir));
  EXPECT_FALSE(file_util::PathExists(from_file));
  EXPECT_TRUE(file_util::PathExists(to_dir));
  EXPECT_FALSE(file_util::PathExists(to_file));
  EXPECT_EQ(0, ReadTextFile(to_dir).compare(text_content_1));

  // test rollback()
  work_item->Rollback();

  EXPECT_TRUE(file_util::PathExists(from_dir));
  EXPECT_EQ(0, ReadTextFile(from_file).compare(text_content_1));
  EXPECT_TRUE(file_util::PathExists(to_dir));
  EXPECT_EQ(0, ReadTextFile(to_file).compare(text_content_2));
}

// Move one file from source to destination when destination already
// exists and is in use.
TEST_F(MoveTreeWorkItemTest, MoveFileDestInUse) {
  // Create a file inside source dir
  FilePath from_dir(test_dir_);
  from_dir = from_dir.AppendASCII("From_Dir");
  file_util::CreateDirectory(from_dir);
  ASSERT_TRUE(file_util::PathExists(from_dir));

  FilePath from_file(from_dir);
  from_file = from_file.AppendASCII("From_File");
  CreateTextFile(from_file.value(), text_content_1);
  ASSERT_TRUE(file_util::PathExists(from_file));

  // Create an executable in destination path by copying ourself to it.
  FilePath to_dir(test_dir_);
  to_dir = to_dir.AppendASCII("To_Dir");
  file_util::CreateDirectory(to_dir);
  ASSERT_TRUE(file_util::PathExists(to_dir));

  wchar_t exe_full_path_str[MAX_PATH];
  ::GetModuleFileName(NULL, exe_full_path_str, MAX_PATH);
  FilePath exe_full_path(exe_full_path_str);
  FilePath to_file(to_dir);
  to_file = to_file.AppendASCII("To_File");
  file_util::CopyFile(exe_full_path, to_file);
  ASSERT_TRUE(file_util::PathExists(to_file));

  // Run the executable in destination path
  STARTUPINFOW si = {sizeof(si)};
  PROCESS_INFORMATION pi = {0};
  ASSERT_TRUE(::CreateProcess(NULL,
                              const_cast<wchar_t*>(to_file.value().c_str()),
                              NULL, NULL, FALSE,
                              CREATE_NO_WINDOW | CREATE_SUSPENDED,
                              NULL, NULL, &si, &pi));

  // test Do()
  scoped_ptr<MoveTreeWorkItem> work_item(
      WorkItem::CreateMoveTreeWorkItem(from_file, to_file, temp_dir_));
  EXPECT_TRUE(work_item->Do());

  EXPECT_TRUE(file_util::PathExists(from_dir));
  EXPECT_FALSE(file_util::PathExists(from_file));
  EXPECT_TRUE(file_util::PathExists(to_dir));
  EXPECT_EQ(0, ReadTextFile(to_file).compare(text_content_1));

  // test rollback()
  work_item->Rollback();

  EXPECT_TRUE(file_util::PathExists(from_dir));
  EXPECT_EQ(0, ReadTextFile(from_file).compare(text_content_1));
  EXPECT_TRUE(file_util::PathExists(to_dir));
  EXPECT_TRUE(file_util::ContentsEqual(exe_full_path, to_file));

  TerminateProcess(pi.hProcess, 0);
  EXPECT_TRUE(WaitForSingleObject(pi.hProcess, 10000) == WAIT_OBJECT_0);
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);
}

// Move one file that is in use to destination.
TEST_F(MoveTreeWorkItemTest, MoveFileInUse) {
  // Create an executable for source by copying ourself to a new source dir.
  FilePath from_dir(test_dir_);
  from_dir = from_dir.AppendASCII("From_Dir");
  file_util::CreateDirectory(from_dir);
  ASSERT_TRUE(file_util::PathExists(from_dir));

  wchar_t exe_full_path_str[MAX_PATH];
  ::GetModuleFileName(NULL, exe_full_path_str, MAX_PATH);
  FilePath exe_full_path(exe_full_path_str);
  FilePath from_file(from_dir);
  from_file = from_file.AppendASCII("From_File");
  file_util::CopyFile(exe_full_path, from_file);
  ASSERT_TRUE(file_util::PathExists(from_file));

  // Create a destination source dir and generate destination file name.
  FilePath to_dir(test_dir_);
  to_dir = to_dir.AppendASCII("To_Dir");
  file_util::CreateDirectory(to_dir);
  ASSERT_TRUE(file_util::PathExists(to_dir));

  FilePath to_file(to_dir);
  to_file = to_file.AppendASCII("To_File");
  CreateTextFile(to_file.value(), text_content_1);
  ASSERT_TRUE(file_util::PathExists(to_file));

  // Run the executable in source path
  STARTUPINFOW si = {sizeof(si)};
  PROCESS_INFORMATION pi = {0};
  ASSERT_TRUE(::CreateProcess(NULL,
                              const_cast<wchar_t*>(from_file.value().c_str()),
                              NULL, NULL, FALSE,
                              CREATE_NO_WINDOW | CREATE_SUSPENDED,
                              NULL, NULL, &si, &pi));

  // test Do()
  scoped_ptr<MoveTreeWorkItem> work_item(
      WorkItem::CreateMoveTreeWorkItem(from_file, to_file, temp_dir_));
  EXPECT_TRUE(work_item->Do());

  EXPECT_TRUE(file_util::PathExists(from_dir));
  EXPECT_FALSE(file_util::PathExists(from_file));
  EXPECT_TRUE(file_util::PathExists(to_dir));
  EXPECT_TRUE(file_util::ContentsEqual(exe_full_path, to_file));

  // Close the process and make sure all the conditions after Do() are
  // still true.
  TerminateProcess(pi.hProcess, 0);
  EXPECT_TRUE(WaitForSingleObject(pi.hProcess, 10000) == WAIT_OBJECT_0);
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);

  EXPECT_TRUE(file_util::PathExists(from_dir));
  EXPECT_FALSE(file_util::PathExists(from_file));
  EXPECT_TRUE(file_util::PathExists(to_dir));
  EXPECT_TRUE(file_util::ContentsEqual(exe_full_path, to_file));

  // test rollback()
  work_item->Rollback();

  EXPECT_TRUE(file_util::PathExists(from_dir));
  EXPECT_TRUE(file_util::ContentsEqual(exe_full_path, from_file));
  EXPECT_TRUE(file_util::PathExists(to_dir));
  EXPECT_EQ(0, ReadTextFile(to_file).compare(text_content_1));
}
