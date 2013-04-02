// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/resource_entry_conversion.h"

#include "base/files/file_path.h"
#include "base/values.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/drive_test_util.h"
#include "chrome/browser/google_apis/gdata_wapi_parser.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drive {

TEST(ResourceEntryConversionTest, ConvertResourceEntryToDriveEntryProto_File) {
  scoped_ptr<base::Value> value =
      google_apis::test_util::LoadJSONFile("chromeos/gdata/file_entry.json");
  ASSERT_TRUE(value.get());

  scoped_ptr<google_apis::ResourceEntry> resource_entry(
      google_apis::ResourceEntry::ExtractAndParse(*value));
  ASSERT_TRUE(resource_entry.get());

  DriveEntryProto entry_proto =
      ConvertResourceEntryToDriveEntryProto(*resource_entry);

  EXPECT_EQ("File 1.mp3",  entry_proto.title());
  EXPECT_EQ("File 1.mp3",  entry_proto.base_name());
  EXPECT_EQ("file:2_file_resource_id",  entry_proto.resource_id());
  EXPECT_EQ("https://file_content_url/",  entry_proto.download_url());
  EXPECT_EQ("https://file1_link_self/file:2_file_resource_id",
            entry_proto.edit_url());
  EXPECT_EQ("",  entry_proto.parent_resource_id());

  EXPECT_FALSE(entry_proto.deleted());
  EXPECT_FALSE(entry_proto.shared_with_me());

  base::Time expected_creation_time;
  base::Time expected_modified_time;

  {
    // 2011-12-14T00:40:47.330Z
    base::Time::Exploded exploded;
    exploded.year = 2011;
    exploded.month = 12;
    exploded.day_of_month = 13;
    exploded.day_of_week = 2;  // Tuesday
    exploded.hour = 0;
    exploded.minute = 40;
    exploded.second = 47;
    exploded.millisecond = 330;
    expected_creation_time = base::Time::FromUTCExploded(exploded);
  }

  {
    // 2011-12-13T00:40:47.330Z
    base::Time::Exploded exploded;
    exploded.year = 2011;
    exploded.month = 12;
    exploded.day_of_month = 14;
    exploded.day_of_week = 3;  // Wednesday
    exploded.hour = 0;
    exploded.minute = 40;
    exploded.second = 47;
    exploded.millisecond = 330;
    expected_modified_time = base::Time::FromUTCExploded(exploded);
  }

  EXPECT_EQ(expected_modified_time.ToInternalValue(),
            entry_proto.file_info().last_modified());
  // Last accessed value equal to 0 means that the file has never been viewed.
  EXPECT_EQ(0, entry_proto.file_info().last_accessed());
  EXPECT_EQ(expected_creation_time.ToInternalValue(),
            entry_proto.file_info().creation_time());

  EXPECT_EQ("audio/mpeg",
            entry_proto.file_specific_info().content_mime_type());
  EXPECT_FALSE(entry_proto.file_specific_info().is_hosted_document());
  EXPECT_EQ("",
            entry_proto.file_specific_info().thumbnail_url());
  EXPECT_EQ("https://file_link_alternate/",
            entry_proto.file_specific_info().alternate_url());
  EXPECT_EQ("https://file_link_share/",
            entry_proto.file_specific_info().share_url());

  // Regular file specific fields.
  EXPECT_EQ(892721,  entry_proto.file_info().size());
  EXPECT_EQ("3b4382ebefec6e743578c76bbd0575ce",
            entry_proto.file_specific_info().file_md5());
  EXPECT_EQ("https://file_link_resumable_edit_media/",
            entry_proto.upload_url());
  EXPECT_FALSE(entry_proto.file_info().is_directory());
}

