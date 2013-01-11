// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google_apis/fake_drive_service.h"

#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/google_apis/gdata_wapi_parser.h"
#include "chrome/browser/google_apis/test_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace google_apis {

class FakeDriveServiceTest : public testing::Test {
 protected:
  FakeDriveServiceTest()
      : ui_thread_(content::BrowserThread::UI, &message_loop_) {
  }

  // Returns the resource entry that matches |resource_id|.
  scoped_ptr<ResourceEntry> FindEntry(const std::string& resource_id) {
    GDataErrorCode error = GDATA_OTHER_ERROR;
    scoped_ptr<ResourceEntry> resource_entry;
    fake_service_.GetResourceEntry(
        resource_id,
        base::Bind(&test_util::CopyResultsFromGetResourceEntryCallback,
                   &error,
                   &resource_entry));
    message_loop_.RunUntilIdle();
    return resource_entry.Pass();
  }

  // Returns true if the resource identified by |resource_id| exists.
  bool Exists(const std::string& resource_id) {
    scoped_ptr<ResourceEntry> resource_entry = FindEntry(resource_id);
    return resource_entry;
  }

  // Adds a new directory at |parent_content_url| (root if empty) with the
  // given name. Returns true on success.
  bool AddNewDirectory(const GURL& parent_content_url,
                       const FilePath::StringType& directory_name) {
    GDataErrorCode error = GDATA_OTHER_ERROR;
    scoped_ptr<ResourceEntry> resource_entry;
    fake_service_.AddNewDirectory(
        parent_content_url,
        directory_name,
        base::Bind(&test_util::CopyResultsFromGetResourceEntryCallback,
                   &error,
                   &resource_entry));
    message_loop_.RunUntilIdle();
    return error == HTTP_SUCCESS;
  }

  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  FakeDriveService fake_service_;
};

TEST_F(FakeDriveServiceTest, GetResourceList_All) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi("gdata/root_feed.json"));

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceList> resource_list;
  fake_service_.GetResourceList(
      GURL(),
      0,  // start_changestamp
      "",  // search_query
      false, // shared_with_me
      "",  // directory_resource_id
      base::Bind(&test_util::CopyResultsFromGetResourceListCallback,
                 &error,
                 &resource_list));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);
  ASSERT_TRUE(resource_list);
  // Do some sanity check.
  EXPECT_EQ(12U, resource_list->entries().size());
}

TEST_F(FakeDriveServiceTest, GetResourceList_InRootDirectory) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi("gdata/root_feed.json"));

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceList> resource_list;
  fake_service_.GetResourceList(
      GURL(),
      0,  // start_changestamp
      "",  // search_query
      false, // shared_with_me
      "folder:root",  // directory_resource_id
      base::Bind(&test_util::CopyResultsFromGetResourceListCallback,
                 &error,
                 &resource_list));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);
  ASSERT_TRUE(resource_list);
  // Do some sanity check. There are 7 entries in the root directory.
  EXPECT_EQ(7U, resource_list->entries().size());
}

TEST_F(FakeDriveServiceTest, GetResourceList_Search) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi("gdata/root_feed.json"));

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceList> resource_list;
  fake_service_.GetResourceList(
      GURL(),
      0,  // start_changestamp
      "File",  // search_query
      false, // shared_with_me
      "",  // directory_resource_id
      base::Bind(&test_util::CopyResultsFromGetResourceListCallback,
                 &error,
                 &resource_list));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);
  ASSERT_TRUE(resource_list);
  // Do some sanity check. There are 3 entries that contain "File" in their
  // titles.
  EXPECT_EQ(3U, resource_list->entries().size());
}

TEST_F(FakeDriveServiceTest, GetResourceList_NoNewEntries) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi("gdata/root_feed.json"));
  // Load the account_metadata.json as well to add the largest changestamp
  // (654321) to the existing entries.
  ASSERT_TRUE(fake_service_.LoadAccountMetadataForWapi(
      "gdata/account_metadata.json"));

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceList> resource_list;
  fake_service_.GetResourceList(
      GURL(),
      654321 + 1,  // start_changestamp
      "",  // search_query
      false, // shared_with_me
      "",  // directory_resource_id
      base::Bind(&test_util::CopyResultsFromGetResourceListCallback,
                 &error,
                 &resource_list));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);
  ASSERT_TRUE(resource_list);
  // This should be empty as the latest changestamp was passed to
  // GetResourceList(), hence there should be no new entries.
  EXPECT_EQ(0U, resource_list->entries().size());
}

