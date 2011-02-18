// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <fstream>

#include "base/base_paths.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "base/threading/platform_thread.h"
#include "chrome/installer/util/work_item.h"
#include "chrome/installer/util/copy_tree_work_item.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
  class CopyTreeWorkItemTest : public testing::Test {
   protected:
    virtual void SetUp() {
      // Name a subdirectory of the user temp directory.
      ASSERT_TRUE(PathService::Get(base::DIR_TEMP, &test_dir_));
      test_dir_ = test_dir_.AppendASCII("CopyTreeWorkItemTest");

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
      logging::CloseLogFile();
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

  bool IsFileInUse(const FilePath& path) {
    if (!file_util::PathExists(path))
      return false;

    HANDLE handle = ::CreateFile(path.value().c_str(), FILE_ALL_ACCESS,
                                 NULL, NULL, OPEN_EXISTING, NULL, NULL);
    if (handle  == INVALID_HANDLE_VALUE)
      return true;

    CloseHandle(handle);
    return false;
  }

  // Simple function to read text from a file.
  std::wstring ReadTextFile(const std::wstring& filename) {
    WCHAR contents[64];
    std::wifstream file;
    file.open(filename.c_str());
    EXPECT_TRUE(file.is_open());
    file.getline(contents, 64);
    file.close();
    return std::wstring(contents);
  }

  wchar_t text_content_1[] = L"Gooooooooooooooooooooogle";
  wchar_t text_content_2[] = L"Overwrite Me";
};

// Copy one file from source to destination.
TEST_F(CopyTreeWorkItemTest, CopyFile) {
  // Create source file
  FilePath file_name_from(test_dir_);
  file_name_from = file_name_from.AppendASCII("File_From.txt");
  CreateTextFile(file_name_from.value(), text_content_1);
  ASSERT_TRUE(file_util::PathExists(file_name_from));

  // Create destination path
  FilePath dir_name_to(test_dir_);
  dir_name_to = dir_name_to.AppendASCII("Copy_To_Subdir");
  file_util::CreateDirectory(dir_name_to);
  ASSERT_TRUE(file_util::PathExists(dir_name_to));

  FilePath file_name_to(dir_name_to);
  file_name_to = file_name_to.AppendASCII("File_To.txt");

  // test Do()
  scoped_ptr<CopyTreeWorkItem> work_item(
      WorkItem::CreateCopyTreeWorkItem(file_name_from,
                                       file_name_to,
                                       temp_dir_,
                                       WorkItem::ALWAYS,
                                       FilePath()));

  EXPECT_TRUE(work_item->Do());

  EXPECT_TRUE(file_util::PathExists(file_name_from));
  EXPECT_TRUE(file_util::PathExists(file_name_to));
  EXPECT_TRUE(file_util::ContentsEqual(file_name_from, file_name_to));

  // test rollback()
  work_item->Rollback();

  EXPECT_FALSE(file_util::PathExists(file_name_to));
  EXPECT_TRUE(file_util::PathExists(file_name_from));
}

// Copy one file, overwriting the existing one in destination.
// Test with always_overwrite being true or false. The file is overwritten
// regardless since the content at destination file is different from source.
TEST_F(CopyTreeWorkItemTest, CopyFileOverwrite) {
  // Create source file
  FilePath file_name_from(test_dir_);
  file_name_from = file_name_from.AppendASCII("File_From.txt");
  CreateTextFile(file_name_from.value(), text_content_1);
  ASSERT_TRUE(file_util::PathExists(file_name_from));

  // Create destination file
  FilePath dir_name_to(test_dir_);
  dir_name_to = dir_name_to.AppendASCII("Copy_To_Subdir");
  file_util::CreateDirectory(dir_name_to);
  ASSERT_TRUE(file_util::PathExists(dir_name_to));

  FilePath file_name_to(dir_name_to);
  file_name_to = file_name_to.AppendASCII("File_To.txt");
  CreateTextFile(file_name_to.value(), text_content_2);
  ASSERT_TRUE(file_util::PathExists(file_name_to));

  // test Do() with always_overwrite being true.
  scoped_ptr<CopyTreeWorkItem> work_item(
      WorkItem::CreateCopyTreeWorkItem(file_name_from,
                                       file_name_to,
                                       temp_dir_,
                                       WorkItem::ALWAYS,
                                       FilePath()));

  EXPECT_TRUE(work_item->Do());

  EXPECT_TRUE(file_util::PathExists(file_name_from));
  EXPECT_TRUE(file_util::PathExists(file_name_to));
  EXPECT_EQ(0, ReadTextFile(file_name_from.value()).compare(text_content_1));
  EXPECT_EQ(0, ReadTextFile(file_name_to.value()).compare(text_content_1));

  // test rollback()
  work_item->Rollback();

  EXPECT_TRUE(file_util::PathExists(file_name_from));
  EXPECT_TRUE(file_util::PathExists(file_name_to));
  EXPECT_EQ(0, ReadTextFile(file_name_from.value()).compare(text_content_1));
  EXPECT_EQ(0, ReadTextFile(file_name_to.value()).compare(text_content_2));

  // test Do() with always_overwrite being false.
  // the file is still overwritten since the content is different.
  work_item.reset(
      WorkItem::CreateCopyTreeWorkItem(file_name_from,
                                       file_name_to,
                                       temp_dir_,
                                       WorkItem::IF_DIFFERENT,
                                       FilePath()));

  EXPECT_TRUE(work_item->Do());

  EXPECT_TRUE(file_util::PathExists(file_name_from));
  EXPECT_TRUE(file_util::PathExists(file_name_to));
  EXPECT_EQ(0, ReadTextFile(file_name_from.value()).compare(text_content_1));
  EXPECT_EQ(0, ReadTextFile(file_name_to.value()).compare(text_content_1));

  // test rollback()
  work_item->Rollback();

  EXPECT_TRUE(file_util::PathExists(file_name_from));
  EXPECT_TRUE(file_util::PathExists(file_name_to));
  EXPECT_EQ(0, ReadTextFile(file_name_from.value()).compare(text_content_1));
  EXPECT_EQ(0, ReadTextFile(file_name_to.value()).compare(text_content_2));
}

// Copy one file, with the existing one in destination having the same
// content.
// If always_overwrite being true, the file is overwritten.
// If always_overwrite being false, the file is unchanged.
TEST_F(CopyTreeWorkItemTest, CopyFileSameContent) {
  // Create source file
  FilePath file_name_from(test_dir_);
  file_name_from = file_name_from.AppendASCII("File_From.txt");
  CreateTextFile(file_name_from.value(), text_content_1);
  ASSERT_TRUE(file_util::PathExists(file_name_from));

  // Create destination file
  FilePath dir_name_to(test_dir_);
  dir_name_to = dir_name_to.AppendASCII("Copy_To_Subdir");
  file_util::CreateDirectory(dir_name_to);
  ASSERT_TRUE(file_util::PathExists(dir_name_to));

  FilePath file_name_to(dir_name_to);
  file_name_to = file_name_to.AppendASCII("File_To.txt");
  CreateTextFile(file_name_to.value(), text_content_1);
  ASSERT_TRUE(file_util::PathExists(file_name_to));

  // test Do() with always_overwrite being true.
  scoped_ptr<CopyTreeWorkItem> work_item(
      WorkItem::CreateCopyTreeWorkItem(file_name_from,
                                       file_name_to,
                                       temp_dir_,
                                       WorkItem::ALWAYS,
                                       FilePath()));

  EXPECT_TRUE(work_item->Do());

  // Get the path of backup file
  FilePath backup_file(work_item->backup_path_.path());
  EXPECT_FALSE(backup_file.empty());
  backup_file = backup_file.AppendASCII("File_To.txt");

  EXPECT_TRUE(file_util::PathExists(file_name_from));
  EXPECT_TRUE(file_util::PathExists(file_name_to));
  EXPECT_EQ(0, ReadTextFile(file_name_from.value()).compare(text_content_1));
  EXPECT_EQ(0, ReadTextFile(file_name_to.value()).compare(text_content_1));
  // we verify the file is overwritten by checking the existence of backup
  // file.
  EXPECT_TRUE(file_util::PathExists(backup_file));
  EXPECT_EQ(0, ReadTextFile(backup_file.value()).compare(text_content_1));

  // test rollback()
  work_item->Rollback();

  EXPECT_TRUE(file_util::PathExists(file_name_from));
  EXPECT_TRUE(file_util::PathExists(file_name_to));
  EXPECT_EQ(0, ReadTextFile(file_name_from.value()).compare(text_content_1));
  EXPECT_EQ(0, ReadTextFile(file_name_to.value()).compare(text_content_1));
  // the backup file should be gone after rollback
  EXPECT_FALSE(file_util::PathExists(backup_file));

  // test Do() with always_overwrite being false. nothing should change.
  work_item.reset(
      WorkItem::CreateCopyTreeWorkItem(file_name_from,
                                       file_name_to,
                                       temp_dir_,
                                       WorkItem::IF_DIFFERENT,
                                       FilePath()));

  EXPECT_TRUE(work_item->Do());

  EXPECT_TRUE(file_util::PathExists(file_name_from));
  EXPECT_TRUE(file_util::PathExists(file_name_to));
  EXPECT_EQ(0, ReadTextFile(file_name_from.value()).compare(text_content_1));
  EXPECT_EQ(0, ReadTextFile(file_name_to.value()).compare(text_content_1));
  // we verify the file is not overwritten by checking that the backup
  // file does not exist.
  EXPECT_FALSE(file_util::PathExists(backup_file));

  // test rollback(). nothing should happen here.
  work_item->Rollback();

  EXPECT_TRUE(file_util::PathExists(file_name_from));
  EXPECT_TRUE(file_util::PathExists(file_name_to));
  EXPECT_EQ(0, ReadTextFile(file_name_from.value()).compare(text_content_1));
  EXPECT_EQ(0, ReadTextFile(file_name_to.value()).compare(text_content_1));
  EXPECT_FALSE(file_util::PathExists(backup_file));
}

// Copy one file and without rollback. Verify all temporary files are deleted.
TEST_F(CopyTreeWorkItemTest, CopyFileAndCleanup) {
  // Create source file
  FilePath file_name_from(test_dir_);
  file_name_from = file_name_from.AppendASCII("File_From.txt");
  CreateTextFile(file_name_from.value(), text_content_1);
  ASSERT_TRUE(file_util::PathExists(file_name_from));

  // Create destination file
  FilePath dir_name_to(test_dir_);
  dir_name_to = dir_name_to.AppendASCII("Copy_To_Subdir");
  file_util::CreateDirectory(dir_name_to);
  ASSERT_TRUE(file_util::PathExists(dir_name_to));

  FilePath file_name_to(dir_name_to);
  file_name_to = file_name_to.AppendASCII("File_To.txt");
  CreateTextFile(file_name_to.value(), text_content_2);
  ASSERT_TRUE(file_util::PathExists(file_name_to));

  FilePath backup_file;

  {
    // test Do().
    scoped_ptr<CopyTreeWorkItem> work_item(
        WorkItem::CreateCopyTreeWorkItem(file_name_from,
                                         file_name_to,
                                         temp_dir_,
                                         WorkItem::IF_DIFFERENT,
                                         FilePath()));

    EXPECT_TRUE(work_item->Do());

    // Get the path of backup file
    backup_file = work_item->backup_path_.path();
    EXPECT_FALSE(backup_file.empty());
    backup_file = backup_file.AppendASCII("File_To.txt");

    EXPECT_TRUE(file_util::PathExists(file_name_from));
    EXPECT_TRUE(file_util::PathExists(file_name_to));
    EXPECT_EQ(0, ReadTextFile(file_name_from.value()).compare(text_content_1));
    EXPECT_EQ(0, ReadTextFile(file_name_to.value()).compare(text_content_1));
    // verify the file is moved to backup place.
    EXPECT_TRUE(file_util::PathExists(backup_file));
    EXPECT_EQ(0, ReadTextFile(backup_file.value()).compare(text_content_2));
  }

  // verify the backup file is cleaned up as well.
  EXPECT_FALSE(file_util::PathExists(backup_file));
}

// Copy one file, with the existing one in destination being used with
// overwrite option as IF_DIFFERENT. This destination-file-in-use should
// be moved to backup location after Do() and moved back after Rollback().
TEST_F(CopyTreeWorkItemTest, CopyFileInUse) {
  // Create source file
  FilePath file_name_from(test_dir_);
  file_name_from = file_name_from.AppendASCII("File_From");
  CreateTextFile(file_name_from.value(), text_content_1);
  ASSERT_TRUE(file_util::PathExists(file_name_from));

  // Create an executable in destination path by copying ourself to it.
  wchar_t exe_full_path_str[MAX_PATH];
  ::GetModuleFileName(NULL, exe_full_path_str, MAX_PATH);
  FilePath exe_full_path(exe_full_path_str);

  FilePath dir_name_to(test_dir_);
  dir_name_to = dir_name_to.AppendASCII("Copy_To_Subdir");
  file_util::CreateDirectory(dir_name_to);
  ASSERT_TRUE(file_util::PathExists(dir_name_to));

  FilePath file_name_to(dir_name_to);
  file_name_to = file_name_to.AppendASCII("File_To");
  file_util::CopyFile(exe_full_path, file_name_to);
  ASSERT_TRUE(file_util::PathExists(file_name_to));

  VLOG(1) << "copy ourself from " << exe_full_path.value()
          << " to " << file_name_to.value();

  // Run the executable in destination path
  STARTUPINFOW si = {sizeof(si)};
  PROCESS_INFORMATION pi = {0};
  ASSERT_TRUE(
      ::CreateProcess(NULL, const_cast<wchar_t*>(file_name_to.value().c_str()),
                       NULL, NULL, FALSE, CREATE_NO_WINDOW | CREATE_SUSPENDED,
                       NULL, NULL, &si, &pi));

  // test Do().
  scoped_ptr<CopyTreeWorkItem> work_item(
      WorkItem::CreateCopyTreeWorkItem(file_name_from,
                                       file_name_to,
                                       temp_dir_,
                                       WorkItem::IF_DIFFERENT,
                                       FilePath()));

  EXPECT_TRUE(work_item->Do());

  // Get the path of backup file
  FilePath backup_file(work_item->backup_path_.path());
  EXPECT_FALSE(backup_file.empty());
  backup_file = backup_file.AppendASCII("File_To");

  EXPECT_TRUE(file_util::PathExists(file_name_from));
  EXPECT_TRUE(file_util::PathExists(file_name_to));
  EXPECT_EQ(0, ReadTextFile(file_name_from.value()).compare(text_content_1));
  EXPECT_EQ(0, ReadTextFile(file_name_to.value()).compare(text_content_1));
  // verify the file in used is moved to backup place.
  EXPECT_TRUE(file_util::PathExists(backup_file));
  EXPECT_TRUE(file_util::ContentsEqual(exe_full_path, backup_file));

  // test rollback()
  work_item->Rollback();

  EXPECT_TRUE(file_util::PathExists(file_name_from));
  EXPECT_TRUE(file_util::PathExists(file_name_to));
  EXPECT_EQ(0, ReadTextFile(file_name_from.value()).compare(text_content_1));
  EXPECT_TRUE(file_util::ContentsEqual(exe_full_path, file_name_to));
  // the backup file should be gone after rollback
  EXPECT_FALSE(file_util::PathExists(backup_file));

  TerminateProcess(pi.hProcess, 0);
  // make sure the handle is closed.
  EXPECT_TRUE(WaitForSingleObject(pi.hProcess, 10000) == WAIT_OBJECT_0);
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);
}

