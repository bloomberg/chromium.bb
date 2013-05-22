// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/file_system/create_directory_operation.h"

#include "base/files/scoped_temp_dir.h"
#include "base/message_loop.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/drive/resource_metadata.h"
#include "chrome/browser/chromeos/drive/test_util.h"
#include "chrome/browser/google_apis/test_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drive {
namespace file_system {
namespace {

// Creates a resource entry instance. The resource id is "resource_id:{title}",
// where {title} is the given |title|.
ResourceEntry CreateResourceEntry(const std::string& title,
                                  const std::string& parent_resource_id,
                                  bool is_directory) {
  ResourceEntry entry;
  const std::string resource_id = "resource_id:" + title;
  entry.set_title(title);
  entry.set_resource_id(resource_id);
  entry.set_parent_resource_id(parent_resource_id);

  PlatformFileInfoProto* file_info = entry.mutable_file_info();
  file_info->set_is_directory(is_directory);

  return entry;
}

}  // namespace

TEST(CreateDirectoryOperationTest, GetExistingDeepestDirectory) {
  MessageLoopForUI message_loop;
  content::TestBrowserThread ui_thread(
      content::BrowserThread::UI, &message_loop);

  // Set up a Metadata instance.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  scoped_ptr<internal::ResourceMetadata, test_util::DestroyHelperForTests>
      metadata(new internal::ResourceMetadata(
          temp_dir.path(), message_loop.message_loop_proxy()));
  FileError error = FILE_ERROR_FAILED;
  metadata->Initialize(
      google_apis::test_util::CreateCopyResultCallback(&error));
  google_apis::test_util::RunBlockingPoolTask();
  ASSERT_EQ(FILE_ERROR_OK, error);

  // Create a testee directory tree.
  // drive/root/dir1/
  // drive/root/dir1/dir2/
  // drive/root/dir1/dir2/file3
  // drive/root/dir1/file4
  // drive/root/dir5/
  const char kTestRootResourceId[] = "test_root";
  ASSERT_EQ(FILE_ERROR_OK, metadata->AddEntry(
      util::CreateMyDriveRootEntry(kTestRootResourceId)));
  ASSERT_EQ(FILE_ERROR_OK, metadata->AddEntry(
      CreateResourceEntry("dir1", kTestRootResourceId, true)));
  ASSERT_EQ(FILE_ERROR_OK, metadata->AddEntry(
      CreateResourceEntry("dir2", "resource_id:dir1", true)));
  ASSERT_EQ(FILE_ERROR_OK, metadata->AddEntry(
      CreateResourceEntry("file3", "resource_id:dir2", false)));
  ASSERT_EQ(FILE_ERROR_OK, metadata->AddEntry(
      CreateResourceEntry("file4", "resource_id:dir1", false)));
  ASSERT_EQ(FILE_ERROR_OK, metadata->AddEntry(
      CreateResourceEntry("dir5", kTestRootResourceId, true)));

  ResourceEntry entry;
  // Searching grand root and mydrive root.
  EXPECT_EQ(
      FILE_PATH_LITERAL("drive"),
      CreateDirectoryOperation::GetExistingDeepestDirectory(
          metadata.get(), base::FilePath(FILE_PATH_LITERAL("drive")),
          &entry).value());
  EXPECT_EQ(util::kDriveGrandRootSpecialResourceId, entry.resource_id());
  EXPECT_EQ(
      FILE_PATH_LITERAL("drive/root"),
      CreateDirectoryOperation::GetExistingDeepestDirectory(
          metadata.get(), base::FilePath(FILE_PATH_LITERAL("drive/root")),
          &entry).value());
  EXPECT_EQ(kTestRootResourceId, entry.resource_id());

  // Searching existing directories.
  EXPECT_EQ(
      FILE_PATH_LITERAL("drive/root/dir1"),
      CreateDirectoryOperation::GetExistingDeepestDirectory(
          metadata.get(), base::FilePath(FILE_PATH_LITERAL("drive/root/dir1")),
          &entry).value());
  EXPECT_EQ("resource_id:dir1", entry.resource_id());
  EXPECT_EQ(
      FILE_PATH_LITERAL("drive/root/dir1/dir2"),
      CreateDirectoryOperation::GetExistingDeepestDirectory(
          metadata.get(),
          base::FilePath(FILE_PATH_LITERAL("drive/root/dir1/dir2")),
          &entry).value());
  EXPECT_EQ("resource_id:dir2", entry.resource_id());

  // Searching missing directories.
  EXPECT_EQ(
      FILE_PATH_LITERAL("drive/root/dir1/dir2"),
      CreateDirectoryOperation::GetExistingDeepestDirectory(
          metadata.get(),
          base::FilePath(FILE_PATH_LITERAL("drive/root/dir1/dir2/dir3")),
          &entry).value());
  EXPECT_EQ("resource_id:dir2", entry.resource_id());
  EXPECT_EQ(
      FILE_PATH_LITERAL("drive/root/dir1/dir2"),
      CreateDirectoryOperation::GetExistingDeepestDirectory(
          metadata.get(),
          base::FilePath(FILE_PATH_LITERAL("drive/root/dir1/dir2/dir3/dir4")),
          &entry).value());
  EXPECT_EQ("resource_id:dir2", entry.resource_id());
  EXPECT_EQ(
      FILE_PATH_LITERAL("drive/root/dir1"),
      CreateDirectoryOperation::GetExistingDeepestDirectory(
          metadata.get(),
          base::FilePath(FILE_PATH_LITERAL("drive/root/dir1/dir2-1/dir3")),
          &entry).value());
  EXPECT_EQ("resource_id:dir1", entry.resource_id());
  EXPECT_EQ(
      FILE_PATH_LITERAL("drive/root/dir5"),
      CreateDirectoryOperation::GetExistingDeepestDirectory(
          metadata.get(),
          base::FilePath(FILE_PATH_LITERAL("drive/root/dir5/dir6/dir7/dir8")),
          &entry).value());
  EXPECT_EQ("resource_id:dir5", entry.resource_id());

  // If the path points to a file (not a directory), failed to find.
  EXPECT_EQ(
      FILE_PATH_LITERAL(""),
      CreateDirectoryOperation::GetExistingDeepestDirectory(
          metadata.get(),
          base::FilePath(FILE_PATH_LITERAL("drive/root/dir1/dir2/file3")),
          &entry).value());
  EXPECT_EQ(
      FILE_PATH_LITERAL(""),
      CreateDirectoryOperation::GetExistingDeepestDirectory(
          metadata.get(),
          base::FilePath(FILE_PATH_LITERAL("drive/root/dir1/file4")),
          &entry).value());

  // Searching missing directory under a "file" should be also failed.
  EXPECT_EQ(
      FILE_PATH_LITERAL(""),
      CreateDirectoryOperation::GetExistingDeepestDirectory(
          metadata.get(),
          base::FilePath(FILE_PATH_LITERAL("drive/root/dir1/file4/dir5")),
          &entry).value());

  // Searching path not starting "drive" should be also failed.
  EXPECT_EQ(
      FILE_PATH_LITERAL(""),
      CreateDirectoryOperation::GetExistingDeepestDirectory(
          metadata.get(),
          base::FilePath(FILE_PATH_LITERAL("123456")),
          &entry).value());
  EXPECT_EQ(
      FILE_PATH_LITERAL(""),
      CreateDirectoryOperation::GetExistingDeepestDirectory(
          metadata.get(),
          base::FilePath(FILE_PATH_LITERAL("foo/bar")),
          &entry).value());
}

}  // namespace file_system
}  // namespace drive
