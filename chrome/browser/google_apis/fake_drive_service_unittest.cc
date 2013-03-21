// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google_apis/fake_drive_service.h"

#include <string>

#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/google_apis/drive_api_parser.h"
#include "chrome/browser/google_apis/gdata_wapi_operations.h"
#include "chrome/browser/google_apis/gdata_wapi_parser.h"
#include "chrome/browser/google_apis/test_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace google_apis {

namespace {

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
        test_util::CreateCopyResultCallback(&error, &resource_entry));
    message_loop_.RunUntilIdle();
    return resource_entry.Pass();
  }

  // Returns true if the resource identified by |resource_id| exists.
  bool Exists(const std::string& resource_id) {
    scoped_ptr<ResourceEntry> resource_entry = FindEntry(resource_id);
    return resource_entry;
  }

  // Adds a new directory at |parent_resource_id| with the given name.
  // Returns true on success.
  bool AddNewDirectory(const std::string& parent_resource_id,
                       const std::string& directory_name) {
    GDataErrorCode error = GDATA_OTHER_ERROR;
    scoped_ptr<ResourceEntry> resource_entry;
    fake_service_.AddNewDirectory(
        parent_resource_id,
        directory_name,
        test_util::CreateCopyResultCallback(&error, &resource_entry));
    message_loop_.RunUntilIdle();
    return error == HTTP_CREATED;
  }

  // Returns true if the resource identified by |resource_id| has a parent
  // identified by |parent_id|.
  bool HasParent(const std::string& resource_id, const std::string& parent_id) {
    const GURL parent_url = FakeDriveService::GetFakeLinkUrl(parent_id);
    scoped_ptr<ResourceEntry> resource_entry = FindEntry(resource_id);
    if (resource_entry.get()) {
      for (size_t i = 0; i < resource_entry->links().size(); ++i) {
        if (resource_entry->links()[i]->type() == Link::LINK_PARENT &&
            resource_entry->links()[i]->href() == parent_url)
          return true;
      }
    }
    return false;
  }

  int64 GetLargestChangeByAboutResource() {
    GDataErrorCode error;
    scoped_ptr<AboutResource> about_resource;
    fake_service_.GetAboutResource(
        test_util::CreateCopyResultCallback(&error, &about_resource));
    message_loop_.RunUntilIdle();
    return about_resource->largest_change_id();
  }

  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  FakeDriveService fake_service_;
};

TEST_F(FakeDriveServiceTest, GetResourceList_All) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "chromeos/gdata/root_feed.json"));

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceList> resource_list;
  fake_service_.GetResourceList(
      GURL(),
      0,  // start_changestamp
      "",  // search_query
      false, // shared_with_me
      "",  // directory_resource_id
      test_util::CreateCopyResultCallback(&error, &resource_list));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);
  ASSERT_TRUE(resource_list);
  // Do some sanity check.
  EXPECT_EQ(14U, resource_list->entries().size());
  EXPECT_EQ(1, fake_service_.resource_list_load_count());
}

TEST_F(FakeDriveServiceTest, GetResourceList_WithStartIndex) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "chromeos/gdata/root_feed.json"));

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceList> resource_list;
  fake_service_.GetResourceList(
      GURL("http://dummyurl/?start-offset=2"),
      0,  // start_changestamp
      "",  // search_query
      false, // shared_with_me
      "",  // directory_resource_id
      test_util::CreateCopyResultCallback(&error, &resource_list));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);
  ASSERT_TRUE(resource_list);
  // Because the start-offset was set to 2, the size should be 12 instead of
  // 14 (the total number).
  EXPECT_EQ(12U, resource_list->entries().size());
  EXPECT_EQ(1, fake_service_.resource_list_load_count());
}

TEST_F(FakeDriveServiceTest, GetResourceList_WithStartIndexAndMaxResults) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "chromeos/gdata/root_feed.json"));

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceList> resource_list;
  fake_service_.GetResourceList(
      GURL("http://localhost/?start-offset=2&max-results=5"),
      0,  // start_changestamp
      "",  // search_query
      false, // shared_with_me
      "",  // directory_resource_id
      test_util::CreateCopyResultCallback(&error, &resource_list));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);
  ASSERT_TRUE(resource_list);
  // Because the max-results was set to 5, the size should be 5 instead of 10.
  EXPECT_EQ(5U, resource_list->entries().size());
  EXPECT_EQ(1, fake_service_.resource_list_load_count());
  // The next link should be provided. The new start-offset should be
  // 2 (original start index) + 5 (max results).
  const google_apis::Link* next_link =
      resource_list->GetLinkByType(Link::LINK_NEXT);
  ASSERT_TRUE(next_link);
  EXPECT_EQ(GURL("http://localhost/?start-offset=7&max-results=5"),
            next_link->href());
}

TEST_F(FakeDriveServiceTest, GetResourceList_WithDefaultMaxResultsChanged) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "chromeos/gdata/root_feed.json"));
  fake_service_.set_default_max_results(3);

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceList> resource_list;
  fake_service_.GetResourceList(
      GURL(),
      0,  // start_changestamp
      "",  // search_query
      false, // shared_with_me
      "",  // directory_resource_id
      test_util::CreateCopyResultCallback(&error, &resource_list));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);
  ASSERT_TRUE(resource_list);
  // Because the default max results was changed to 3, the size should be 3
  // instead of 12 (the total number).
  EXPECT_EQ(3U, resource_list->entries().size());
  EXPECT_EQ(1, fake_service_.resource_list_load_count());
  // The next link should be provided.
  const google_apis::Link* next_link =
      resource_list->GetLinkByType(Link::LINK_NEXT);
  ASSERT_TRUE(next_link);
  EXPECT_EQ(GURL("http://localhost/?start-offset=3&max-results=3"),
            next_link->href());
}