// Test overwrite option NEW_NAME_IF_IN_USE:
// 1. If destination file is in use, the source should be copied with the
//    new name after Do() and this new name file should be deleted
//    after rollback.
// 2. If destination file is not in use, the source should be copied in the
//    destination folder after Do() and should be rolled back after Rollback().
TEST_F(CopyTreeWorkItemTest, NewNameAndCopyTest) {
  // Create source file
  FilePath file_name_from(test_dir_);
  file_name_from = file_name_from.AppendASCII("File_From");
  CreateTextFile(file_name_from.value(), text_content_1);
  ASSERT_TRUE(file_util::PathExists(file_name_from));

  // Create an executable in destination path by copying ourself to it.
  wchar_t exe_full_path_str[MAX_PATH];
  ::GetModuleFileName(NULL, exe_full_path_str, MAX_PATH);
  FilePath exe_full_path(exe_full_path_str);

  FilePath dir_name_to(test_dir_);
  dir_name_to = dir_name_to.AppendASCII("Copy_To_Subdir");
  file_util::CreateDirectory(dir_name_to);
  ASSERT_TRUE(file_util::PathExists(dir_name_to));

  FilePath file_name_to(dir_name_to), alternate_to(dir_name_to);
  file_name_to = file_name_to.AppendASCII("File_To");
  alternate_to = alternate_to.AppendASCII("Alternate_To");
  file_util::CopyFile(exe_full_path, file_name_to);
  ASSERT_TRUE(file_util::PathExists(file_name_to));

  VLOG(1) << "copy ourself from " << exe_full_path.value()
          << " to " << file_name_to.value();

  // Run the executable in destination path
  STARTUPINFOW si = {sizeof(si)};
  PROCESS_INFORMATION pi = {0};
  ASSERT_TRUE(
      ::CreateProcess(NULL, const_cast<wchar_t*>(file_name_to.value().c_str()),
                       NULL, NULL, FALSE, CREATE_NO_WINDOW | CREATE_SUSPENDED,
                       NULL, NULL, &si, &pi));

  // test Do().
  scoped_ptr<CopyTreeWorkItem> work_item(
      WorkItem::CreateCopyTreeWorkItem(file_name_from,
                                       file_name_to,
                                       temp_dir_,
                                       WorkItem::NEW_NAME_IF_IN_USE,
                                       alternate_to));

  EXPECT_TRUE(work_item->Do());

  EXPECT_TRUE(file_util::PathExists(file_name_from));
  EXPECT_TRUE(file_util::PathExists(file_name_to));
  EXPECT_EQ(0, ReadTextFile(file_name_from.value()).compare(text_content_1));
  EXPECT_TRUE(file_util::ContentsEqual(exe_full_path, file_name_to));
  // verify that the backup path does not exist
  EXPECT_TRUE(work_item->backup_path_.path().empty());
  EXPECT_TRUE(file_util::ContentsEqual(file_name_from, alternate_to));

  // test rollback()
  work_item->Rollback();

  EXPECT_TRUE(file_util::PathExists(file_name_from));
  EXPECT_TRUE(file_util::PathExists(file_name_to));
  EXPECT_EQ(0, ReadTextFile(file_name_from.value()).compare(text_content_1));
  EXPECT_TRUE(file_util::ContentsEqual(exe_full_path, file_name_to));
  EXPECT_TRUE(work_item->backup_path_.path().empty());
  // the alternate file should be gone after rollback
  EXPECT_FALSE(file_util::PathExists(alternate_to));

  TerminateProcess(pi.hProcess, 0);
  // make sure the handle is closed.
  EXPECT_TRUE(WaitForSingleObject(pi.hProcess, 10000) == WAIT_OBJECT_0);
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);

  // Now the process has terminated, lets try overwriting the file again
  work_item.reset(WorkItem::CreateCopyTreeWorkItem(
      file_name_from, file_name_to,
      temp_dir_, WorkItem::NEW_NAME_IF_IN_USE,
      alternate_to));
  if (IsFileInUse(file_name_to))
    base::PlatformThread::Sleep(2000);
  // If file is still in use, the rest of the test will fail.
  ASSERT_FALSE(IsFileInUse(file_name_to));
  EXPECT_TRUE(work_item->Do());

  // Get the path of backup file
  FilePath backup_file(work_item->backup_path_.path());
  EXPECT_FALSE(backup_file.empty());
  backup_file = backup_file.AppendASCII("File_To");

  EXPECT_TRUE(file_util::PathExists(file_name_from));
  EXPECT_TRUE(file_util::PathExists(file_name_to));
  EXPECT_EQ(0, ReadTextFile(file_name_from.value()).compare(text_content_1));
  EXPECT_TRUE(file_util::ContentsEqual(file_name_from, file_name_to));
  // verify that the backup path does exist
  EXPECT_TRUE(file_util::PathExists(backup_file));
  EXPECT_FALSE(file_util::PathExists(alternate_to));

  // test rollback()
  work_item->Rollback();

  EXPECT_TRUE(file_util::PathExists(file_name_from));
  EXPECT_TRUE(file_util::PathExists(file_name_to));
  EXPECT_EQ(0, ReadTextFile(file_name_from.value()).compare(text_content_1));
  EXPECT_TRUE(file_util::ContentsEqual(exe_full_path, file_name_to));
  // the backup file should be gone after rollback
  EXPECT_FALSE(file_util::PathExists(backup_file));
  EXPECT_FALSE(file_util::PathExists(alternate_to));
}