TEST_F(FakeDriveServiceTest, GetResourceList_WithNewEntry) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi("gdata/root_feed.json"));
  // Load the account_metadata.json as well to add the largest changestamp
  // (654321) to the existing entries.
  ASSERT_TRUE(fake_service_.LoadAccountMetadataForWapi(
      "gdata/account_metadata.json"));
  // Add a new directory in the root directory. The new directory will have
  // the changestamp of 654322.
  ASSERT_TRUE(AddNewDirectory(GURL(), FILE_PATH_LITERAL("new directory")));

  // Get the resource list newer than 654321.
  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceList> resource_list;
  fake_service_.GetResourceList(
      GURL(),
      654321 + 1,  // start_changestamp
      "",  // search_query
      false, // shared_with_me
      "",  // directory_resource_id
      base::Bind(&test_util::CopyResultsFromGetResourceListCallback,
                 &error,
                 &resource_list));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);
  ASSERT_TRUE(resource_list);
  // The result should only contain the newly created directory.
  ASSERT_EQ(1U, resource_list->entries().size());
  EXPECT_EQ("new directory",
            UTF16ToUTF8(resource_list->entries()[0]->title()));
}

TEST_F(FakeDriveServiceTest, GetResourceList_Offline) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi("gdata/root_feed.json"));
  fake_service_.set_offline(true);

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceList> resource_list;
  fake_service_.GetResourceList(
      GURL(),
      0,  // start_changestamp
      "",  // search_query
      false, // shared_with_me
      "",  // directory_resource_id
      base::Bind(&test_util::CopyResultsFromGetResourceListCallback,
                 &error,
                 &resource_list));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(GDATA_NO_CONNECTION, error);
  EXPECT_FALSE(resource_list);
}

TEST_F(FakeDriveServiceTest, GetAccountMetadata) {
  ASSERT_TRUE(fake_service_.LoadAccountMetadataForWapi(
      "gdata/account_metadata.json"));

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<AccountMetadataFeed> account_metadata;
  fake_service_.GetAccountMetadata(
      base::Bind(&test_util::CopyResultsFromGetAccountMetadataCallback,
                 &error,
                 &account_metadata));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);

  ASSERT_TRUE(account_metadata);
  // Do some sanity check.
  EXPECT_EQ(2U, account_metadata->installed_apps().size());
}

TEST_F(FakeDriveServiceTest, GetAccountMetadata_Offline) {
  ASSERT_TRUE(fake_service_.LoadAccountMetadataForWapi(
      "gdata/account_metadata.json"));
  fake_service_.set_offline(true);

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<AccountMetadataFeed> account_metadata;
  fake_service_.GetAccountMetadata(
      base::Bind(&test_util::CopyResultsFromGetAccountMetadataCallback,
                 &error,
                 &account_metadata));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(GDATA_NO_CONNECTION, error);
  EXPECT_FALSE(account_metadata);
}

TEST_F(FakeDriveServiceTest, GetApplicationInfo) {
  ASSERT_TRUE(fake_service_.LoadApplicationInfoForDriveApi(
      "drive/applist.json"));

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<base::Value> app_info;
  fake_service_.GetApplicationInfo(
      base::Bind(&test_util::CopyResultsFromGetDataCallback,
                 &error,
                 &app_info));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);

  ASSERT_TRUE(app_info);
}

TEST_F(FakeDriveServiceTest, GetApplicationInfo_Offline) {
  ASSERT_TRUE(fake_service_.LoadApplicationInfoForDriveApi(
      "drive/applist.json"));
  fake_service_.set_offline(true);

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<base::Value> app_info;
  fake_service_.GetApplicationInfo(
      base::Bind(&test_util::CopyResultsFromGetDataCallback,
                 &error,
                 &app_info));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(GDATA_NO_CONNECTION, error);
  EXPECT_FALSE(app_info);
}