TEST_F(FakeDriveServiceTest, GetResourceList_InRootDirectory) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "chromeos/gdata/root_feed.json"));

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceList> resource_list;
  fake_service_.GetResourceList(
      GURL(),
      0,  // start_changestamp
      "",  // search_query
      false, // shared_with_me
      fake_service_.GetRootResourceId(),  // directory_resource_id
      test_util::CreateCopyResultCallback(&error, &resource_list));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);
  ASSERT_TRUE(resource_list);
  // Do some sanity check. There are 7 entries in the root directory.
  EXPECT_EQ(8U, resource_list->entries().size());
  EXPECT_EQ(1, fake_service_.resource_list_load_count());
}

TEST_F(FakeDriveServiceTest, GetResourceList_Search) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "chromeos/gdata/root_feed.json"));

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceList> resource_list;
  fake_service_.GetResourceList(
      GURL(),
      0,  // start_changestamp
      "File",  // search_query
      false, // shared_with_me
      "",  // directory_resource_id
      test_util::CreateCopyResultCallback(&error, &resource_list));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);
  ASSERT_TRUE(resource_list);
  // Do some sanity check. There are 4 entries that contain "File" in their
  // titles.
  EXPECT_EQ(4U, resource_list->entries().size());
  EXPECT_EQ(1, fake_service_.resource_list_load_count());
}

TEST_F(FakeDriveServiceTest, GetResourceList_SearchWithAttribute) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "chromeos/gdata/root_feed.json"));

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceList> resource_list;
  fake_service_.GetResourceList(
      GURL(),
      0,  // start_changestamp
      "title:1.txt",  // search_query
      false, // shared_with_me
      "",  // directory_resource_id
      test_util::CreateCopyResultCallback(&error, &resource_list));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);
  ASSERT_TRUE(resource_list);
  // Do some sanity check. There are 4 entries that contain "1.txt" in their
  // titles.
  EXPECT_EQ(4U, resource_list->entries().size());
  EXPECT_EQ(1, fake_service_.resource_list_load_count());
}

TEST_F(FakeDriveServiceTest, GetResourceList_SearchMultipleQueries) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "chromeos/gdata/root_feed.json"));

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceList> resource_list;
  fake_service_.GetResourceList(
      GURL(),
      0,  // start_changestamp
      "Directory 1",  // search_query
      false, // shared_with_me
      "",  // directory_resource_id
      test_util::CreateCopyResultCallback(&error, &resource_list));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);
  ASSERT_TRUE(resource_list);
  // There are 2 entries that contain both "Directory" and "1" in their titles.
  EXPECT_EQ(2U, resource_list->entries().size());

  fake_service_.GetResourceList(
      GURL(),
      0,  // start_changestamp
      "\"Directory 1\"",  // search_query
      false, // shared_with_me
      "",  // directory_resource_id
      test_util::CreateCopyResultCallback(&error, &resource_list));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);
  ASSERT_TRUE(resource_list);
  // There is 1 entry that contain "Directory 1" in its title.
  EXPECT_EQ(1U, resource_list->entries().size());
  EXPECT_EQ(2, fake_service_.resource_list_load_count());
}

TEST_F(FakeDriveServiceTest, GetResourceList_NoNewEntries) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "chromeos/gdata/root_feed.json"));
  // Load the account_metadata.json as well to add the largest changestamp
  // (654321) to the existing entries.
  ASSERT_TRUE(fake_service_.LoadAccountMetadataForWapi(
      "chromeos/gdata/account_metadata.json"));

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceList> resource_list;
  fake_service_.GetResourceList(
      GURL(),
      654321 + 1,  // start_changestamp
      "",  // search_query
      false, // shared_with_me
      "",  // directory_resource_id
      test_util::CreateCopyResultCallback(&error, &resource_list));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);
  ASSERT_TRUE(resource_list);
  // This should be empty as the latest changestamp was passed to
  // GetResourceList(), hence there should be no new entries.
  EXPECT_EQ(0U, resource_list->entries().size());
  // It's considered loaded even if the result is empty.
  EXPECT_EQ(1, fake_service_.resource_list_load_count());
}

TEST_F(FakeDriveServiceTest, GetResourceList_WithNewEntry) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "chromeos/gdata/root_feed.json"));
  // Load the account_metadata.json as well to add the largest changestamp
  // (654321) to the existing entries.
  ASSERT_TRUE(fake_service_.LoadAccountMetadataForWapi(
      "chromeos/gdata/account_metadata.json"));
  // Add a new directory in the root directory. The new directory will have
  // the changestamp of 654322.
  ASSERT_TRUE(AddNewDirectory(
      fake_service_.GetRootResourceId(), "new directory"));

  // Get the resource list newer than 654321.
  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceList> resource_list;
  fake_service_.GetResourceList(
      GURL(),
      654321 + 1,  // start_changestamp
      "",  // search_query
      false, // shared_with_me
      "",  // directory_resource_id
      test_util::CreateCopyResultCallback(&error, &resource_list));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);
  ASSERT_TRUE(resource_list);
  // The result should only contain the newly created directory.
  ASSERT_EQ(1U, resource_list->entries().size());
  EXPECT_EQ("new directory", resource_list->entries()[0]->title());
  EXPECT_EQ(1, fake_service_.resource_list_load_count());
}

TEST_F(FakeDriveServiceTest, GetResourceList_Offline) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "chromeos/gdata/root_feed.json"));
  fake_service_.set_offline(true);

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceList> resource_list;
  fake_service_.GetResourceList(
      GURL(),
      0,  // start_changestamp
      "",  // search_query
      false, // shared_with_me
      "",  // directory_resource_id
      test_util::CreateCopyResultCallback(&error, &resource_list));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(GDATA_NO_CONNECTION, error);
  EXPECT_FALSE(resource_list);
}

TEST_F(FakeDriveServiceTest, GetAccountMetadata) {
  ASSERT_TRUE(fake_service_.LoadAccountMetadataForWapi(
      "chromeos/gdata/account_metadata.json"));

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<AccountMetadata> account_metadata;
  fake_service_.GetAccountMetadata(
      test_util::CreateCopyResultCallback(&error, &account_metadata));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);

  ASSERT_TRUE(account_metadata);
  // Do some sanity check.
  EXPECT_EQ(2U, account_metadata->installed_apps().size());
  EXPECT_EQ(1, fake_service_.account_metadata_load_count());
}