// Test overwrite option IF_NOT_PRESENT:
// 1. If destination file/directory exist, the source should not be copied
// 2. If destination file/directory do not exist, the source should be copied
//    in the destination folder after Do() and should be rolled back after
//    Rollback().
// Flaky, http://crbug.com/59785.
TEST_F(CopyTreeWorkItemTest, FLAKY_IfNotPresentTest) {
  // Create source file
  FilePath file_name_from(test_dir_);
  file_name_from = file_name_from.AppendASCII("File_From");
  CreateTextFile(file_name_from.value(), text_content_1);
  ASSERT_TRUE(file_util::PathExists(file_name_from));

  // Create an executable in destination path by copying ourself to it.
  wchar_t exe_full_path_str[MAX_PATH];
  ::GetModuleFileName(NULL, exe_full_path_str, MAX_PATH);
  FilePath exe_full_path(exe_full_path_str);

  FilePath dir_name_to(test_dir_);
  dir_name_to = dir_name_to.AppendASCII("Copy_To_Subdir");
  file_util::CreateDirectory(dir_name_to);
  ASSERT_TRUE(file_util::PathExists(dir_name_to));
  FilePath file_name_to(dir_name_to);
  file_name_to = file_name_to.AppendASCII("File_To");
  file_util::CopyFile(exe_full_path, file_name_to);
  ASSERT_TRUE(file_util::PathExists(file_name_to));

  // Get the path of backup file
  FilePath backup_file(temp_dir_);
  backup_file = backup_file.AppendASCII("File_To");

  // test Do().
  scoped_ptr<CopyTreeWorkItem> work_item(
      WorkItem::CreateCopyTreeWorkItem(
          file_name_from,
          file_name_to, temp_dir_,
          WorkItem::IF_NOT_PRESENT,
          FilePath()));
  EXPECT_TRUE(work_item->Do());

  // verify that the source, destination have not changed and backup path
  // does not exist
  EXPECT_TRUE(file_util::PathExists(file_name_from));
  EXPECT_TRUE(file_util::PathExists(file_name_to));
  EXPECT_EQ(0, ReadTextFile(file_name_from.value()).compare(text_content_1));
  EXPECT_TRUE(file_util::ContentsEqual(exe_full_path, file_name_to));
  EXPECT_FALSE(file_util::PathExists(backup_file));

  // test rollback()
  work_item->Rollback();

  // verify that the source, destination have not changed and backup path
  // does not exist after rollback also
  EXPECT_TRUE(file_util::PathExists(file_name_from));
  EXPECT_TRUE(file_util::PathExists(file_name_to));
  EXPECT_EQ(0, ReadTextFile(file_name_from.value()).compare(text_content_1));
  EXPECT_TRUE(file_util::ContentsEqual(exe_full_path, file_name_to));
  EXPECT_FALSE(file_util::PathExists(backup_file));

  // Now delete the destination and try copying the file again.
  file_util::Delete(file_name_to, true);
  work_item.reset(WorkItem::CreateCopyTreeWorkItem(
      file_name_from, file_name_to,
      temp_dir_, WorkItem::IF_NOT_PRESENT,
      FilePath()));
  EXPECT_TRUE(work_item->Do());

  // verify that the source, destination are the same and backup path
  // does not exist
  EXPECT_TRUE(file_util::PathExists(file_name_from));
  EXPECT_TRUE(file_util::PathExists(file_name_to));
  EXPECT_EQ(0, ReadTextFile(file_name_from.value()).compare(text_content_1));
  EXPECT_EQ(0, ReadTextFile(file_name_to.value()).compare(text_content_1));
  EXPECT_FALSE(file_util::PathExists(backup_file));

  // test rollback()
  work_item->Rollback();

  // verify that the destination does not exist anymore
  EXPECT_TRUE(file_util::PathExists(file_name_from));
  EXPECT_FALSE(file_util::PathExists(file_name_to));
  EXPECT_EQ(0, ReadTextFile(file_name_from.value()).compare(text_content_1));
  EXPECT_FALSE(file_util::PathExists(backup_file));
}