TEST_F(FakeDriveServiceTest, GetResourceEntry_ExistingFile) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi("gdata/root_feed.json"));

  const std::string kResourceId = "file:2_file_resource_id";
  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceEntry> resource_entry;
  fake_service_.GetResourceEntry(
      kResourceId,
      base::Bind(&test_util::CopyResultsFromGetResourceEntryCallback,
                 &error,
                 &resource_entry));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);
  ASSERT_TRUE(resource_entry);
  // Do some sanity check.
  EXPECT_EQ(kResourceId, resource_entry->resource_id());
}

TEST_F(FakeDriveServiceTest, GetResourceEntry_NonexistingFile) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi("gdata/root_feed.json"));

  const std::string kResourceId = "file:nonexisting_resource_id";
  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceEntry> resource_entry;
  fake_service_.GetResourceEntry(
      kResourceId,
      base::Bind(&test_util::CopyResultsFromGetResourceEntryCallback,
                 &error,
                 &resource_entry));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(HTTP_NOT_FOUND, error);
  ASSERT_FALSE(resource_entry);
}

TEST_F(FakeDriveServiceTest, GetResourceEntry_Offline) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi("gdata/root_feed.json"));
  fake_service_.set_offline(true);

  const std::string kResourceId = "file:2_file_resource_id";
  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceEntry> resource_entry;
  fake_service_.GetResourceEntry(
      kResourceId,
      base::Bind(&test_util::CopyResultsFromGetResourceEntryCallback,
                 &error,
                 &resource_entry));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(GDATA_NO_CONNECTION, error);
  EXPECT_FALSE(resource_entry);
}

TEST_F(FakeDriveServiceTest, DeleteResource_ExistingFile) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi("gdata/root_feed.json"));

  // Resource "file:2_file_resource_id" should now exist.
  ASSERT_TRUE(Exists("file:2_file_resource_id"));

  GDataErrorCode error = GDATA_OTHER_ERROR;
  fake_service_.DeleteResource(
      GURL("https://file1_link_self/file:2_file_resource_id"),
      base::Bind(&test_util::CopyResultsFromEntryActionCallback,
                 &error));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);
  // Resource "file:2_file_resource_id" should be gone now.
  EXPECT_FALSE(Exists("file:2_file_resource_id"));
}

TEST_F(FakeDriveServiceTest, DeleteResource_NonexistingFile) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi("gdata/root_feed.json"));

  GDataErrorCode error = GDATA_OTHER_ERROR;
  fake_service_.DeleteResource(
      GURL("https://file1_link_self/file:nonexisting_resource_id"),
      base::Bind(&test_util::CopyResultsFromEntryActionCallback,
                 &error));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(HTTP_NOT_FOUND, error);
}

TEST_F(FakeDriveServiceTest, DeleteResource_Offline) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi("gdata/root_feed.json"));
  fake_service_.set_offline(true);

  GDataErrorCode error = GDATA_OTHER_ERROR;
  fake_service_.DeleteResource(
      GURL("https://file1_link_self/file:2_file_resource_id"),
      base::Bind(&test_util::CopyResultsFromEntryActionCallback,
                 &error));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(GDATA_NO_CONNECTION, error);
}

TEST_F(FakeDriveServiceTest, DownloadFile_ExistingFile) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi("gdata/root_feed.json"));

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  const GURL kContentUrl("https://file_content_url/");
  const FilePath kOutputFilePath = temp_dir.path().AppendASCII("whatever.txt");
  GDataErrorCode error = GDATA_OTHER_ERROR;
  FilePath output_file_path;
  fake_service_.DownloadFile(
      FilePath::FromUTF8Unsafe("/drive/whatever.txt"),  // virtual path
      kOutputFilePath,
      kContentUrl,
      base::Bind(&test_util::CopyResultsFromDownloadActionCallback,
                 &error,
                 &output_file_path),
      GetContentCallback());
  message_loop_.RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_EQ(output_file_path, kOutputFilePath);
  std::string content;
  ASSERT_TRUE(file_util::ReadFileToString(output_file_path, &content));
  EXPECT_EQ(kContentUrl.spec(), content);
}