TEST_F(FakeDriveServiceTest, GetAccountMetadata_Offline) {
  ASSERT_TRUE(fake_service_.LoadAccountMetadataForWapi(
      "chromeos/gdata/account_metadata.json"));
  fake_service_.set_offline(true);

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<AccountMetadata> account_metadata;
  fake_service_.GetAccountMetadata(
      test_util::CreateCopyResultCallback(&error, &account_metadata));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(GDATA_NO_CONNECTION, error);
  EXPECT_FALSE(account_metadata);
}

TEST_F(FakeDriveServiceTest, GetAboutResource) {
  ASSERT_TRUE(fake_service_.LoadAccountMetadataForWapi(
      "chromeos/gdata/account_metadata.json"));

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<AboutResource> about_resource;
  fake_service_.GetAboutResource(
      test_util::CreateCopyResultCallback(&error, &about_resource));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);

  ASSERT_TRUE(about_resource);
  // Do some sanity check.
  EXPECT_EQ(fake_service_.GetRootResourceId(),
            about_resource->root_folder_id());
  EXPECT_EQ(1, fake_service_.about_resource_load_count());
}

TEST_F(FakeDriveServiceTest, GetAboutResource_Offline) {
  ASSERT_TRUE(fake_service_.LoadAccountMetadataForWapi(
      "chromeos/gdata/account_metadata.json"));
  fake_service_.set_offline(true);

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<AboutResource> about_resource;
  fake_service_.GetAboutResource(
      test_util::CreateCopyResultCallback(&error, &about_resource));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(GDATA_NO_CONNECTION, error);
  EXPECT_FALSE(about_resource);
}

TEST_F(FakeDriveServiceTest, GetAppList) {
  ASSERT_TRUE(fake_service_.LoadAppListForDriveApi(
      "chromeos/drive/applist.json"));

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<AppList> app_list;
  fake_service_.GetAppList(
      test_util::CreateCopyResultCallback(&error, &app_list));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);

  ASSERT_TRUE(app_list);
}

TEST_F(FakeDriveServiceTest, GetAppList_Offline) {
  ASSERT_TRUE(fake_service_.LoadAppListForDriveApi(
      "chromeos/drive/applist.json"));
  fake_service_.set_offline(true);

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<AppList> app_list;
  fake_service_.GetAppList(
      test_util::CreateCopyResultCallback(&error, &app_list));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(GDATA_NO_CONNECTION, error);
  EXPECT_FALSE(app_list);
}

TEST_F(FakeDriveServiceTest, GetResourceEntry_ExistingFile) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "chromeos/gdata/root_feed.json"));

  const std::string kResourceId = "file:2_file_resource_id";
  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceEntry> resource_entry;
  fake_service_.GetResourceEntry(
      kResourceId,
      test_util::CreateCopyResultCallback(&error, &resource_entry));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);
  ASSERT_TRUE(resource_entry);
  // Do some sanity check.
  EXPECT_EQ(kResourceId, resource_entry->resource_id());
}

TEST_F(FakeDriveServiceTest, GetResourceEntry_NonexistingFile) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "chromeos/gdata/root_feed.json"));

  const std::string kResourceId = "file:nonexisting_resource_id";
  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceEntry> resource_entry;
  fake_service_.GetResourceEntry(
      kResourceId,
      test_util::CreateCopyResultCallback(&error, &resource_entry));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(HTTP_NOT_FOUND, error);
  ASSERT_FALSE(resource_entry);
}

TEST_F(FakeDriveServiceTest, GetResourceEntry_Offline) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "chromeos/gdata/root_feed.json"));
  fake_service_.set_offline(true);

  const std::string kResourceId = "file:2_file_resource_id";
  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceEntry> resource_entry;
  fake_service_.GetResourceEntry(
      kResourceId,
      test_util::CreateCopyResultCallback(&error, &resource_entry));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(GDATA_NO_CONNECTION, error);
  EXPECT_FALSE(resource_entry);
}

TEST_F(FakeDriveServiceTest, DeleteResource_ExistingFile) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "chromeos/gdata/root_feed.json"));

  // Resource "file:2_file_resource_id" should now exist.
  ASSERT_TRUE(Exists("file:2_file_resource_id"));

  GDataErrorCode error = GDATA_OTHER_ERROR;
  fake_service_.DeleteResource("file:2_file_resource_id",
                               "",  // etag
                               test_util::CreateCopyResultCallback(&error));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);
  // Resource "file:2_file_resource_id" should be gone now.
  EXPECT_FALSE(Exists("file:2_file_resource_id"));
}

TEST_F(FakeDriveServiceTest, DeleteResource_NonexistingFile) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "chromeos/gdata/root_feed.json"));

  GDataErrorCode error = GDATA_OTHER_ERROR;
  fake_service_.DeleteResource("file:nonexisting_resource_id",
                               "",  // etag
                               test_util::CreateCopyResultCallback(&error));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(HTTP_NOT_FOUND, error);
}

TEST_F(FakeDriveServiceTest, DeleteResource_Offline) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "chromeos/gdata/root_feed.json"));
  fake_service_.set_offline(true);

  GDataErrorCode error = GDATA_OTHER_ERROR;
  fake_service_.DeleteResource("file:2_file_resource_id",
                               "",  // etag
                               test_util::CreateCopyResultCallback(&error));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(GDATA_NO_CONNECTION, error);
}

TEST_F(FakeDriveServiceTest, DownloadFile_ExistingFile) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "chromeos/gdata/root_feed.json"));

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  const GURL kContentUrl("https://file_content_url/");
  const base::FilePath kOutputFilePath =
      temp_dir.path().AppendASCII("whatever.txt");
  GDataErrorCode error = GDATA_OTHER_ERROR;
  base::FilePath output_file_path;
  fake_service_.DownloadFile(
      base::FilePath::FromUTF8Unsafe("/drive/whatever.txt"),  // virtual path
      kOutputFilePath,
      kContentUrl,
      test_util::CreateCopyResultCallback(&error, &output_file_path),
      GetContentCallback());
  message_loop_.RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_EQ(output_file_path, kOutputFilePath);
  std::string content;
  ASSERT_TRUE(file_util::ReadFileToString(output_file_path, &content));
  // The content is "x"s of the file size specified in root_feed.json.
  EXPECT_EQ("xxxxxxxxxx", content);
}

