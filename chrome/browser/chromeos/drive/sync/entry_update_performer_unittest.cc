// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/sync/entry_update_performer.h"

#include "base/task_runner_util.h"
#include "chrome/browser/chromeos/drive/file_system/operation_test_base.h"
#include "chrome/browser/chromeos/drive/resource_metadata.h"
#include "chrome/browser/drive/drive_api_util.h"
#include "chrome/browser/drive/fake_drive_service.h"
#include "google_apis/drive/gdata_wapi_parser.h"
#include "google_apis/drive/test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drive {
namespace internal {

class EntryUpdatePerformerTest : public file_system::OperationTestBase {
 protected:
  virtual void SetUp() OVERRIDE {
   OperationTestBase::SetUp();
   performer_.reset(new EntryUpdatePerformer(blocking_task_runner(),
                                             observer(),
                                             scheduler(),
                                             metadata()));
  }

  scoped_ptr<EntryUpdatePerformer> performer_;
};

TEST_F(EntryUpdatePerformerTest, UpdateEntry) {
  base::FilePath src_path(
      FILE_PATH_LITERAL("drive/root/Directory 1/SubDirectory File 1.txt"));
  base::FilePath dest_path(
      FILE_PATH_LITERAL("drive/root/Directory 1/Sub Directory Folder"));

  ResourceEntry src_entry, dest_entry;
  EXPECT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(src_path, &src_entry));
  EXPECT_EQ(FILE_ERROR_OK, GetLocalResourceEntry(dest_path, &dest_entry));

  // Update local entry.
  base::Time new_last_modified = base::Time::FromInternalValue(
      src_entry.file_info().last_modified()) + base::TimeDelta::FromSeconds(1);
  base::Time new_last_accessed = base::Time::FromInternalValue(
      src_entry.file_info().last_accessed()) + base::TimeDelta::FromSeconds(2);

  src_entry.set_parent_local_id(dest_entry.local_id());
  src_entry.set_title("Moved" + src_entry.title());
  src_entry.mutable_file_info()->set_last_modified(
      new_last_modified.ToInternalValue());
  src_entry.mutable_file_info()->set_last_accessed(
      new_last_accessed.ToInternalValue());
  src_entry.set_metadata_edit_state(ResourceEntry::DIRTY);

  FileError error = FILE_ERROR_FAILED;
  base::PostTaskAndReplyWithResult(
      blocking_task_runner(),
      FROM_HERE,
      base::Bind(&ResourceMetadata::RefreshEntry,
                 base::Unretained(metadata()),
                 src_entry),
      google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);

  // Perform server side update.
  error = FILE_ERROR_FAILED;
  performer_->UpdateEntry(
      src_entry.local_id(),
      google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);

  // Verify the file is updated on the server.
  google_apis::GDataErrorCode gdata_error = google_apis::GDATA_OTHER_ERROR;
  scoped_ptr<google_apis::ResourceEntry> gdata_entry;
  fake_service()->GetResourceEntry(
      src_entry.resource_id(),
      google_apis::test_util::CreateCopyResultCallback(&gdata_error,
                                                       &gdata_entry));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(google_apis::HTTP_SUCCESS, gdata_error);
  ASSERT_TRUE(gdata_entry);

  EXPECT_EQ(src_entry.title(), gdata_entry->title());
  EXPECT_EQ(new_last_modified, gdata_entry->updated_time());
  EXPECT_EQ(new_last_accessed, gdata_entry->last_viewed_time());

  const google_apis::Link* parent_link =
      gdata_entry->GetLinkByType(google_apis::Link::LINK_PARENT);
  ASSERT_TRUE(parent_link);
  EXPECT_EQ(dest_entry.resource_id(),
            util::ExtractResourceIdFromUrl(parent_link->href()));
}

TEST_F(EntryUpdatePerformerTest, UpdateEntry_NotFound) {
  const std::string id = "this ID should result in NOT_FOUND";
  FileError error = FILE_ERROR_FAILED;
  performer_->UpdateEntry(
      id, google_apis::test_util::CreateCopyResultCallback(&error));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, error);
}

}  // namespace internal
}  // namespace drive