TEST_F(FakeDriveServiceTest, DownloadFile_NonexistingFile) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi("gdata/root_feed.json"));

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  const GURL kContentUrl("https://non_existing_content_url/");
  const FilePath kOutputFilePath = temp_dir.path().AppendASCII("whatever.txt");
  GDataErrorCode error = GDATA_OTHER_ERROR;
  FilePath output_file_path;
  fake_service_.DownloadFile(
      FilePath::FromUTF8Unsafe("/drive/whatever.txt"),  // virtual path
      kOutputFilePath,
      kContentUrl,
      base::Bind(&test_util::CopyResultsFromDownloadActionCallback,
                 &error,
                 &output_file_path),
      GetContentCallback());
  message_loop_.RunUntilIdle();

  EXPECT_EQ(HTTP_NOT_FOUND, error);
}

TEST_F(FakeDriveServiceTest, DownloadFile_Offline) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi("gdata/root_feed.json"));
  fake_service_.set_offline(true);

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  const GURL kContentUrl("https://file_content_url/");
  const FilePath kOutputFilePath = temp_dir.path().AppendASCII("whatever.txt");
  GDataErrorCode error = GDATA_OTHER_ERROR;
  FilePath output_file_path;
  fake_service_.DownloadFile(
      FilePath::FromUTF8Unsafe("/drive/whatever.txt"),  // virtual path
      kOutputFilePath,
      kContentUrl,
      base::Bind(&test_util::CopyResultsFromDownloadActionCallback,
                 &error,
                 &output_file_path),
      GetContentCallback());
  message_loop_.RunUntilIdle();

  EXPECT_EQ(GDATA_NO_CONNECTION, error);
}

TEST_F(FakeDriveServiceTest, CopyHostedDocument_ExistingHostedDocument) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi("gdata/root_feed.json"));

  const std::string kResourceId = "document:5_document_resource_id";
  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceEntry> resource_entry;
  fake_service_.CopyHostedDocument(
      kResourceId,
      FILE_PATH_LITERAL("new name"),
      base::Bind(&test_util::CopyResultsFromGetResourceEntryCallback,
                 &error,
                 &resource_entry));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);
  ASSERT_TRUE(resource_entry);
  // The copied entry should have the new resource ID and the title.
  EXPECT_EQ(kResourceId + "_copied", resource_entry->resource_id());
  EXPECT_EQ("new name", UTF16ToUTF8(resource_entry->title()));
  // Should be incremented as a new hosted document was created.
  EXPECT_EQ(1, fake_service_.largest_changestamp());
}

TEST_F(FakeDriveServiceTest, CopyHostedDocument_NonexistingHostedDocument) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi("gdata/root_feed.json"));

  const std::string kResourceId = "document:nonexisting_resource_id";
  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceEntry> resource_entry;
  fake_service_.CopyHostedDocument(
      kResourceId,
      FILE_PATH_LITERAL("new name"),
      base::Bind(&test_util::CopyResultsFromGetResourceEntryCallback,
                 &error,
                 &resource_entry));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(HTTP_NOT_FOUND, error);
}

TEST_F(FakeDriveServiceTest, CopyHostedDocument_ExistingRegularFile) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi("gdata/root_feed.json"));

  const std::string kResourceId = "file:2_file_resource_id";
  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceEntry> resource_entry;
  fake_service_.CopyHostedDocument(
      kResourceId,
      FILE_PATH_LITERAL("new name"),
      base::Bind(&test_util::CopyResultsFromGetResourceEntryCallback,
                 &error,
                 &resource_entry));
  message_loop_.RunUntilIdle();

  // The copy should fail as this is not a hosted document.
  EXPECT_EQ(HTTP_NOT_FOUND, error);
  EXPECT_FALSE(resource_entry);
}