TEST_F(FakeDriveServiceTest, DownloadFile_NonexistingFile) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "chromeos/gdata/root_feed.json"));

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  const GURL kContentUrl("https://non_existing_content_url/");
  const base::FilePath kOutputFilePath =
      temp_dir.path().AppendASCII("whatever.txt");
  GDataErrorCode error = GDATA_OTHER_ERROR;
  base::FilePath output_file_path;
  fake_service_.DownloadFile(
      base::FilePath::FromUTF8Unsafe("/drive/whatever.txt"),  // virtual path
      kOutputFilePath,
      kContentUrl,
      test_util::CreateCopyResultCallback(&error, &output_file_path),
      GetContentCallback());
  message_loop_.RunUntilIdle();

  EXPECT_EQ(HTTP_NOT_FOUND, error);
}

TEST_F(FakeDriveServiceTest, DownloadFile_Offline) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "chromeos/gdata/root_feed.json"));
  fake_service_.set_offline(true);

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  const GURL kContentUrl("https://file_content_url/");
  const base::FilePath kOutputFilePath =
      temp_dir.path().AppendASCII("whatever.txt");
  GDataErrorCode error = GDATA_OTHER_ERROR;
  base::FilePath output_file_path;
  fake_service_.DownloadFile(
      base::FilePath::FromUTF8Unsafe("/drive/whatever.txt"),  // virtual path
      kOutputFilePath,
      kContentUrl,
      test_util::CreateCopyResultCallback(&error, &output_file_path),
      GetContentCallback());
  message_loop_.RunUntilIdle();

  EXPECT_EQ(GDATA_NO_CONNECTION, error);
}

TEST_F(FakeDriveServiceTest, CopyHostedDocument_ExistingHostedDocument) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "chromeos/gdata/root_feed.json"));
  ASSERT_TRUE(fake_service_.LoadAccountMetadataForWapi(
      "chromeos/gdata/account_metadata.json"));

  int64 old_largest_change_id = GetLargestChangeByAboutResource();

  const std::string kResourceId = "document:5_document_resource_id";
  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceEntry> resource_entry;
  fake_service_.CopyHostedDocument(
      kResourceId,
      "new name",
      test_util::CreateCopyResultCallback(&error, &resource_entry));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);
  ASSERT_TRUE(resource_entry);
  // The copied entry should have the new resource ID and the title.
  EXPECT_EQ(kResourceId + "_copied", resource_entry->resource_id());
  EXPECT_EQ("new name", resource_entry->title());
  // Should be incremented as a new hosted document was created.
  EXPECT_EQ(old_largest_change_id + 1, fake_service_.largest_changestamp());
  EXPECT_EQ(old_largest_change_id + 1, GetLargestChangeByAboutResource());
}

TEST_F(FakeDriveServiceTest, CopyHostedDocument_NonexistingHostedDocument) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "chromeos/gdata/root_feed.json"));

  const std::string kResourceId = "document:nonexisting_resource_id";
  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceEntry> resource_entry;
  fake_service_.CopyHostedDocument(
      kResourceId,
      "new name",
      test_util::CreateCopyResultCallback(&error, &resource_entry));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(HTTP_NOT_FOUND, error);
}

TEST_F(FakeDriveServiceTest, CopyHostedDocument_ExistingRegularFile) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "chromeos/gdata/root_feed.json"));

  const std::string kResourceId = "file:2_file_resource_id";
  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceEntry> resource_entry;
  fake_service_.CopyHostedDocument(
      kResourceId,
      "new name",
      test_util::CreateCopyResultCallback(&error, &resource_entry));
  message_loop_.RunUntilIdle();

  // The copy should fail as this is not a hosted document.
  EXPECT_EQ(HTTP_NOT_FOUND, error);
  EXPECT_FALSE(resource_entry);
}

TEST_F(FakeDriveServiceTest, CopyHostedDocument_Offline) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "chromeos/gdata/root_feed.json"));
  fake_service_.set_offline(true);

  const std::string kResourceId = "document:5_document_resource_id";
  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceEntry> resource_entry;
  fake_service_.CopyHostedDocument(
      kResourceId,
      "new name",
      test_util::CreateCopyResultCallback(&error, &resource_entry));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(GDATA_NO_CONNECTION, error);
  EXPECT_FALSE(resource_entry);
}

TEST_F(FakeDriveServiceTest, RenameResource_ExistingFile) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "chromeos/gdata/root_feed.json"));
  ASSERT_TRUE(fake_service_.LoadAccountMetadataForWapi(
      "chromeos/gdata/account_metadata.json"));

  int64 old_largest_change_id = GetLargestChangeByAboutResource();

  const std::string kResourceId = "file:2_file_resource_id";

  GDataErrorCode error = GDATA_OTHER_ERROR;
  fake_service_.RenameResource(kResourceId,
                               "new name",
                               test_util::CreateCopyResultCallback(&error));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);

  scoped_ptr<ResourceEntry> resource_entry = FindEntry(kResourceId);
  ASSERT_TRUE(resource_entry);
  EXPECT_EQ("new name", resource_entry->title());
  // Should be incremented as a file was renamed.
  EXPECT_EQ(old_largest_change_id + 1, fake_service_.largest_changestamp());
  EXPECT_EQ(old_largest_change_id + 1, GetLargestChangeByAboutResource());
}

TEST_F(FakeDriveServiceTest, RenameResource_NonexistingFile) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "chromeos/gdata/root_feed.json"));

  const std::string kResourceId = "file:nonexisting_file";

  GDataErrorCode error = GDATA_OTHER_ERROR;
  fake_service_.RenameResource(kResourceId,
                               "new name",
                               test_util::CreateCopyResultCallback(&error));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(HTTP_NOT_FOUND, error);
}