// Copy one file without rollback. The existing one in destination is in use.
// Verify it is moved to backup location and stays there.
// Flaky, http://crbug.com/59783.
TEST_F(CopyTreeWorkItemTest, FLAKY_CopyFileInUseAndCleanup) {
  // Create source file
  FilePath file_name_from(test_dir_);
  file_name_from = file_name_from.AppendASCII("File_From");
  CreateTextFile(file_name_from.value(), text_content_1);
  ASSERT_TRUE(file_util::PathExists(file_name_from));

  // Create an executable in destination path by copying ourself to it.
  wchar_t exe_full_path_str[MAX_PATH];
  ::GetModuleFileName(NULL, exe_full_path_str, MAX_PATH);
  FilePath exe_full_path(exe_full_path_str);

  FilePath dir_name_to(test_dir_);
  dir_name_to = dir_name_to.AppendASCII("Copy_To_Subdir");
  file_util::CreateDirectory(dir_name_to);
  ASSERT_TRUE(file_util::PathExists(dir_name_to));

  FilePath file_name_to(dir_name_to);
  file_name_to = file_name_to.AppendASCII("File_To");
  file_util::CopyFile(exe_full_path, file_name_to);
  ASSERT_TRUE(file_util::PathExists(file_name_to));

  VLOG(1) << "copy ourself from " << exe_full_path.value()
          << " to " << file_name_to.value();

  // Run the executable in destination path
  STARTUPINFOW si = {sizeof(si)};
  PROCESS_INFORMATION pi = {0};
  ASSERT_TRUE(
      ::CreateProcess(NULL, const_cast<wchar_t*>(file_name_to.value().c_str()),
                       NULL, NULL, FALSE, CREATE_NO_WINDOW | CREATE_SUSPENDED,
                       NULL, NULL, &si, &pi));

  FilePath backup_file;

  // test Do().
  {
    scoped_ptr<CopyTreeWorkItem> work_item(
        WorkItem::CreateCopyTreeWorkItem(file_name_from,
                                         file_name_to,
                                         temp_dir_,
                                         WorkItem::IF_DIFFERENT,
                                         FilePath()));

    EXPECT_TRUE(work_item->Do());

    // Get the path of backup file
    backup_file = work_item->backup_path_.path();
    EXPECT_FALSE(backup_file.empty());
    backup_file = backup_file.AppendASCII("File_To");

    EXPECT_TRUE(file_util::PathExists(file_name_from));
    EXPECT_TRUE(file_util::PathExists(file_name_to));
    EXPECT_EQ(0, ReadTextFile(file_name_from.value()).compare(text_content_1));
    EXPECT_EQ(0, ReadTextFile(file_name_to.value()).compare(text_content_1));
    // verify the file in used is moved to backup place.
    EXPECT_TRUE(file_util::PathExists(backup_file));
    EXPECT_TRUE(file_util::ContentsEqual(exe_full_path, backup_file));
  }

  // verify the file in used should be still at the backup place.
  EXPECT_TRUE(file_util::PathExists(backup_file));
  EXPECT_TRUE(file_util::ContentsEqual(exe_full_path, backup_file));

  TerminateProcess(pi.hProcess, 0);
  // make sure the handle is closed.
  EXPECT_TRUE(WaitForSingleObject(pi.hProcess, 10000) == WAIT_OBJECT_0);
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);
}