TEST_F(FakeDriveServiceTest, CopyHostedDocument_Offline) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi("gdata/root_feed.json"));
  fake_service_.set_offline(true);

  const std::string kResourceId = "document:5_document_resource_id";
  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceEntry> resource_entry;
  fake_service_.CopyHostedDocument(
      kResourceId,
      FILE_PATH_LITERAL("new name"),
      base::Bind(&test_util::CopyResultsFromGetResourceEntryCallback,
                 &error,
                 &resource_entry));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(GDATA_NO_CONNECTION, error);
  EXPECT_FALSE(resource_entry);
}

TEST_F(FakeDriveServiceTest, RenameResource_ExistingFile) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi("gdata/root_feed.json"));

  const std::string kResourceId = "file:2_file_resource_id";
  const GURL kEditUrl("https://file1_link_self/file:2_file_resource_id");

  GDataErrorCode error = GDATA_OTHER_ERROR;
  fake_service_.RenameResource(
      kEditUrl,
      FILE_PATH_LITERAL("new name"),
      base::Bind(&test_util::CopyResultsFromEntryActionCallback,
                 &error));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);

  scoped_ptr<ResourceEntry> resource_entry = FindEntry(kResourceId);
  ASSERT_TRUE(resource_entry);
  EXPECT_EQ("new name", UTF16ToUTF8(resource_entry->title()));
  // Should be incremented as a file was renamed.
  EXPECT_EQ(1, fake_service_.largest_changestamp());
}

TEST_F(FakeDriveServiceTest, RenameResource_NonexistingFile) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi("gdata/root_feed.json"));

  const GURL kEditUrl("https://file1_link_self/file:nonexisting_file");

  GDataErrorCode error = GDATA_OTHER_ERROR;
  fake_service_.RenameResource(
      kEditUrl,
      FILE_PATH_LITERAL("new name"),
      base::Bind(&test_util::CopyResultsFromEntryActionCallback,
                 &error));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(HTTP_NOT_FOUND, error);
}

TEST_F(FakeDriveServiceTest, RenameResource_Offline) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi("gdata/root_feed.json"));
  fake_service_.set_offline(true);

  const std::string kResourceId = "file:2_file_resource_id";
  const GURL kEditUrl("https://file1_link_self/file:2_file_resource_id");

  GDataErrorCode error = GDATA_OTHER_ERROR;
  fake_service_.RenameResource(
      kEditUrl,
      FILE_PATH_LITERAL("new name"),
      base::Bind(&test_util::CopyResultsFromEntryActionCallback,
                 &error));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(GDATA_NO_CONNECTION, error);
}

TEST_F(FakeDriveServiceTest, AddResourceToDirectory_FileInRootDirectory) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi("gdata/root_feed.json"));

  const std::string kResourceId = "file:2_file_resource_id";
  const GURL kEditUrl("https://file1_link_self/file:2_file_resource_id");
  const GURL kNewParentContentUrl("https://new_url");

  scoped_ptr<ResourceEntry> resource_entry = FindEntry(kResourceId);
  ASSERT_TRUE(resource_entry);
  // The parent link should not exist as this file is in the root directory.
  const google_apis::Link* parent_link =
      resource_entry->GetLinkByType(Link::LINK_PARENT);
  ASSERT_FALSE(parent_link);

  GDataErrorCode error = GDATA_OTHER_ERROR;
  fake_service_.AddResourceToDirectory(
      kNewParentContentUrl,
      kEditUrl,
      base::Bind(&test_util::CopyResultsFromEntryActionCallback,
                 &error));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);

  resource_entry = FindEntry(kResourceId);
  ASSERT_TRUE(resource_entry);
  // The parent link should now exist as the parent directory is changed.
  parent_link = resource_entry->GetLinkByType(Link::LINK_PARENT);
  ASSERT_TRUE(parent_link);
  EXPECT_EQ(kNewParentContentUrl, parent_link->href());
  // Should be incremented as a file was moved.
  EXPECT_EQ(1, fake_service_.largest_changestamp());
}