TEST_F(FakeDriveServiceTest, RenameResource_Offline) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "chromeos/gdata/root_feed.json"));
  fake_service_.set_offline(true);

  const std::string kResourceId = "file:2_file_resource_id";

  GDataErrorCode error = GDATA_OTHER_ERROR;
  fake_service_.RenameResource(kResourceId,
                               "new name",
                               test_util::CreateCopyResultCallback(&error));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(GDATA_NO_CONNECTION, error);
}

TEST_F(FakeDriveServiceTest, AddResourceToDirectory_FileInRootDirectory) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "chromeos/gdata/root_feed.json"));
  ASSERT_TRUE(fake_service_.LoadAccountMetadataForWapi(
      "chromeos/gdata/account_metadata.json"));

  int64 old_largest_change_id = GetLargestChangeByAboutResource();

  const std::string kResourceId = "file:2_file_resource_id";
  const std::string kOldParentResourceId = fake_service_.GetRootResourceId();
  const std::string kNewParentResourceId = "folder:1_folder_resource_id";

  // Here's the original parent link.
  EXPECT_TRUE(HasParent(kResourceId, kOldParentResourceId));
  EXPECT_FALSE(HasParent(kResourceId, kNewParentResourceId));

  GDataErrorCode error = GDATA_OTHER_ERROR;
  fake_service_.AddResourceToDirectory(
      kNewParentResourceId,
      kResourceId,
      test_util::CreateCopyResultCallback(&error));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);

  // The parent link should now be changed.
  EXPECT_TRUE(HasParent(kResourceId, kOldParentResourceId));
  EXPECT_TRUE(HasParent(kResourceId, kNewParentResourceId));
  // Should be incremented as a file was moved.
  EXPECT_EQ(old_largest_change_id + 1, fake_service_.largest_changestamp());
  EXPECT_EQ(old_largest_change_id + 1, GetLargestChangeByAboutResource());
}

TEST_F(FakeDriveServiceTest, AddResourceToDirectory_FileInNonRootDirectory) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "chromeos/gdata/root_feed.json"));
  ASSERT_TRUE(fake_service_.LoadAccountMetadataForWapi(
      "chromeos/gdata/account_metadata.json"));

  int64 old_largest_change_id = GetLargestChangeByAboutResource();

  const std::string kResourceId = "file:subdirectory_file_1_id";
  const std::string kOldParentResourceId = "folder:1_folder_resource_id";
  const std::string kNewParentResourceId = "folder:2_folder_resource_id";

  // Here's the original parent link.
  EXPECT_TRUE(HasParent(kResourceId, kOldParentResourceId));
  EXPECT_FALSE(HasParent(kResourceId, kNewParentResourceId));

  GDataErrorCode error = GDATA_OTHER_ERROR;
  fake_service_.AddResourceToDirectory(
      kNewParentResourceId,
      kResourceId,
      test_util::CreateCopyResultCallback(&error));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);

  // The parent link should now be changed.
  EXPECT_TRUE(HasParent(kResourceId, kOldParentResourceId));
  EXPECT_TRUE(HasParent(kResourceId, kNewParentResourceId));
  // Should be incremented as a file was moved.
  EXPECT_EQ(old_largest_change_id + 1, fake_service_.largest_changestamp());
  EXPECT_EQ(old_largest_change_id + 1, GetLargestChangeByAboutResource());
}

TEST_F(FakeDriveServiceTest, AddResourceToDirectory_NonexistingFile) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "chromeos/gdata/root_feed.json"));

  const std::string kResourceId = "file:nonexisting_file";
  const std::string kNewParentResourceId = "folder:1_folder_resource_id";

  GDataErrorCode error = GDATA_OTHER_ERROR;
  fake_service_.AddResourceToDirectory(
      kNewParentResourceId,
      kResourceId,
      test_util::CreateCopyResultCallback(&error));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(HTTP_NOT_FOUND, error);
}

TEST_F(FakeDriveServiceTest, AddResourceToDirectory_OrphanFile) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "chromeos/gdata/root_feed.json"));
  ASSERT_TRUE(fake_service_.LoadAccountMetadataForWapi(
      "chromeos/gdata/account_metadata.json"));

  int64 old_largest_change_id = GetLargestChangeByAboutResource();

  const std::string kResourceId = "file:1_orphanfile_resource_id";
  const std::string kNewParentResourceId = "folder:1_folder_resource_id";

  // The file does not belong to any directory, even to the root.
  EXPECT_FALSE(HasParent(kResourceId, kNewParentResourceId));
  EXPECT_FALSE(HasParent(kResourceId, fake_service_.GetRootResourceId()));

  GDataErrorCode error = GDATA_OTHER_ERROR;
  fake_service_.AddResourceToDirectory(
      kNewParentResourceId,
      kResourceId,
      test_util::CreateCopyResultCallback(&error));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);

  // The parent link should now be changed.
  EXPECT_TRUE(HasParent(kResourceId, kNewParentResourceId));
  EXPECT_FALSE(HasParent(kResourceId, fake_service_.GetRootResourceId()));
  // Should be incremented as a file was moved.
  EXPECT_EQ(old_largest_change_id + 1, fake_service_.largest_changestamp());
  EXPECT_EQ(old_largest_change_id + 1, GetLargestChangeByAboutResource());
}

TEST_F(FakeDriveServiceTest, AddResourceToDirectory_Offline) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "chromeos/gdata/root_feed.json"));
  fake_service_.set_offline(true);

  const std::string kResourceId = "file:2_file_resource_id";
  const std::string kNewParentResourceId = "folder:1_folder_resource_id";

  GDataErrorCode error = GDATA_OTHER_ERROR;
  fake_service_.AddResourceToDirectory(
      kNewParentResourceId,
      kResourceId,
      test_util::CreateCopyResultCallback(&error));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(GDATA_NO_CONNECTION, error);
}

