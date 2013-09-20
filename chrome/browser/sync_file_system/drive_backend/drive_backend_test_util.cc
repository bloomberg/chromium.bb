// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/drive_backend_test_util.h"

#include <set>

#include "chrome/browser/drive/drive_api_util.h"
#include "chrome/browser/google_apis/drive_api_parser.h"
#include "chrome/browser/google_apis/drive_entry_kinds.h"
#include "chrome/browser/google_apis/gdata_wapi_parser.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_file_system {
namespace drive_backend {
namespace test_util {

void ExpectEquivalentServiceMetadata(const ServiceMetadata& left,
                                     const ServiceMetadata& right) {
  EXPECT_EQ(left.largest_change_id(), right.largest_change_id());
  EXPECT_EQ(left.sync_root_tracker_id(), right.sync_root_tracker_id());
  EXPECT_EQ(left.next_tracker_id(), right.next_tracker_id());
}

void ExpectEquivalentDetails(const FileDetails& left,
                             const FileDetails& right) {
  std::set<std::string> parents;
  for (int i = 0; i < left.parent_folder_ids_size(); ++i)
    EXPECT_TRUE(parents.insert(left.parent_folder_ids(i)).second);

  for (int i = 0; i < right.parent_folder_ids_size(); ++i)
    EXPECT_EQ(1u, parents.erase(left.parent_folder_ids(i)));
  EXPECT_TRUE(parents.empty());

  EXPECT_EQ(left.title(), right.title());
  EXPECT_EQ(left.file_kind(), right.file_kind());
  EXPECT_EQ(left.md5(), right.md5());
  EXPECT_EQ(left.etag(), right.etag());
  EXPECT_EQ(left.creation_time(), right.creation_time());
  EXPECT_EQ(left.modification_time(), right.modification_time());
  EXPECT_EQ(left.deleted(), right.deleted());
  EXPECT_EQ(left.change_id(), right.change_id());
}

void ExpectEquivalentMetadata(const FileMetadata& left,
                              const FileMetadata& right) {
  EXPECT_EQ(left.file_id(), right.file_id());
  ExpectEquivalentDetails(left.details(), right.details());
}

void ExpectEquivalentTrackers(const FileTracker& left,
                              const FileTracker& right) {
  EXPECT_EQ(left.tracker_id(), right.tracker_id());
  EXPECT_EQ(left.parent_tracker_id(), right.parent_tracker_id());
  EXPECT_EQ(left.file_id(), right.file_id());
  EXPECT_EQ(left.app_id(), right.app_id());
  EXPECT_EQ(left.tracker_kind(), right.tracker_kind());
  ExpectEquivalentDetails(left.synced_details(), right.synced_details());
  EXPECT_EQ(left.dirty(), right.dirty());
  EXPECT_EQ(left.active(), right.active());
  EXPECT_EQ(left.needs_folder_listing(), right.needs_folder_listing());
}

void ExpectEquivalentResourceAndMetadata(
    const google_apis::FileResource& resource,
    const FileMetadata& metadata) {
  EXPECT_EQ(resource.file_id(), metadata.file_id());
  const FileDetails& details = metadata.details();
  EXPECT_EQ(resource.title(), details.title());

  google_apis::DriveEntryKind resource_kind = drive::util::GetKind(resource);
  switch (details.file_kind()) {
    case FILE_KIND_UNSUPPORTED:
      EXPECT_NE(google_apis::ENTRY_KIND_FILE, resource_kind);
      EXPECT_NE(google_apis::ENTRY_KIND_FOLDER, resource_kind);
      break;
    case FILE_KIND_FILE:
      EXPECT_EQ(google_apis::ENTRY_KIND_FILE, resource_kind);
      break;
    case FILE_KIND_FOLDER:
      EXPECT_EQ(google_apis::ENTRY_KIND_FOLDER, resource_kind);
      break;
  }

  EXPECT_EQ(resource.md5_checksum(), details.md5());
  EXPECT_EQ(resource.created_date(),
            base::Time::FromInternalValue(details.creation_time()));
  EXPECT_EQ(resource.modified_date(),
            base::Time::FromInternalValue(details.modification_time()));
  EXPECT_EQ(resource.labels().is_trashed(), details.deleted());
}

void ExpectEquivalentMetadataAndTracker(const FileMetadata& metadata,
                                        const FileTracker& tracker) {
  EXPECT_EQ(metadata.file_id(), tracker.file_id());
  if (!tracker.dirty())
    ExpectEquivalentDetails(metadata.details(), tracker.synced_details());
}

}  // namespace test_util
}  // namespace drive_backend
}  // namespace sync_file_system