TEST_F(FakeDriveServiceTest, AddResourceToDirectory_FileInNonRootDirectory) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi("gdata/root_feed.json"));

  const std::string kResourceId = "file:subdirectory_file_1_id";
  const GURL kEditUrl(
      "https://dir1_file_link_self/file:subdirectory_file_1_id");
  const GURL kNewParentContentUrl("https://new_url");

  scoped_ptr<ResourceEntry> resource_entry = FindEntry(kResourceId);
  ASSERT_TRUE(resource_entry);
  // Here's the original parent link.
  const google_apis::Link* parent_link =
      resource_entry->GetLinkByType(Link::LINK_PARENT);
  ASSERT_TRUE(parent_link);
  EXPECT_EQ("https://dir_1_self_link/folder:1_folder_resource_id",
            parent_link->href().spec());

  GDataErrorCode error = GDATA_OTHER_ERROR;
  fake_service_.AddResourceToDirectory(
      kNewParentContentUrl,
      kEditUrl,
      base::Bind(&test_util::CopyResultsFromEntryActionCallback,
                 &error));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);

  resource_entry = FindEntry(kResourceId);
  ASSERT_TRUE(resource_entry);
  // The parent link should now be changed.
  parent_link = resource_entry->GetLinkByType(Link::LINK_PARENT);
  ASSERT_TRUE(parent_link);
  EXPECT_EQ(kNewParentContentUrl, parent_link->href());
  // Should be incremented as a file was moved.
  EXPECT_EQ(1, fake_service_.largest_changestamp());
}

TEST_F(FakeDriveServiceTest, AddResourceToDirectory_NonexistingFile) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi("gdata/root_feed.json"));

  const GURL kEditUrl("https://file1_link_self/file:nonexisting_file");
  const GURL kNewParentContentUrl("https://new_url");

  GDataErrorCode error = GDATA_OTHER_ERROR;
  fake_service_.AddResourceToDirectory(
      kNewParentContentUrl,
      kEditUrl,
      base::Bind(&test_util::CopyResultsFromEntryActionCallback,
                 &error));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(HTTP_NOT_FOUND, error);
}

TEST_F(FakeDriveServiceTest, AddResourceToDirectory_Offline) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi("gdata/root_feed.json"));
  fake_service_.set_offline(true);

  const std::string kResourceId = "file:2_file_resource_id";
  const GURL kEditUrl("https://file1_link_self/file:2_file_resource_id");
  const GURL kNewParentContentUrl("https://new_url");

  GDataErrorCode error = GDATA_OTHER_ERROR;
  fake_service_.AddResourceToDirectory(
      kNewParentContentUrl,
      kEditUrl,
      base::Bind(&test_util::CopyResultsFromEntryActionCallback,
                 &error));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(GDATA_NO_CONNECTION, error);
}

TEST_F(FakeDriveServiceTest, RemoveResourceFromDirectory_ExistingFile) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi("gdata/root_feed.json"));

  const std::string kResourceId("file:subdirectory_file_1_id");
  const GURL kParentContentUrl(
      "https://dir_1_self_link/folder:1_folder_resource_id");

  scoped_ptr<ResourceEntry> resource_entry = FindEntry(kResourceId);
  ASSERT_TRUE(resource_entry);
  // The parent link should exist now.
  const google_apis::Link* parent_link =
      resource_entry->GetLinkByType(Link::LINK_PARENT);
  ASSERT_TRUE(parent_link);

  GDataErrorCode error = GDATA_OTHER_ERROR;
  fake_service_.RemoveResourceFromDirectory(
      kParentContentUrl,
      kResourceId,
      base::Bind(&test_util::CopyResultsFromEntryActionCallback,
                 &error));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);

  resource_entry = FindEntry(kResourceId);
  ASSERT_TRUE(resource_entry);
  // The parent link should be gone now.
  parent_link = resource_entry->GetLinkByType(Link::LINK_PARENT);
  ASSERT_FALSE(parent_link);
  // Should be incremented as a file was moved to the root directory.
  EXPECT_EQ(1, fake_service_.largest_changestamp());
}

TEST_F(FakeDriveServiceTest, RemoveResourceFromDirectory_NonexistingFile) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi("gdata/root_feed.json"));

  const std::string kResourceId("file:nonexisting_file");
  const GURL kParentContentUrl(
      "https://dir_1_self_link/folder:1_folder_resource_id");

  GDataErrorCode error = GDATA_OTHER_ERROR;
  fake_service_.RemoveResourceFromDirectory(
      kParentContentUrl,
      kResourceId,
      base::Bind(&test_util::CopyResultsFromEntryActionCallback,
                 &error));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(HTTP_NOT_FOUND, error);
}