TEST_F(FakeDriveServiceTest, RemoveResourceFromDirectory_ExistingFile) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "chromeos/gdata/root_feed.json"));
  ASSERT_TRUE(fake_service_.LoadAccountMetadataForWapi(
      "chromeos/gdata/account_metadata.json"));

  int64 old_largest_change_id = GetLargestChangeByAboutResource();

  const std::string kResourceId = "file:subdirectory_file_1_id";
  const std::string kParentResourceId = "folder:1_folder_resource_id";

  scoped_ptr<ResourceEntry> resource_entry = FindEntry(kResourceId);
  ASSERT_TRUE(resource_entry);
  // The parent link should exist now.
  const google_apis::Link* parent_link =
      resource_entry->GetLinkByType(Link::LINK_PARENT);
  ASSERT_TRUE(parent_link);

  GDataErrorCode error = GDATA_OTHER_ERROR;
  fake_service_.RemoveResourceFromDirectory(
      kParentResourceId,
      kResourceId,
      test_util::CreateCopyResultCallback(&error));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);

  resource_entry = FindEntry(kResourceId);
  ASSERT_TRUE(resource_entry);
  // The parent link should be gone now.
  parent_link = resource_entry->GetLinkByType(Link::LINK_PARENT);
  ASSERT_FALSE(parent_link);
  // Should be incremented as a file was moved to the root directory.
  EXPECT_EQ(old_largest_change_id + 1, fake_service_.largest_changestamp());
  EXPECT_EQ(old_largest_change_id + 1, GetLargestChangeByAboutResource());
}

TEST_F(FakeDriveServiceTest, RemoveResourceFromDirectory_NonexistingFile) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "chromeos/gdata/root_feed.json"));

  const std::string kResourceId = "file:nonexisting_file";
  const std::string kParentResourceId = "folder:1_folder_resource_id";

  GDataErrorCode error = GDATA_OTHER_ERROR;
  fake_service_.RemoveResourceFromDirectory(
      kParentResourceId,
      kResourceId,
      test_util::CreateCopyResultCallback(&error));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(HTTP_NOT_FOUND, error);
}

TEST_F(FakeDriveServiceTest, RemoveResourceFromDirectory_OrphanFile) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "chromeos/gdata/root_feed.json"));

  const std::string kResourceId = "file:1_orphanfile_resource_id";
  const std::string kParentResourceId = fake_service_.GetRootResourceId();

  GDataErrorCode error = GDATA_OTHER_ERROR;
  fake_service_.RemoveResourceFromDirectory(
      kParentResourceId,
      kResourceId,
      test_util::CreateCopyResultCallback(&error));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(HTTP_NOT_FOUND, error);
}

TEST_F(FakeDriveServiceTest, RemoveResourceFromDirectory_Offline) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "chromeos/gdata/root_feed.json"));
  fake_service_.set_offline(true);

  const std::string kResourceId = "file:subdirectory_file_1_id";
  const std::string kParentResourceId = "folder:1_folder_resource_id";

  GDataErrorCode error = GDATA_OTHER_ERROR;
  fake_service_.RemoveResourceFromDirectory(
      kParentResourceId,
      kResourceId,
      test_util::CreateCopyResultCallback(&error));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(GDATA_NO_CONNECTION, error);
}

TEST_F(FakeDriveServiceTest, AddNewDirectory_ToRootDirectory) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "chromeos/gdata/root_feed.json"));
  ASSERT_TRUE(fake_service_.LoadAccountMetadataForWapi(
      "chromeos/gdata/account_metadata.json"));

  int64 old_largest_change_id = GetLargestChangeByAboutResource();

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceEntry> resource_entry;
  fake_service_.AddNewDirectory(
      fake_service_.GetRootResourceId(),
      "new directory",
      test_util::CreateCopyResultCallback(&error, &resource_entry));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(HTTP_CREATED, error);
  ASSERT_TRUE(resource_entry);
  EXPECT_EQ("resource_id_1", resource_entry->resource_id());
  EXPECT_EQ("new directory", resource_entry->title());
  EXPECT_TRUE(HasParent(resource_entry->resource_id(),
                        fake_service_.GetRootResourceId()));
  // Should be incremented as a new directory was created.
  EXPECT_EQ(old_largest_change_id + 1, fake_service_.largest_changestamp());
  EXPECT_EQ(old_largest_change_id + 1, GetLargestChangeByAboutResource());
}

TEST_F(FakeDriveServiceTest, AddNewDirectory_ToRootDirectoryOnEmptyFileSystem) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "chromeos/gdata/empty_feed.json"));
  ASSERT_TRUE(fake_service_.LoadAccountMetadataForWapi(
      "chromeos/gdata/account_metadata.json"));

  int64 old_largest_change_id = GetLargestChangeByAboutResource();

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceEntry> resource_entry;
  fake_service_.AddNewDirectory(
      fake_service_.GetRootResourceId(),
      "new directory",
      test_util::CreateCopyResultCallback(&error, &resource_entry));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(HTTP_CREATED, error);
  ASSERT_TRUE(resource_entry);
  EXPECT_EQ("resource_id_1", resource_entry->resource_id());
  EXPECT_EQ("new directory", resource_entry->title());
  EXPECT_TRUE(HasParent(resource_entry->resource_id(),
                        fake_service_.GetRootResourceId()));
  // Should be incremented as a new directory was created.
  EXPECT_EQ(old_largest_change_id + 1, fake_service_.largest_changestamp());
  EXPECT_EQ(old_largest_change_id + 1, GetLargestChangeByAboutResource());
}