TEST(ResourceEntryConversionTest,
     ConvertResourceEntryToDriveEntryProto_HostedDocument) {
  scoped_ptr<base::Value> value =
      google_apis::test_util::LoadJSONFile(
          "chromeos/gdata/hosted_document_entry.json");
  ASSERT_TRUE(value.get());

  scoped_ptr<google_apis::ResourceEntry> resource_entry(
      google_apis::ResourceEntry::ExtractAndParse(*value));
  ASSERT_TRUE(resource_entry.get());

  DriveEntryProto entry_proto =
      ConvertResourceEntryToDriveEntryProto(*resource_entry);

  EXPECT_EQ("Document 1",  entry_proto.title());
  EXPECT_EQ("Document 1.gdoc",  entry_proto.base_name());  // The suffix added.
  EXPECT_EQ(".gdoc", entry_proto.file_specific_info().document_extension());
  EXPECT_EQ("document:5_document_resource_id",  entry_proto.resource_id());
  EXPECT_EQ("https://3_document_content/",  entry_proto.download_url());
  EXPECT_EQ("https://3_document_self_link/document:5_document_resource_id",
            entry_proto.edit_url());
  EXPECT_EQ("",  entry_proto.parent_resource_id());

  EXPECT_FALSE(entry_proto.deleted());
  EXPECT_FALSE(entry_proto.shared_with_me());

  // 2011-12-12T23:28:52.783Z
  base::Time::Exploded exploded;
  exploded.year = 2011;
  exploded.month = 12;
  exploded.day_of_month = 12;
  exploded.day_of_week = 1;  // Monday
  exploded.hour = 23;
  exploded.minute = 28;
  exploded.second = 52;
  exploded.millisecond = 783;
  const base::Time expected_last_modified_time =
      base::Time::FromUTCExploded(exploded);

  // 2011-12-12T23:28:46.686Z
  exploded.year = 2011;
  exploded.month = 12;
  exploded.day_of_month = 12;
  exploded.day_of_week = 1;  // Monday
  exploded.hour = 23;
  exploded.minute = 28;
  exploded.second = 46;
  exploded.millisecond = 686;
  const base::Time expected_creation_time =
      base::Time::FromUTCExploded(exploded);

  // 2011-12-13T02:12:18.527Z
  exploded.year = 2011;
  exploded.month = 12;
  exploded.day_of_month = 13;
  exploded.day_of_week = 2;  // Tuesday
  exploded.hour = 2;
  exploded.minute = 12;
  exploded.second = 18;
  exploded.millisecond = 527;
  const base::Time expected_last_accessed_time =
      base::Time::FromUTCExploded(exploded);

  EXPECT_EQ(expected_last_modified_time.ToInternalValue(),
            entry_proto.file_info().last_modified());
  EXPECT_EQ(expected_last_accessed_time.ToInternalValue(),
            entry_proto.file_info().last_accessed());
  EXPECT_EQ(expected_creation_time.ToInternalValue(),
            entry_proto.file_info().creation_time());

  EXPECT_EQ("text/html",
            entry_proto.file_specific_info().content_mime_type());
  EXPECT_TRUE(entry_proto.file_specific_info().is_hosted_document());
  EXPECT_EQ("https://3_document_thumbnail_link/",
            entry_proto.file_specific_info().thumbnail_url());
  EXPECT_EQ("https://3_document_alternate_link/",
            entry_proto.file_specific_info().alternate_url());
  EXPECT_EQ("https://3_document_share_link/",
            entry_proto.file_specific_info().share_url());

  // The size should be 0 for a hosted document.
  EXPECT_EQ(0,  entry_proto.file_info().size());
  EXPECT_FALSE(entry_proto.file_info().is_directory());
}

TEST(ResourceEntryConversionTest,
     ConvertResourceEntryToDriveEntryProto_Directory) {
  scoped_ptr<base::Value> value =
      google_apis::test_util::LoadJSONFile(
          "chromeos/gdata/directory_entry.json");
  ASSERT_TRUE(value.get());

  scoped_ptr<google_apis::ResourceEntry> resource_entry(
      google_apis::ResourceEntry::ExtractAndParse(*value));
  ASSERT_TRUE(resource_entry.get());

  DriveEntryProto entry_proto =
      ConvertResourceEntryToDriveEntryProto(*resource_entry);

  EXPECT_EQ("Sub Directory Folder",  entry_proto.title());
  EXPECT_EQ("Sub Directory Folder",  entry_proto.base_name());
  EXPECT_EQ("folder:sub_dir_folder_resource_id",  entry_proto.resource_id());
  EXPECT_EQ("https://1_folder_content_url/",  entry_proto.download_url());
  EXPECT_EQ("https://dir2_sub_self_link/folder:sub_dir_folder_resource_id",
            entry_proto.edit_url());
  // The parent resource ID should be obtained as this is a sub directory
  // under a non-root directory.
  EXPECT_EQ("folder:1_folder_resource_id",  entry_proto.parent_resource_id());

  EXPECT_FALSE(entry_proto.deleted());
  EXPECT_FALSE(entry_proto.shared_with_me());

  // 2011-04-01T18:34:08.234Z
  base::Time::Exploded exploded;
  exploded.year = 2011;
  exploded.month = 04;
  exploded.day_of_month = 01;
  exploded.day_of_week = 5;  // Friday
  exploded.hour = 18;
  exploded.minute = 34;
  exploded.second = 8;
  exploded.millisecond = 234;
  const base::Time expected_last_modified_time =
      base::Time::FromUTCExploded(exploded);

  // 2010-11-07T05:03:54.719Z
  exploded.year = 2010;
  exploded.month = 11;
  exploded.day_of_month = 7;
  exploded.day_of_week = 0;  // Sunday
  exploded.hour = 5;
  exploded.minute = 3;
  exploded.second = 54;
  exploded.millisecond = 719;
  const base::Time expected_creation_time =
      base::Time::FromUTCExploded(exploded);

  // 2011-11-02T04:37:38.469Z
  exploded.year = 2011;
  exploded.month = 11;
  exploded.day_of_month = 2;
  exploded.day_of_week = 2;  // Tuesday
  exploded.hour = 4;
  exploded.minute = 37;
  exploded.second = 38;
  exploded.millisecond = 469;
  const base::Time expected_last_accessed_time =
      base::Time::FromUTCExploded(exploded);

  EXPECT_EQ(expected_last_modified_time.ToInternalValue(),
            entry_proto.file_info().last_modified());
  EXPECT_EQ(expected_last_accessed_time.ToInternalValue(),
            entry_proto.file_info().last_accessed());
  EXPECT_EQ(expected_creation_time.ToInternalValue(),
            entry_proto.file_info().creation_time());

  // Directory should have this.
  EXPECT_EQ("https://2_folder_resumable_create_media_link/",
            entry_proto.upload_url());

  EXPECT_TRUE(entry_proto.file_info().is_directory());
}