TEST_F(FakeDriveServiceTest, RemoveResourceFromDirectory_Offline) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi("gdata/root_feed.json"));
  fake_service_.set_offline(true);

  const std::string kResourceId("file:subdirectory_file_1_id");
  const GURL kParentContentUrl(
      "https://dir_1_self_link/folder:1_folder_resource_id");

  GDataErrorCode error = GDATA_OTHER_ERROR;
  fake_service_.RemoveResourceFromDirectory(
      kParentContentUrl,
      kResourceId,
      base::Bind(&test_util::CopyResultsFromEntryActionCallback,
                 &error));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(GDATA_NO_CONNECTION, error);
}

TEST_F(FakeDriveServiceTest, AddNewDirectory_ToRootDirectory) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi("gdata/root_feed.json"));

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceEntry> resource_entry;
  fake_service_.AddNewDirectory(
      GURL(),  // Empty means add it to the root directory.
      FILE_PATH_LITERAL("new directory"),
      base::Bind(&test_util::CopyResultsFromGetResourceEntryCallback,
                 &error,
                 &resource_entry));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);
  ASSERT_TRUE(resource_entry);
  EXPECT_EQ("resource_id_1", resource_entry->resource_id());
  EXPECT_EQ("new directory", UTF16ToUTF8(resource_entry->title()));
  // The parent link should not exist as the new directory was added in the
  // root.
  const google_apis::Link* parent_link =
      resource_entry->GetLinkByType(Link::LINK_PARENT);
  ASSERT_FALSE(parent_link);
  // Should be incremented as a new directory was created.
  EXPECT_EQ(1, fake_service_.largest_changestamp());
}

TEST_F(FakeDriveServiceTest, AddNewDirectory_ToNonRootDirectory) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi("gdata/root_feed.json"));

  const GURL kParentContentUrl(
      "https://dir_1_self_link/folder:1_folder_resource_id");

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceEntry> resource_entry;
  fake_service_.AddNewDirectory(
      kParentContentUrl,
      FILE_PATH_LITERAL("new directory"),
      base::Bind(&test_util::CopyResultsFromGetResourceEntryCallback,
                 &error,
                 &resource_entry));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);
  ASSERT_TRUE(resource_entry);
  EXPECT_EQ("resource_id_1", resource_entry->resource_id());
  EXPECT_EQ("new directory", UTF16ToUTF8(resource_entry->title()));
  const google_apis::Link* parent_link =
      resource_entry->GetLinkByType(Link::LINK_PARENT);
  ASSERT_TRUE(parent_link);
  EXPECT_EQ(kParentContentUrl, parent_link->href());
  // Should be incremented as a new directory was created.
  EXPECT_EQ(1, fake_service_.largest_changestamp());
}

TEST_F(FakeDriveServiceTest, AddNewDirectory_ToNonexistingDirectory) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi("gdata/root_feed.json"));

  const GURL kParentContentUrl(
      "https://dir_1_self_link/folder:nonexisting_resource_id");

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceEntry> resource_entry;
  fake_service_.AddNewDirectory(
      kParentContentUrl,
      FILE_PATH_LITERAL("new directory"),
      base::Bind(&test_util::CopyResultsFromGetResourceEntryCallback,
                 &error,
                 &resource_entry));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(HTTP_NOT_FOUND, error);
  EXPECT_FALSE(resource_entry);
}

TEST_F(FakeDriveServiceTest, AddNewDirectory_Offline) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi("gdata/root_feed.json"));
  fake_service_.set_offline(true);

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceEntry> resource_entry;
  fake_service_.AddNewDirectory(
      GURL(),  // Empty means add it to the root directory.
      FILE_PATH_LITERAL("new directory"),
      base::Bind(&test_util::CopyResultsFromGetResourceEntryCallback,
                 &error,
                 &resource_entry));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(GDATA_NO_CONNECTION, error);
  EXPECT_FALSE(resource_entry);
}

}  // namespace google_apis