TEST_F(FakeDriveServiceTest, AddNewDirectory_ToNonRootDirectory) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "chromeos/gdata/root_feed.json"));
  ASSERT_TRUE(fake_service_.LoadAccountMetadataForWapi(
      "chromeos/gdata/account_metadata.json"));

  int64 old_largest_change_id = GetLargestChangeByAboutResource();

  const std::string kParentResourceId = "folder:1_folder_resource_id";

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceEntry> resource_entry;
  fake_service_.AddNewDirectory(
      kParentResourceId,
      "new directory",
      test_util::CreateCopyResultCallback(&error, &resource_entry));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(HTTP_CREATED, error);
  ASSERT_TRUE(resource_entry);
  EXPECT_EQ("resource_id_1", resource_entry->resource_id());
  EXPECT_EQ("new directory", resource_entry->title());
  EXPECT_TRUE(HasParent(resource_entry->resource_id(), kParentResourceId));
  // Should be incremented as a new directory was created.
  EXPECT_EQ(old_largest_change_id + 1, fake_service_.largest_changestamp());
  EXPECT_EQ(old_largest_change_id + 1, GetLargestChangeByAboutResource());
}

TEST_F(FakeDriveServiceTest, AddNewDirectory_ToNonexistingDirectory) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "chromeos/gdata/root_feed.json"));

  const std::string kParentResourceId = "folder:nonexisting_resource_id";

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceEntry> resource_entry;
  fake_service_.AddNewDirectory(
      kParentResourceId,
      "new directory",
      test_util::CreateCopyResultCallback(&error, &resource_entry));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(HTTP_NOT_FOUND, error);
  EXPECT_FALSE(resource_entry);
}

TEST_F(FakeDriveServiceTest, AddNewDirectory_Offline) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "chromeos/gdata/root_feed.json"));
  fake_service_.set_offline(true);

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceEntry> resource_entry;
  fake_service_.AddNewDirectory(
      fake_service_.GetRootResourceId(),
      "new directory",
      test_util::CreateCopyResultCallback(&error, &resource_entry));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(GDATA_NO_CONNECTION, error);
  EXPECT_FALSE(resource_entry);
}

TEST_F(FakeDriveServiceTest, InitiateUploadNewFile_Offline) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "chromeos/gdata/root_feed.json"));
  fake_service_.set_offline(true);

  GDataErrorCode error = GDATA_OTHER_ERROR;
  GURL upload_location;
  fake_service_.InitiateUploadNewFile(
      base::FilePath(FILE_PATH_LITERAL("drive/Directory 1")),
      "test/foo",
      13,
      "folder:1_folder_resource_id",
      "new file.foo",
      test_util::CreateCopyResultCallback(&error, &upload_location));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(GDATA_NO_CONNECTION, error);
  EXPECT_TRUE(upload_location.is_empty());
}

TEST_F(FakeDriveServiceTest, InitiateUploadNewFile_NotFound) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "chromeos/gdata/root_feed.json"));

  GDataErrorCode error = GDATA_OTHER_ERROR;
  GURL upload_location;
  fake_service_.InitiateUploadNewFile(
      base::FilePath(FILE_PATH_LITERAL("drive/Directory 1")),
      "test/foo",
      13,
      "non_existent",
      "new file.foo",
      test_util::CreateCopyResultCallback(&error, &upload_location));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(HTTP_NOT_FOUND, error);
  EXPECT_TRUE(upload_location.is_empty());
}

TEST_F(FakeDriveServiceTest, InitiateUploadNewFile) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "chromeos/gdata/root_feed.json"));

  GDataErrorCode error = GDATA_OTHER_ERROR;
  GURL upload_location;
  fake_service_.InitiateUploadNewFile(
      base::FilePath(FILE_PATH_LITERAL("drive/Directory 1")),
      "test/foo",
      13,
      "folder:1_folder_resource_id",
      "new file.foo",
      test_util::CreateCopyResultCallback(&error, &upload_location));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_FALSE(upload_location.is_empty());
  EXPECT_NE(GURL("https://1_folder_resumable_create_media_link"),
            upload_location);
}

TEST_F(FakeDriveServiceTest, InitiateUploadExistingFile_Offline) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "chromeos/gdata/root_feed.json"));
  fake_service_.set_offline(true);

  GDataErrorCode error = GDATA_OTHER_ERROR;
  GURL upload_location;
  fake_service_.InitiateUploadExistingFile(
      base::FilePath(FILE_PATH_LITERAL("drive/File 1")),
      "test/foo",
      13,
      "file:2_file_resource_id",
      "",  // etag
      test_util::CreateCopyResultCallback(&error, &upload_location));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(GDATA_NO_CONNECTION, error);
  EXPECT_TRUE(upload_location.is_empty());
}

TEST_F(FakeDriveServiceTest, InitiateUploadExistingFile_NotFound) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "chromeos/gdata/root_feed.json"));

  GDataErrorCode error = GDATA_OTHER_ERROR;
  GURL upload_location;
  fake_service_.InitiateUploadExistingFile(
      base::FilePath(FILE_PATH_LITERAL("drive/File 1")),
      "test/foo",
      13,
      "non_existent",
      "",  // etag
      test_util::CreateCopyResultCallback(&error, &upload_location));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(HTTP_NOT_FOUND, error);
  EXPECT_TRUE(upload_location.is_empty());
}

TEST_F(FakeDriveServiceTest, InitiateUploadExistingFile_WrongETag) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "chromeos/gdata/root_feed.json"));

  GDataErrorCode error = GDATA_OTHER_ERROR;
  GURL upload_location;
  fake_service_.InitiateUploadExistingFile(
      base::FilePath(FILE_PATH_LITERAL("drive/File 1.txt")),
      "text/plain",
      13,
      "file:2_file_resource_id",
      "invalid_etag",
      test_util::CreateCopyResultCallback(&error, &upload_location));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(HTTP_PRECONDITION, error);
  EXPECT_TRUE(upload_location.is_empty());
}

TEST_F(FakeDriveServiceTest, InitiateUpload_ExistingFile) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "chromeos/gdata/root_feed.json"));

  GDataErrorCode error = GDATA_OTHER_ERROR;
  GURL upload_location;
  fake_service_.InitiateUploadExistingFile(
      base::FilePath(FILE_PATH_LITERAL("drive/File 1.txt")),
      "text/plain",
      13,
      "file:2_file_resource_id",
      "\"HhMOFgxXHit7ImBr\"",
      test_util::CreateCopyResultCallback(&error, &upload_location));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_EQ(GURL("https://2_file_link_resumable_create_media"),
            upload_location);
}