TEST(ResourceEntryConversionTest,
     ConvertResourceEntryToDriveEntryProto_DeletedHostedDocument) {
  scoped_ptr<base::Value> value =
      google_apis::test_util::LoadJSONFile(
          "chromeos/gdata/deleted_hosted_document_entry.json");
  ASSERT_TRUE(value.get());

  scoped_ptr<google_apis::ResourceEntry> resource_entry(
      google_apis::ResourceEntry::ExtractAndParse(*value));
  ASSERT_TRUE(resource_entry.get());

  DriveEntryProto entry_proto =
      ConvertResourceEntryToDriveEntryProto(*resource_entry);

  EXPECT_EQ("Deleted document",  entry_proto.title());
  EXPECT_EQ("Deleted document.gdoc",  entry_proto.base_name());
  EXPECT_EQ("document:deleted_in_root_id",  entry_proto.resource_id());
  EXPECT_EQ("https://content_url/",  entry_proto.download_url());
  EXPECT_EQ("https://edit_url/document%3Adeleted_in_root_id",
            entry_proto.edit_url());
  EXPECT_EQ("",  entry_proto.parent_resource_id());

  EXPECT_TRUE(entry_proto.deleted());  // The document was deleted.
  EXPECT_FALSE(entry_proto.shared_with_me());

  // 2012-04-10T22:50:55.797Z
  base::Time::Exploded exploded;
  exploded.year = 2012;
  exploded.month = 04;
  exploded.day_of_month = 10;
  exploded.day_of_week = 2;  // Tuesday
  exploded.hour = 22;
  exploded.minute = 50;
  exploded.second = 55;
  exploded.millisecond = 797;
  const base::Time expected_last_modified_time =
      base::Time::FromUTCExploded(exploded);

  // 2012-04-10T22:50:53.237Z
  exploded.year = 2012;
  exploded.month = 04;
  exploded.day_of_month = 10;
  exploded.day_of_week = 2;  // Tuesday
  exploded.hour = 22;
  exploded.minute = 50;
  exploded.second = 53;
  exploded.millisecond = 237;
  const base::Time expected_creation_time =
      base::Time::FromUTCExploded(exploded);

  // 2012-04-10T22:50:55.797Z
  exploded.year = 2012;
  exploded.month = 04;
  exploded.day_of_month = 10;
  exploded.day_of_week = 2;  // Tuesday
  exploded.hour = 22;
  exploded.minute = 50;
  exploded.second = 55;
  exploded.millisecond = 797;
  const base::Time expected_last_accessed_time =
      base::Time::FromUTCExploded(exploded);

  EXPECT_EQ(expected_last_modified_time.ToInternalValue(),
            entry_proto.file_info().last_modified());
  EXPECT_EQ(expected_last_accessed_time.ToInternalValue(),
            entry_proto.file_info().last_accessed());
  EXPECT_EQ(expected_creation_time.ToInternalValue(),
            entry_proto.file_info().creation_time());

  EXPECT_EQ("text/html",
            entry_proto.file_specific_info().content_mime_type());
  EXPECT_TRUE(entry_proto.file_specific_info().is_hosted_document());
  EXPECT_EQ("",
            entry_proto.file_specific_info().thumbnail_url());
  EXPECT_EQ("https://alternate/document%3Adeleted_in_root_id/edit",
            entry_proto.file_specific_info().alternate_url());
  EXPECT_EQ("",
            entry_proto.file_specific_info().share_url());

  // The size should be 0 for a hosted document.
  EXPECT_EQ(0,  entry_proto.file_info().size());
}

TEST(ResourceEntryConversionTest,
     ConvertResourceEntryToDriveEntryProto_SharedWithMeEntry) {
  scoped_ptr<base::Value> value = google_apis::test_util::LoadJSONFile(
      "chromeos/gdata/shared_with_me_entry.json");
  ASSERT_TRUE(value.get());

  scoped_ptr<google_apis::ResourceEntry> resource_entry(
      google_apis::ResourceEntry::ExtractAndParse(*value));
  ASSERT_TRUE(resource_entry.get());

  DriveEntryProto entry_proto =
      ConvertResourceEntryToDriveEntryProto(*resource_entry);

  EXPECT_TRUE(entry_proto.shared_with_me());
}

}  // namespace drive