// Copy a tree from source to destination.
// Flaky, http://crbug.com/59784.
TEST_F(CopyTreeWorkItemTest, FLAKY_CopyTree) {
  // Create source tree
  FilePath dir_name_from(test_dir_);
  dir_name_from = dir_name_from.AppendASCII("from");
  file_util::CreateDirectory(dir_name_from);
  ASSERT_TRUE(file_util::PathExists(dir_name_from));

  FilePath dir_name_from_1(dir_name_from);
  dir_name_from_1 = dir_name_from_1.AppendASCII("1");
  file_util::CreateDirectory(dir_name_from_1);
  ASSERT_TRUE(file_util::PathExists(dir_name_from_1));

  FilePath dir_name_from_2(dir_name_from);
  dir_name_from_2 = dir_name_from_2.AppendASCII("2");
  file_util::CreateDirectory(dir_name_from_2);
  ASSERT_TRUE(file_util::PathExists(dir_name_from_2));

  FilePath file_name_from_1(dir_name_from_1);
  file_name_from_1 = file_name_from_1.AppendASCII("File_1.txt");
  CreateTextFile(file_name_from_1.value(), text_content_1);
  ASSERT_TRUE(file_util::PathExists(file_name_from_1));

  FilePath file_name_from_2(dir_name_from_2);
  file_name_from_2 = file_name_from_2.AppendASCII("File_2.txt");
  CreateTextFile(file_name_from_2.value(), text_content_1);
  ASSERT_TRUE(file_util::PathExists(file_name_from_2));

  FilePath dir_name_to(test_dir_);
  dir_name_to = dir_name_to.AppendASCII("to");

  // test Do()
  {
    scoped_ptr<CopyTreeWorkItem> work_item(
        WorkItem::CreateCopyTreeWorkItem(dir_name_from,
                                         dir_name_to,
                                         temp_dir_,
                                         WorkItem::ALWAYS,
                                         FilePath()));

    EXPECT_TRUE(work_item->Do());
  }

  FilePath file_name_to_1(dir_name_to);
  file_name_to_1 = file_name_to_1.AppendASCII("1");
  file_name_to_1 = file_name_to_1.AppendASCII("File_1.txt");
  EXPECT_TRUE(file_util::PathExists(file_name_to_1));
  VLOG(1) << "compare " << file_name_from_1.value()
          << " and " << file_name_to_1.value();
  EXPECT_TRUE(file_util::ContentsEqual(file_name_from_1, file_name_to_1));

  FilePath file_name_to_2(dir_name_to);
  file_name_to_2 = file_name_to_2.AppendASCII("2");
  file_name_to_2 = file_name_to_2.AppendASCII("File_2.txt");
  EXPECT_TRUE(file_util::PathExists(file_name_to_2));
  VLOG(1) << "compare " << file_name_from_2.value()
          << " and " << file_name_to_2.value();
  EXPECT_TRUE(file_util::ContentsEqual(file_name_from_2, file_name_to_2));
}