TEST_F(FakeDriveServiceTest, ResumeUpload_Offline) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "chromeos/gdata/root_feed.json"));

  GDataErrorCode error = GDATA_OTHER_ERROR;
  GURL upload_location;
  fake_service_.InitiateUploadNewFile(
      base::FilePath(FILE_PATH_LITERAL("drive/Directory 1/new file.foo")),
      "test/foo",
      15,
      "folder:1_folder_resource_id",
      "new file.foo",
      test_util::CreateCopyResultCallback(&error, &upload_location));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_FALSE(upload_location.is_empty());
  EXPECT_NE(GURL("https://1_folder_resumable_create_media_link"),
            upload_location);

  fake_service_.set_offline(true);

  UploadRangeResponse response;
  scoped_ptr<ResourceEntry> entry;
  fake_service_.ResumeUpload(
      UPLOAD_NEW_FILE,
      base::FilePath(FILE_PATH_LITERAL("drive/Directory 1/new file.foo")),
      upload_location,
      0, 13, 15, "test/foo",
      scoped_refptr<net::IOBuffer>(),
      test_util::CreateCopyResultCallback(&response, &entry));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(GDATA_NO_CONNECTION, response.code);
  EXPECT_FALSE(entry.get());
}

TEST_F(FakeDriveServiceTest, ResumeUpload_NotFound) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "chromeos/gdata/root_feed.json"));

  GDataErrorCode error = GDATA_OTHER_ERROR;
  GURL upload_location;
  fake_service_.InitiateUploadNewFile(
      base::FilePath(FILE_PATH_LITERAL("drive/Directory 1/new file.foo")),
      "test/foo",
      15,
      "folder:1_folder_resource_id",
      "new file.foo",
      test_util::CreateCopyResultCallback(&error, &upload_location));
  message_loop_.RunUntilIdle();

  ASSERT_EQ(HTTP_SUCCESS, error);

  UploadRangeResponse response;
  scoped_ptr<ResourceEntry> entry;
  fake_service_.ResumeUpload(
      UPLOAD_NEW_FILE,
      base::FilePath(FILE_PATH_LITERAL("drive/Directory 1/new file.foo")),
      GURL("https://foo.com/"),
      0, 13, 15, "test/foo",
      scoped_refptr<net::IOBuffer>(),
      test_util::CreateCopyResultCallback(&response, &entry));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(HTTP_NOT_FOUND, response.code);
  EXPECT_FALSE(entry.get());
}

TEST_F(FakeDriveServiceTest, ResumeUpload_ExistingFile) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "chromeos/gdata/root_feed.json"));

  GDataErrorCode error = GDATA_OTHER_ERROR;
  GURL upload_location;
  fake_service_.InitiateUploadExistingFile(
      base::FilePath(FILE_PATH_LITERAL("drive/File 1.txt")),
      "text/plain",
      15,
      "file:2_file_resource_id",
      "\"HhMOFgxXHit7ImBr\"",
      test_util::CreateCopyResultCallback(&error, &upload_location));
  message_loop_.RunUntilIdle();

  ASSERT_EQ(HTTP_SUCCESS, error);

  UploadRangeResponse response;
  scoped_ptr<ResourceEntry> entry;
  fake_service_.ResumeUpload(
      UPLOAD_EXISTING_FILE,
      base::FilePath(FILE_PATH_LITERAL("drive/File 1.txt")),
      upload_location,
      0, 13, 15, "text/plain",
      scoped_refptr<net::IOBuffer>(),
      test_util::CreateCopyResultCallback(&response, &entry));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(HTTP_RESUME_INCOMPLETE, response.code);
  EXPECT_FALSE(entry.get());

  fake_service_.ResumeUpload(
      UPLOAD_EXISTING_FILE,
      base::FilePath(FILE_PATH_LITERAL("drive/File 1.txt")),
      upload_location,
      14, 15, 15, "text/plain",
      scoped_refptr<net::IOBuffer>(),
      test_util::CreateCopyResultCallback(&response, &entry));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, response.code);
  EXPECT_TRUE(entry.get());
  EXPECT_EQ(15L, entry->file_size());
  EXPECT_TRUE(Exists(entry->resource_id()));
}

TEST_F(FakeDriveServiceTest, ResumeUpload_NewFile) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "chromeos/gdata/root_feed.json"));

  GDataErrorCode error = GDATA_OTHER_ERROR;
  GURL upload_location;
  fake_service_.InitiateUploadNewFile(
      base::FilePath(FILE_PATH_LITERAL("drive/Directory 1/new file.foo")),
      "test/foo",
      15,
      "folder:1_folder_resource_id",
      "new file.foo",
      test_util::CreateCopyResultCallback(&error, &upload_location));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_FALSE(upload_location.is_empty());
  EXPECT_NE(GURL("https://1_folder_resumable_create_media_link"),
            upload_location);

  UploadRangeResponse response;
  scoped_ptr<ResourceEntry> entry;
  fake_service_.ResumeUpload(
      UPLOAD_NEW_FILE,
      base::FilePath(FILE_PATH_LITERAL("drive/Directory 1/new file.foo")),
      upload_location,
      0, 13, 15, "test/foo",
      scoped_refptr<net::IOBuffer>(),
      test_util::CreateCopyResultCallback(&response, &entry));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(HTTP_RESUME_INCOMPLETE, response.code);
  EXPECT_FALSE(entry.get());

  fake_service_.ResumeUpload(
      UPLOAD_NEW_FILE,
      base::FilePath(FILE_PATH_LITERAL("drive/Directory 1/new file.foo")),
      upload_location,
      14, 15, 15, "test/foo",
      scoped_refptr<net::IOBuffer>(),
      test_util::CreateCopyResultCallback(&response, &entry));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(HTTP_CREATED, response.code);
  EXPECT_TRUE(entry.get());
  EXPECT_EQ(15L, entry->file_size());
  EXPECT_TRUE(Exists(entry->resource_id()));
}

}  // namespace

}  // namespace google_apis
