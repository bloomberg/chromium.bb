// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/drive/fake_drive_service.h"

#include <string>
#include <vector>

#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/md5.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "google_apis/drive/drive_api_parser.h"
#include "google_apis/drive/gdata_wapi_parser.h"
#include "google_apis/drive/gdata_wapi_requests.h"
#include "google_apis/drive/test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using google_apis::AboutResource;
using google_apis::AppList;
using google_apis::GDataErrorCode;
using google_apis::GDATA_NO_CONNECTION;
using google_apis::GDATA_OTHER_ERROR;
using google_apis::GetContentCallback;
using google_apis::HTTP_CREATED;
using google_apis::HTTP_NOT_FOUND;
using google_apis::HTTP_NO_CONTENT;
using google_apis::HTTP_PRECONDITION;
using google_apis::HTTP_RESUME_INCOMPLETE;
using google_apis::HTTP_SUCCESS;
using google_apis::Link;
using google_apis::ProgressCallback;
using google_apis::ResourceEntry;
using google_apis::ResourceList;
using google_apis::UploadRangeResponse;
namespace test_util = google_apis::test_util;

namespace drive {

namespace {

class FakeDriveServiceTest : public testing::Test {
 protected:
  // Returns the resource entry that matches |resource_id|.
  scoped_ptr<ResourceEntry> FindEntry(const std::string& resource_id) {
    GDataErrorCode error = GDATA_OTHER_ERROR;
    scoped_ptr<ResourceEntry> resource_entry;
    fake_service_.GetResourceEntry(
        resource_id,
        test_util::CreateCopyResultCallback(&error, &resource_entry));
    base::RunLoop().RunUntilIdle();
    return resource_entry.Pass();
  }

  // Returns true if the resource identified by |resource_id| exists.
  bool Exists(const std::string& resource_id) {
    scoped_ptr<ResourceEntry> resource_entry = FindEntry(resource_id);
    return resource_entry && !resource_entry->deleted();
  }

  // Adds a new directory at |parent_resource_id| with the given name.
  // Returns true on success.
  bool AddNewDirectory(const std::string& parent_resource_id,
                       const std::string& directory_title) {
    GDataErrorCode error = GDATA_OTHER_ERROR;
    scoped_ptr<ResourceEntry> resource_entry;
    fake_service_.AddNewDirectory(
        parent_resource_id,
        directory_title,
        DriveServiceInterface::AddNewDirectoryOptions(),
        test_util::CreateCopyResultCallback(&error, &resource_entry));
    base::RunLoop().RunUntilIdle();
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
    base::RunLoop().RunUntilIdle();
    return about_resource->largest_change_id();
  }

  content::TestBrowserThreadBundle thread_bundle_;
  FakeDriveService fake_service_;
};

TEST_F(FakeDriveServiceTest, GetAllResourceList) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "gdata/root_feed.json"));

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceList> resource_list;
  fake_service_.GetAllResourceList(
      test_util::CreateCopyResultCallback(&error, &resource_list));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);
  ASSERT_TRUE(resource_list);
  // Do some sanity check.
  EXPECT_EQ(15U, resource_list->entries().size());
  EXPECT_EQ(1, fake_service_.resource_list_load_count());
}

TEST_F(FakeDriveServiceTest, GetAllResourceList_Offline) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "gdata/root_feed.json"));
  fake_service_.set_offline(true);

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceList> resource_list;
  fake_service_.GetAllResourceList(
      test_util::CreateCopyResultCallback(&error, &resource_list));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(GDATA_NO_CONNECTION, error);
  EXPECT_FALSE(resource_list);
}

TEST_F(FakeDriveServiceTest, GetResourceListInDirectory_InRootDirectory) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "gdata/root_feed.json"));

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceList> resource_list;
  fake_service_.GetResourceListInDirectory(
      fake_service_.GetRootResourceId(),
      test_util::CreateCopyResultCallback(&error, &resource_list));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);
  ASSERT_TRUE(resource_list);
  // Do some sanity check. There are 8 entries in the root directory.
  EXPECT_EQ(8U, resource_list->entries().size());
  EXPECT_EQ(1, fake_service_.directory_load_count());
}

TEST_F(FakeDriveServiceTest, GetResourceListInDirectory_InNonRootDirectory) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "gdata/root_feed.json"));

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceList> resource_list;
  fake_service_.GetResourceListInDirectory(
      "folder:1_folder_resource_id",
      test_util::CreateCopyResultCallback(&error, &resource_list));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);
  ASSERT_TRUE(resource_list);
  // Do some sanity check. There is three entries in 1_folder_resource_id
  // directory.
  EXPECT_EQ(3U, resource_list->entries().size());
  EXPECT_EQ(1, fake_service_.directory_load_count());
}

TEST_F(FakeDriveServiceTest, GetResourceListInDirectory_Offline) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "gdata/root_feed.json"));
  fake_service_.set_offline(true);

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceList> resource_list;
  fake_service_.GetResourceListInDirectory(
      fake_service_.GetRootResourceId(),
      test_util::CreateCopyResultCallback(&error, &resource_list));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(GDATA_NO_CONNECTION, error);
  EXPECT_FALSE(resource_list);
}

TEST_F(FakeDriveServiceTest, Search) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "gdata/root_feed.json"));

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceList> resource_list;
  fake_service_.Search(
      "File",  // search_query
      test_util::CreateCopyResultCallback(&error, &resource_list));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);
  ASSERT_TRUE(resource_list);
  // Do some sanity check. There are 4 entries that contain "File" in their
  // titles.
  EXPECT_EQ(4U, resource_list->entries().size());
}

TEST_F(FakeDriveServiceTest, Search_WithAttribute) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "gdata/root_feed.json"));

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceList> resource_list;
  fake_service_.Search(
      "title:1.txt",  // search_query
      test_util::CreateCopyResultCallback(&error, &resource_list));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);
  ASSERT_TRUE(resource_list);
  // Do some sanity check. There are 4 entries that contain "1.txt" in their
  // titles.
  EXPECT_EQ(4U, resource_list->entries().size());
}

TEST_F(FakeDriveServiceTest, Search_MultipleQueries) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "gdata/root_feed.json"));

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceList> resource_list;
  fake_service_.Search(
      "Directory 1",  // search_query
      test_util::CreateCopyResultCallback(&error, &resource_list));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);
  ASSERT_TRUE(resource_list);
  // There are 2 entries that contain both "Directory" and "1" in their titles.
  EXPECT_EQ(2U, resource_list->entries().size());

  fake_service_.Search(
      "\"Directory 1\"",  // search_query
      test_util::CreateCopyResultCallback(&error, &resource_list));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);
  ASSERT_TRUE(resource_list);
  // There is 1 entry that contain "Directory 1" in its title.
  EXPECT_EQ(1U, resource_list->entries().size());
}

TEST_F(FakeDriveServiceTest, Search_Offline) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "gdata/root_feed.json"));
  fake_service_.set_offline(true);

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceList> resource_list;
  fake_service_.Search(
      "Directory 1",  // search_query
      test_util::CreateCopyResultCallback(&error, &resource_list));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(GDATA_NO_CONNECTION, error);
  EXPECT_FALSE(resource_list);
}

TEST_F(FakeDriveServiceTest, Search_Deleted) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "gdata/root_feed.json"));

  GDataErrorCode error = GDATA_OTHER_ERROR;
  fake_service_.DeleteResource("file:2_file_resource_id",
                               std::string(),  // etag
                               test_util::CreateCopyResultCallback(&error));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(HTTP_NO_CONTENT, error);

  error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceList> resource_list;
  fake_service_.Search(
      "File",  // search_query
      test_util::CreateCopyResultCallback(&error, &resource_list));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);
  ASSERT_TRUE(resource_list);
  // Do some sanity check. There are 4 entries that contain "File" in their
  // titles and one of them is deleted.
  EXPECT_EQ(3U, resource_list->entries().size());
}

TEST_F(FakeDriveServiceTest, Search_Trashed) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "gdata/root_feed.json"));

  GDataErrorCode error = GDATA_OTHER_ERROR;
  fake_service_.TrashResource("file:2_file_resource_id",
                              test_util::CreateCopyResultCallback(&error));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(HTTP_SUCCESS, error);

  error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceList> resource_list;
  fake_service_.Search(
      "File",  // search_query
      test_util::CreateCopyResultCallback(&error, &resource_list));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);
  ASSERT_TRUE(resource_list);
  // Do some sanity check. There are 4 entries that contain "File" in their
  // titles and one of them is deleted.
  EXPECT_EQ(3U, resource_list->entries().size());
}

TEST_F(FakeDriveServiceTest, SearchByTitle) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "gdata/root_feed.json"));

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceList> resource_list;
  fake_service_.SearchByTitle(
      "1.txt",  // title
      fake_service_.GetRootResourceId(),  // directory_resource_id
      test_util::CreateCopyResultCallback(&error, &resource_list));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);
  ASSERT_TRUE(resource_list);
  // Do some sanity check. There are 2 entries that contain "1.txt" in their
  // titles directly under the root directory.
  EXPECT_EQ(2U, resource_list->entries().size());
}

TEST_F(FakeDriveServiceTest, SearchByTitle_EmptyDirectoryResourceId) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "gdata/root_feed.json"));

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceList> resource_list;
  fake_service_.SearchByTitle(
      "1.txt",  // title
      "",  // directory resource id
      test_util::CreateCopyResultCallback(&error, &resource_list));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);
  ASSERT_TRUE(resource_list);
  // Do some sanity check. There are 4 entries that contain "1.txt" in their
  // titles.
  EXPECT_EQ(4U, resource_list->entries().size());
}

TEST_F(FakeDriveServiceTest, SearchByTitle_Offline) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "gdata/root_feed.json"));
  fake_service_.set_offline(true);

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceList> resource_list;
  fake_service_.SearchByTitle(
      "Directory 1",  // title
      fake_service_.GetRootResourceId(),  // directory_resource_id
      test_util::CreateCopyResultCallback(&error, &resource_list));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(GDATA_NO_CONNECTION, error);
  EXPECT_FALSE(resource_list);
}

TEST_F(FakeDriveServiceTest, GetChangeList_NoNewEntries) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "gdata/root_feed.json"));

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceList> resource_list;
  fake_service_.GetChangeList(
      fake_service_.about_resource().largest_change_id() + 1,
      test_util::CreateCopyResultCallback(&error, &resource_list));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);
  ASSERT_TRUE(resource_list);
  EXPECT_EQ(fake_service_.about_resource().largest_change_id(),
            resource_list->largest_changestamp());
  // This should be empty as the latest changestamp was passed to
  // GetResourceList(), hence there should be no new entries.
  EXPECT_EQ(0U, resource_list->entries().size());
  // It's considered loaded even if the result is empty.
  EXPECT_EQ(1, fake_service_.change_list_load_count());
}

TEST_F(FakeDriveServiceTest, GetChangeList_WithNewEntry) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "gdata/root_feed.json"));
  const int64 old_largest_change_id =
      fake_service_.about_resource().largest_change_id();

  // Add a new directory in the root directory.
  ASSERT_TRUE(AddNewDirectory(
      fake_service_.GetRootResourceId(), "new directory"));

  // Get the resource list newer than old_largest_change_id.
  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceList> resource_list;
  fake_service_.GetChangeList(
      old_largest_change_id + 1,
      test_util::CreateCopyResultCallback(&error, &resource_list));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);
  ASSERT_TRUE(resource_list);
  EXPECT_EQ(fake_service_.about_resource().largest_change_id(),
            resource_list->largest_changestamp());
  // The result should only contain the newly created directory.
  ASSERT_EQ(1U, resource_list->entries().size());
  EXPECT_EQ("new directory", resource_list->entries()[0]->title());
  EXPECT_EQ(1, fake_service_.change_list_load_count());
}

TEST_F(FakeDriveServiceTest, GetChangeList_Offline) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "gdata/root_feed.json"));
  fake_service_.set_offline(true);

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceList> resource_list;
  fake_service_.GetChangeList(
      654321,  // start_changestamp
      test_util::CreateCopyResultCallback(&error, &resource_list));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(GDATA_NO_CONNECTION, error);
  EXPECT_FALSE(resource_list);
}

TEST_F(FakeDriveServiceTest, GetChangeList_DeletedEntry) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "gdata/root_feed.json"));
  ASSERT_TRUE(Exists("file:2_file_resource_id"));
  const int64 old_largest_change_id =
      fake_service_.about_resource().largest_change_id();

  GDataErrorCode error = GDATA_OTHER_ERROR;
  fake_service_.DeleteResource("file:2_file_resource_id",
                               std::string(),  // etag
                               test_util::CreateCopyResultCallback(&error));
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(HTTP_NO_CONTENT, error);
  ASSERT_FALSE(Exists("file:2_file_resource_id"));

  // Get the resource list newer than old_largest_change_id.
  error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceList> resource_list;
  fake_service_.GetChangeList(
      old_largest_change_id + 1,
      test_util::CreateCopyResultCallback(&error, &resource_list));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);
  ASSERT_TRUE(resource_list);
  EXPECT_EQ(fake_service_.about_resource().largest_change_id(),
            resource_list->largest_changestamp());
  // The result should only contain the deleted file.
  ASSERT_EQ(1U, resource_list->entries().size());
  const ResourceEntry& entry = *resource_list->entries()[0];
  EXPECT_EQ("file:2_file_resource_id", entry.resource_id());
  EXPECT_TRUE(entry.title().empty());
  EXPECT_TRUE(entry.deleted());
  EXPECT_EQ(1, fake_service_.change_list_load_count());
}

TEST_F(FakeDriveServiceTest, GetChangeList_TrashedEntry) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "gdata/root_feed.json"));
  ASSERT_TRUE(Exists("file:2_file_resource_id"));
  const int64 old_largest_change_id =
      fake_service_.about_resource().largest_change_id();

  GDataErrorCode error = GDATA_OTHER_ERROR;
  fake_service_.TrashResource("file:2_file_resource_id",
                              test_util::CreateCopyResultCallback(&error));
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(HTTP_SUCCESS, error);
  ASSERT_FALSE(Exists("file:2_file_resource_id"));

  // Get the resource list newer than old_largest_change_id.
  error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceList> resource_list;
  fake_service_.GetChangeList(
      old_largest_change_id + 1,
      test_util::CreateCopyResultCallback(&error, &resource_list));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);
  ASSERT_TRUE(resource_list);
  EXPECT_EQ(fake_service_.about_resource().largest_change_id(),
            resource_list->largest_changestamp());
  // The result should only contain the trashed file.
  ASSERT_EQ(1U, resource_list->entries().size());
  const ResourceEntry& entry = *resource_list->entries()[0];
  EXPECT_EQ("file:2_file_resource_id", entry.resource_id());
  EXPECT_TRUE(entry.deleted());
  EXPECT_EQ(1, fake_service_.change_list_load_count());
}

TEST_F(FakeDriveServiceTest, GetRemainingChangeList_GetAllResourceList) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "gdata/root_feed.json"));
  fake_service_.set_default_max_results(6);

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceList> resource_list;
  fake_service_.GetAllResourceList(
      test_util::CreateCopyResultCallback(&error, &resource_list));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(HTTP_SUCCESS, error);
  ASSERT_TRUE(resource_list);

  // Do some sanity check.
  // The number of results is 14 entries. Thus, it should split into three
  // chunks: 6, 6, and then 2.
  EXPECT_EQ(6U, resource_list->entries().size());
  EXPECT_EQ(1, fake_service_.resource_list_load_count());

  // Second page loading.
  const google_apis::Link* next_link =
      resource_list->GetLinkByType(Link::LINK_NEXT);
  ASSERT_TRUE(next_link);
  // Keep the next url before releasing the |resource_list|.
  GURL next_url(next_link->href());

  error = GDATA_OTHER_ERROR;
  resource_list.reset();
  fake_service_.GetRemainingChangeList(
      next_url,
      test_util::CreateCopyResultCallback(&error, &resource_list));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);
  ASSERT_TRUE(resource_list);

  EXPECT_EQ(6U, resource_list->entries().size());
  EXPECT_EQ(1, fake_service_.resource_list_load_count());

  // Third page loading.
  next_link = resource_list->GetLinkByType(Link::LINK_NEXT);
  ASSERT_TRUE(next_link);
  next_url = GURL(next_link->href());

  error = GDATA_OTHER_ERROR;
  resource_list.reset();
  fake_service_.GetRemainingChangeList(
      next_url,
      test_util::CreateCopyResultCallback(&error, &resource_list));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);
  ASSERT_TRUE(resource_list);

  EXPECT_EQ(3U, resource_list->entries().size());
  EXPECT_EQ(1, fake_service_.resource_list_load_count());
}

TEST_F(FakeDriveServiceTest,
       GetRemainingFileList_GetResourceListInDirectory) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "gdata/root_feed.json"));
  fake_service_.set_default_max_results(3);

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceList> resource_list;
  fake_service_.GetResourceListInDirectory(
      fake_service_.GetRootResourceId(),
      test_util::CreateCopyResultCallback(&error, &resource_list));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(HTTP_SUCCESS, error);
  ASSERT_TRUE(resource_list);

  // Do some sanity check.
  // The number of results is 8 entries. Thus, it should split into three
  // chunks: 3, 3, and then 2.
  EXPECT_EQ(3U, resource_list->entries().size());
  EXPECT_EQ(1, fake_service_.directory_load_count());

  // Second page loading.
  const google_apis::Link* next_link =
      resource_list->GetLinkByType(Link::LINK_NEXT);
  ASSERT_TRUE(next_link);
  // Keep the next url before releasing the |resource_list|.
  GURL next_url(next_link->href());

  error = GDATA_OTHER_ERROR;
  resource_list.reset();
  fake_service_.GetRemainingFileList(
      next_url,
      test_util::CreateCopyResultCallback(&error, &resource_list));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);
  ASSERT_TRUE(resource_list);

  EXPECT_EQ(3U, resource_list->entries().size());
  EXPECT_EQ(1, fake_service_.directory_load_count());

  // Third page loading.
  next_link = resource_list->GetLinkByType(Link::LINK_NEXT);
  ASSERT_TRUE(next_link);
  next_url = GURL(next_link->href());

  error = GDATA_OTHER_ERROR;
  resource_list.reset();
  fake_service_.GetRemainingFileList(
      next_url,
      test_util::CreateCopyResultCallback(&error, &resource_list));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);
  ASSERT_TRUE(resource_list);

  EXPECT_EQ(2U, resource_list->entries().size());
  EXPECT_EQ(1, fake_service_.directory_load_count());
}

TEST_F(FakeDriveServiceTest, GetRemainingFileList_Search) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "gdata/root_feed.json"));
  fake_service_.set_default_max_results(2);

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceList> resource_list;
  fake_service_.Search(
      "File",  // search_query
      test_util::CreateCopyResultCallback(&error, &resource_list));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(HTTP_SUCCESS, error);
  ASSERT_TRUE(resource_list);

  // Do some sanity check.
  // The number of results is 4 entries. Thus, it should split into two
  // chunks: 2, and then 2
  EXPECT_EQ(2U, resource_list->entries().size());

  // Second page loading.
  const google_apis::Link* next_link =
      resource_list->GetLinkByType(Link::LINK_NEXT);
  ASSERT_TRUE(next_link);
  // Keep the next url before releasing the |resource_list|.
  GURL next_url(next_link->href());

  error = GDATA_OTHER_ERROR;
  resource_list.reset();
  fake_service_.GetRemainingFileList(
      next_url,
      test_util::CreateCopyResultCallback(&error, &resource_list));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);
  ASSERT_TRUE(resource_list);

  EXPECT_EQ(2U, resource_list->entries().size());
}

TEST_F(FakeDriveServiceTest, GetRemainingChangeList_GetChangeList) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "gdata/root_feed.json"));
  fake_service_.set_default_max_results(2);
  const int64 old_largest_change_id =
      fake_service_.about_resource().largest_change_id();

  // Add 5 new directory in the root directory.
  for (int i = 0; i < 5; ++i) {
    ASSERT_TRUE(AddNewDirectory(
        fake_service_.GetRootResourceId(),
        base::StringPrintf("new directory %d", i)));
  }

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceList> resource_list;
  fake_service_.GetChangeList(
      old_largest_change_id + 1,  // start_changestamp
      test_util::CreateCopyResultCallback(&error, &resource_list));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(HTTP_SUCCESS, error);
  ASSERT_TRUE(resource_list);

  // Do some sanity check.
  // The number of results is 5 entries. Thus, it should split into three
  // chunks: 2, 2 and then 1.
  EXPECT_EQ(2U, resource_list->entries().size());
  EXPECT_EQ(1, fake_service_.change_list_load_count());

  // Second page loading.
  const google_apis::Link* next_link =
      resource_list->GetLinkByType(Link::LINK_NEXT);
  ASSERT_TRUE(next_link);
  // Keep the next url before releasing the |resource_list|.
  GURL next_url(next_link->href());

  error = GDATA_OTHER_ERROR;
  resource_list.reset();
  fake_service_.GetRemainingChangeList(
      next_url,
      test_util::CreateCopyResultCallback(&error, &resource_list));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);
  ASSERT_TRUE(resource_list);

  EXPECT_EQ(2U, resource_list->entries().size());
  EXPECT_EQ(1, fake_service_.change_list_load_count());

  // Third page loading.
  next_link = resource_list->GetLinkByType(Link::LINK_NEXT);
  ASSERT_TRUE(next_link);
  next_url = GURL(next_link->href());

  error = GDATA_OTHER_ERROR;
  resource_list.reset();
  fake_service_.GetRemainingChangeList(
      next_url,
      test_util::CreateCopyResultCallback(&error, &resource_list));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);
  ASSERT_TRUE(resource_list);

  EXPECT_EQ(1U, resource_list->entries().size());
  EXPECT_EQ(1, fake_service_.change_list_load_count());
}

TEST_F(FakeDriveServiceTest, GetAboutResource) {
  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<AboutResource> about_resource;
  fake_service_.GetAboutResource(
      test_util::CreateCopyResultCallback(&error, &about_resource));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);

  ASSERT_TRUE(about_resource);
  // Do some sanity check.
  EXPECT_EQ(fake_service_.GetRootResourceId(),
            about_resource->root_folder_id());
  EXPECT_EQ(1, fake_service_.about_resource_load_count());
}

TEST_F(FakeDriveServiceTest, GetAboutResource_Offline) {
  fake_service_.set_offline(true);

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<AboutResource> about_resource;
  fake_service_.GetAboutResource(
      test_util::CreateCopyResultCallback(&error, &about_resource));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(GDATA_NO_CONNECTION, error);
  EXPECT_FALSE(about_resource);
}

TEST_F(FakeDriveServiceTest, GetAppList) {
  ASSERT_TRUE(fake_service_.LoadAppListForDriveApi(
      "drive/applist.json"));

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<AppList> app_list;
  fake_service_.GetAppList(
      test_util::CreateCopyResultCallback(&error, &app_list));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);

  ASSERT_TRUE(app_list);
  EXPECT_EQ(1, fake_service_.app_list_load_count());
}

TEST_F(FakeDriveServiceTest, GetAppList_Offline) {
  ASSERT_TRUE(fake_service_.LoadAppListForDriveApi(
      "drive/applist.json"));
  fake_service_.set_offline(true);

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<AppList> app_list;
  fake_service_.GetAppList(
      test_util::CreateCopyResultCallback(&error, &app_list));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(GDATA_NO_CONNECTION, error);
  EXPECT_FALSE(app_list);
}

TEST_F(FakeDriveServiceTest, GetResourceEntry_ExistingFile) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "gdata/root_feed.json"));

  const std::string kResourceId = "file:2_file_resource_id";
  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceEntry> resource_entry;
  fake_service_.GetResourceEntry(
      kResourceId,
      test_util::CreateCopyResultCallback(&error, &resource_entry));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);
  ASSERT_TRUE(resource_entry);
  // Do some sanity check.
  EXPECT_EQ(kResourceId, resource_entry->resource_id());
}

TEST_F(FakeDriveServiceTest, GetResourceEntry_NonexistingFile) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "gdata/root_feed.json"));

  const std::string kResourceId = "file:nonexisting_resource_id";
  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceEntry> resource_entry;
  fake_service_.GetResourceEntry(
      kResourceId,
      test_util::CreateCopyResultCallback(&error, &resource_entry));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(HTTP_NOT_FOUND, error);
  ASSERT_FALSE(resource_entry);
}

TEST_F(FakeDriveServiceTest, GetResourceEntry_Offline) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "gdata/root_feed.json"));
  fake_service_.set_offline(true);

  const std::string kResourceId = "file:2_file_resource_id";
  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceEntry> resource_entry;
  fake_service_.GetResourceEntry(
      kResourceId,
      test_util::CreateCopyResultCallback(&error, &resource_entry));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(GDATA_NO_CONNECTION, error);
  EXPECT_FALSE(resource_entry);
}

TEST_F(FakeDriveServiceTest, GetShareUrl) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "gdata/root_feed.json"));

  const std::string kResourceId = "file:2_file_resource_id";
  GDataErrorCode error = GDATA_OTHER_ERROR;
  GURL share_url;
  fake_service_.GetShareUrl(
      kResourceId,
      GURL(),  // embed origin
      test_util::CreateCopyResultCallback(&error, &share_url));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_FALSE(share_url.is_empty());
}

TEST_F(FakeDriveServiceTest, DeleteResource_ExistingFile) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "gdata/root_feed.json"));

  // Resource "file:2_file_resource_id" should now exist.
  ASSERT_TRUE(Exists("file:2_file_resource_id"));

  GDataErrorCode error = GDATA_OTHER_ERROR;
  fake_service_.DeleteResource("file:2_file_resource_id",
                               std::string(),  // etag
                               test_util::CreateCopyResultCallback(&error));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(HTTP_NO_CONTENT, error);
  // Resource "file:2_file_resource_id" should be gone now.
  EXPECT_FALSE(Exists("file:2_file_resource_id"));

  error = GDATA_OTHER_ERROR;
  fake_service_.DeleteResource("file:2_file_resource_id",
                               std::string(),  // etag
                               test_util::CreateCopyResultCallback(&error));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(HTTP_NOT_FOUND, error);
  EXPECT_FALSE(Exists("file:2_file_resource_id"));
}

TEST_F(FakeDriveServiceTest, DeleteResource_NonexistingFile) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "gdata/root_feed.json"));

  GDataErrorCode error = GDATA_OTHER_ERROR;
  fake_service_.DeleteResource("file:nonexisting_resource_id",
                               std::string(),  // etag
                               test_util::CreateCopyResultCallback(&error));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(HTTP_NOT_FOUND, error);
}

TEST_F(FakeDriveServiceTest, DeleteResource_ETagMatch) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "gdata/root_feed.json"));

  // Resource "file:2_file_resource_id" should now exist.
  scoped_ptr<ResourceEntry> entry = FindEntry("file:2_file_resource_id");
  ASSERT_TRUE(entry);
  ASSERT_FALSE(entry->deleted());
  ASSERT_FALSE(entry->etag().empty());

  GDataErrorCode error = GDATA_OTHER_ERROR;
  fake_service_.DeleteResource("file:2_file_resource_id",
                               entry->etag() + "_mismatch",
                               test_util::CreateCopyResultCallback(&error));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(HTTP_PRECONDITION, error);
  // Resource "file:2_file_resource_id" should still exist.
  EXPECT_TRUE(Exists("file:2_file_resource_id"));

  error = GDATA_OTHER_ERROR;
  fake_service_.DeleteResource("file:2_file_resource_id",
                               entry->etag(),
                               test_util::CreateCopyResultCallback(&error));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(HTTP_NO_CONTENT, error);
  // Resource "file:2_file_resource_id" should be gone now.
  EXPECT_FALSE(Exists("file:2_file_resource_id"));
}

TEST_F(FakeDriveServiceTest, DeleteResource_Offline) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "gdata/root_feed.json"));
  fake_service_.set_offline(true);

  GDataErrorCode error = GDATA_OTHER_ERROR;
  fake_service_.DeleteResource("file:2_file_resource_id",
                               std::string(),  // etag
                               test_util::CreateCopyResultCallback(&error));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(GDATA_NO_CONNECTION, error);
}

TEST_F(FakeDriveServiceTest, TrashResource_ExistingFile) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "gdata/root_feed.json"));

  // Resource "file:2_file_resource_id" should now exist.
  ASSERT_TRUE(Exists("file:2_file_resource_id"));

  GDataErrorCode error = GDATA_OTHER_ERROR;
  fake_service_.TrashResource("file:2_file_resource_id",
                              test_util::CreateCopyResultCallback(&error));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);
  // Resource "file:2_file_resource_id" should be gone now.
  EXPECT_FALSE(Exists("file:2_file_resource_id"));

  error = GDATA_OTHER_ERROR;
  fake_service_.TrashResource("file:2_file_resource_id",
                              test_util::CreateCopyResultCallback(&error));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(HTTP_NOT_FOUND, error);
  EXPECT_FALSE(Exists("file:2_file_resource_id"));
}

TEST_F(FakeDriveServiceTest, TrashResource_NonexistingFile) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "gdata/root_feed.json"));

  GDataErrorCode error = GDATA_OTHER_ERROR;
  fake_service_.TrashResource("file:nonexisting_resource_id",
                              test_util::CreateCopyResultCallback(&error));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(HTTP_NOT_FOUND, error);
}

TEST_F(FakeDriveServiceTest, TrashResource_Offline) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "gdata/root_feed.json"));
  fake_service_.set_offline(true);

  GDataErrorCode error = GDATA_OTHER_ERROR;
  fake_service_.TrashResource("file:2_file_resource_id",
                              test_util::CreateCopyResultCallback(&error));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(GDATA_NO_CONNECTION, error);
}

TEST_F(FakeDriveServiceTest, DownloadFile_ExistingFile) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "gdata/root_feed.json"));

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  std::vector<test_util::ProgressInfo> download_progress_values;

  const base::FilePath kOutputFilePath =
      temp_dir.path().AppendASCII("whatever.txt");
  GDataErrorCode error = GDATA_OTHER_ERROR;
  base::FilePath output_file_path;
  test_util::TestGetContentCallback get_content_callback;
  fake_service_.DownloadFile(
      kOutputFilePath,
      "file:2_file_resource_id",
      test_util::CreateCopyResultCallback(&error, &output_file_path),
      get_content_callback.callback(),
      base::Bind(&test_util::AppendProgressCallbackResult,
                 &download_progress_values));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_EQ(output_file_path, kOutputFilePath);
  std::string content;
  ASSERT_TRUE(base::ReadFileToString(output_file_path, &content));
  // The content is "x"s of the file size specified in root_feed.json.
  EXPECT_EQ("This is some test content.", content);
  ASSERT_TRUE(!download_progress_values.empty());
  EXPECT_TRUE(base::STLIsSorted(download_progress_values));
  EXPECT_LE(0, download_progress_values.front().first);
  EXPECT_GE(26, download_progress_values.back().first);
  EXPECT_EQ(content, get_content_callback.GetConcatenatedData());
}

TEST_F(FakeDriveServiceTest, DownloadFile_NonexistingFile) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "gdata/root_feed.json"));

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  const base::FilePath kOutputFilePath =
      temp_dir.path().AppendASCII("whatever.txt");
  GDataErrorCode error = GDATA_OTHER_ERROR;
  base::FilePath output_file_path;
  fake_service_.DownloadFile(
      kOutputFilePath,
      "file:non_existent_file_resource_id",
      test_util::CreateCopyResultCallback(&error, &output_file_path),
      GetContentCallback(),
      ProgressCallback());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(HTTP_NOT_FOUND, error);
}

TEST_F(FakeDriveServiceTest, DownloadFile_Offline) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "gdata/root_feed.json"));
  fake_service_.set_offline(true);

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  const base::FilePath kOutputFilePath =
      temp_dir.path().AppendASCII("whatever.txt");
  GDataErrorCode error = GDATA_OTHER_ERROR;
  base::FilePath output_file_path;
  fake_service_.DownloadFile(
      kOutputFilePath,
      "file:2_file_resource_id",
      test_util::CreateCopyResultCallback(&error, &output_file_path),
      GetContentCallback(),
      ProgressCallback());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(GDATA_NO_CONNECTION, error);
}

TEST_F(FakeDriveServiceTest, CopyResource) {
  const base::Time::Exploded kModifiedDate = {2012, 7, 0, 19, 15, 59, 13, 123};

  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "gdata/root_feed.json"));

  int64 old_largest_change_id = GetLargestChangeByAboutResource();

  const std::string kResourceId = "file:2_file_resource_id";
  const std::string kParentResourceId = "folder:2_folder_resource_id";
  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceEntry> resource_entry;
  fake_service_.CopyResource(
      kResourceId,
      kParentResourceId,
      "new title",
      base::Time::FromUTCExploded(kModifiedDate),
      test_util::CreateCopyResultCallback(&error, &resource_entry));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);
  ASSERT_TRUE(resource_entry);
  // The copied entry should have the new resource ID and the title.
  EXPECT_NE(kResourceId, resource_entry->resource_id());
  EXPECT_EQ("new title", resource_entry->title());
  EXPECT_EQ(base::Time::FromUTCExploded(kModifiedDate),
            resource_entry->updated_time());
  EXPECT_TRUE(HasParent(resource_entry->resource_id(), kParentResourceId));
  // Should be incremented as a new hosted document was created.
  EXPECT_EQ(old_largest_change_id + 1,
            fake_service_.about_resource().largest_change_id());
  EXPECT_EQ(old_largest_change_id + 1, GetLargestChangeByAboutResource());
}

TEST_F(FakeDriveServiceTest, CopyResource_NonExisting) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "gdata/root_feed.json"));

  const std::string kResourceId = "document:nonexisting_resource_id";
  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceEntry> resource_entry;
  fake_service_.CopyResource(
      kResourceId,
      "folder:1_folder_resource_id",
      "new title",
      base::Time(),
      test_util::CreateCopyResultCallback(&error, &resource_entry));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(HTTP_NOT_FOUND, error);
}

TEST_F(FakeDriveServiceTest, CopyResource_EmptyParentResourceId) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "gdata/root_feed.json"));

  int64 old_largest_change_id = GetLargestChangeByAboutResource();

  const std::string kResourceId = "file:2_file_resource_id";
  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceEntry> resource_entry;
  fake_service_.CopyResource(
      kResourceId,
      std::string(),
      "new title",
      base::Time(),
      test_util::CreateCopyResultCallback(&error, &resource_entry));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);
  ASSERT_TRUE(resource_entry);
  // The copied entry should have the new resource ID and the title.
  EXPECT_NE(kResourceId, resource_entry->resource_id());
  EXPECT_EQ("new title", resource_entry->title());
  EXPECT_TRUE(HasParent(kResourceId, fake_service_.GetRootResourceId()));
  // Should be incremented as a new hosted document was created.
  EXPECT_EQ(old_largest_change_id + 1,
            fake_service_.about_resource().largest_change_id());
  EXPECT_EQ(old_largest_change_id + 1, GetLargestChangeByAboutResource());
}

TEST_F(FakeDriveServiceTest, CopyResource_Offline) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "gdata/root_feed.json"));
  fake_service_.set_offline(true);

  const std::string kResourceId = "file:2_file_resource_id";
  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceEntry> resource_entry;
  fake_service_.CopyResource(
      kResourceId,
      "folder:1_folder_resource_id",
      "new title",
      base::Time(),
      test_util::CreateCopyResultCallback(&error, &resource_entry));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(GDATA_NO_CONNECTION, error);
  EXPECT_FALSE(resource_entry);
}

TEST_F(FakeDriveServiceTest, UpdateResource) {
  const base::Time::Exploded kModifiedDate = {2012, 7, 0, 19, 15, 59, 13, 123};
  const base::Time::Exploded kViewedDate = {2013, 8, 1, 20, 16, 00, 14, 234};

  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "gdata/root_feed.json"));

  int64 old_largest_change_id = GetLargestChangeByAboutResource();

  const std::string kResourceId = "file:2_file_resource_id";
  const std::string kParentResourceId = "folder:2_folder_resource_id";
  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceEntry> resource_entry;
  fake_service_.UpdateResource(
      kResourceId,
      kParentResourceId,
      "new title",
      base::Time::FromUTCExploded(kModifiedDate),
      base::Time::FromUTCExploded(kViewedDate),
      test_util::CreateCopyResultCallback(&error, &resource_entry));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);
  ASSERT_TRUE(resource_entry);
  // The updated entry should have the new title.
  EXPECT_EQ(kResourceId, resource_entry->resource_id());
  EXPECT_EQ("new title", resource_entry->title());
  EXPECT_EQ(base::Time::FromUTCExploded(kModifiedDate),
            resource_entry->updated_time());
  EXPECT_EQ(base::Time::FromUTCExploded(kViewedDate),
            resource_entry->last_viewed_time());
  EXPECT_TRUE(HasParent(kResourceId, kParentResourceId));
  // Should be incremented as a new hosted document was created.
  EXPECT_EQ(old_largest_change_id + 1,
            fake_service_.about_resource().largest_change_id());
  EXPECT_EQ(old_largest_change_id + 1, GetLargestChangeByAboutResource());
}

TEST_F(FakeDriveServiceTest, UpdateResource_NonExisting) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "gdata/root_feed.json"));

  const std::string kResourceId = "document:nonexisting_resource_id";
  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceEntry> resource_entry;
  fake_service_.UpdateResource(
      kResourceId,
      "folder:1_folder_resource_id",
      "new title",
      base::Time(),
      base::Time(),
      test_util::CreateCopyResultCallback(&error, &resource_entry));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(HTTP_NOT_FOUND, error);
}

TEST_F(FakeDriveServiceTest, UpdateResource_EmptyParentResourceId) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "gdata/root_feed.json"));

  int64 old_largest_change_id = GetLargestChangeByAboutResource();

  const std::string kResourceId = "file:2_file_resource_id";

  // Just make sure that the resource is under root.
  ASSERT_TRUE(HasParent(kResourceId, "fake_root"));

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceEntry> resource_entry;
  fake_service_.UpdateResource(
      kResourceId,
      std::string(),
      "new title",
      base::Time(),
      base::Time(),
      test_util::CreateCopyResultCallback(&error, &resource_entry));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);
  ASSERT_TRUE(resource_entry);
  // The updated entry should have the new title.
  EXPECT_EQ(kResourceId, resource_entry->resource_id());
  EXPECT_EQ("new title", resource_entry->title());
  EXPECT_TRUE(HasParent(kResourceId, "fake_root"));
  // Should be incremented as a new hosted document was created.
  EXPECT_EQ(old_largest_change_id + 1,
            fake_service_.about_resource().largest_change_id());
  EXPECT_EQ(old_largest_change_id + 1, GetLargestChangeByAboutResource());
}

TEST_F(FakeDriveServiceTest, UpdateResource_Offline) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "gdata/root_feed.json"));
  fake_service_.set_offline(true);

  const std::string kResourceId = "file:2_file_resource_id";
  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceEntry> resource_entry;
  fake_service_.UpdateResource(
      kResourceId,
      std::string(),
      "new title",
      base::Time(),
      base::Time(),
      test_util::CreateCopyResultCallback(&error, &resource_entry));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(GDATA_NO_CONNECTION, error);
  EXPECT_FALSE(resource_entry);
}

TEST_F(FakeDriveServiceTest, RenameResource_ExistingFile) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "gdata/root_feed.json"));

  int64 old_largest_change_id = GetLargestChangeByAboutResource();

  const std::string kResourceId = "file:2_file_resource_id";

  GDataErrorCode error = GDATA_OTHER_ERROR;
  fake_service_.RenameResource(kResourceId,
                               "new title",
                               test_util::CreateCopyResultCallback(&error));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);

  scoped_ptr<ResourceEntry> resource_entry = FindEntry(kResourceId);
  ASSERT_TRUE(resource_entry);
  EXPECT_EQ("new title", resource_entry->title());
  // Should be incremented as a file was renamed.
  EXPECT_EQ(old_largest_change_id + 1,
            fake_service_.about_resource().largest_change_id());
  EXPECT_EQ(old_largest_change_id + 1, GetLargestChangeByAboutResource());
}

TEST_F(FakeDriveServiceTest, RenameResource_NonexistingFile) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "gdata/root_feed.json"));

  const std::string kResourceId = "file:nonexisting_file";

  GDataErrorCode error = GDATA_OTHER_ERROR;
  fake_service_.RenameResource(kResourceId,
                               "new title",
                               test_util::CreateCopyResultCallback(&error));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(HTTP_NOT_FOUND, error);
}

TEST_F(FakeDriveServiceTest, RenameResource_Offline) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "gdata/root_feed.json"));
  fake_service_.set_offline(true);

  const std::string kResourceId = "file:2_file_resource_id";

  GDataErrorCode error = GDATA_OTHER_ERROR;
  fake_service_.RenameResource(kResourceId,
                               "new title",
                               test_util::CreateCopyResultCallback(&error));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(GDATA_NO_CONNECTION, error);
}

TEST_F(FakeDriveServiceTest, AddResourceToDirectory_FileInRootDirectory) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "gdata/root_feed.json"));

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
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);

  // The parent link should now be changed.
  EXPECT_TRUE(HasParent(kResourceId, kOldParentResourceId));
  EXPECT_TRUE(HasParent(kResourceId, kNewParentResourceId));
  // Should be incremented as a file was moved.
  EXPECT_EQ(old_largest_change_id + 1,
            fake_service_.about_resource().largest_change_id());
  EXPECT_EQ(old_largest_change_id + 1, GetLargestChangeByAboutResource());
}

TEST_F(FakeDriveServiceTest, AddResourceToDirectory_FileInNonRootDirectory) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "gdata/root_feed.json"));

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
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);

  // The parent link should now be changed.
  EXPECT_TRUE(HasParent(kResourceId, kOldParentResourceId));
  EXPECT_TRUE(HasParent(kResourceId, kNewParentResourceId));
  // Should be incremented as a file was moved.
  EXPECT_EQ(old_largest_change_id + 1,
            fake_service_.about_resource().largest_change_id());
  EXPECT_EQ(old_largest_change_id + 1, GetLargestChangeByAboutResource());
}

TEST_F(FakeDriveServiceTest, AddResourceToDirectory_NonexistingFile) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "gdata/root_feed.json"));

  const std::string kResourceId = "file:nonexisting_file";
  const std::string kNewParentResourceId = "folder:1_folder_resource_id";

  GDataErrorCode error = GDATA_OTHER_ERROR;
  fake_service_.AddResourceToDirectory(
      kNewParentResourceId,
      kResourceId,
      test_util::CreateCopyResultCallback(&error));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(HTTP_NOT_FOUND, error);
}

TEST_F(FakeDriveServiceTest, AddResourceToDirectory_OrphanFile) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "gdata/root_feed.json"));

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
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);

  // The parent link should now be changed.
  EXPECT_TRUE(HasParent(kResourceId, kNewParentResourceId));
  EXPECT_FALSE(HasParent(kResourceId, fake_service_.GetRootResourceId()));
  // Should be incremented as a file was moved.
  EXPECT_EQ(old_largest_change_id + 1,
            fake_service_.about_resource().largest_change_id());
  EXPECT_EQ(old_largest_change_id + 1, GetLargestChangeByAboutResource());
}

TEST_F(FakeDriveServiceTest, AddResourceToDirectory_Offline) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "gdata/root_feed.json"));
  fake_service_.set_offline(true);

  const std::string kResourceId = "file:2_file_resource_id";
  const std::string kNewParentResourceId = "folder:1_folder_resource_id";

  GDataErrorCode error = GDATA_OTHER_ERROR;
  fake_service_.AddResourceToDirectory(
      kNewParentResourceId,
      kResourceId,
      test_util::CreateCopyResultCallback(&error));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(GDATA_NO_CONNECTION, error);
}

TEST_F(FakeDriveServiceTest, RemoveResourceFromDirectory_ExistingFile) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "gdata/root_feed.json"));

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
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(HTTP_NO_CONTENT, error);

  resource_entry = FindEntry(kResourceId);
  ASSERT_TRUE(resource_entry);
  // The parent link should be gone now.
  parent_link = resource_entry->GetLinkByType(Link::LINK_PARENT);
  ASSERT_FALSE(parent_link);
  // Should be incremented as a file was moved to the root directory.
  EXPECT_EQ(old_largest_change_id + 1,
            fake_service_.about_resource().largest_change_id());
  EXPECT_EQ(old_largest_change_id + 1, GetLargestChangeByAboutResource());
}

TEST_F(FakeDriveServiceTest, RemoveResourceFromDirectory_NonexistingFile) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "gdata/root_feed.json"));

  const std::string kResourceId = "file:nonexisting_file";
  const std::string kParentResourceId = "folder:1_folder_resource_id";

  GDataErrorCode error = GDATA_OTHER_ERROR;
  fake_service_.RemoveResourceFromDirectory(
      kParentResourceId,
      kResourceId,
      test_util::CreateCopyResultCallback(&error));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(HTTP_NOT_FOUND, error);
}

TEST_F(FakeDriveServiceTest, RemoveResourceFromDirectory_OrphanFile) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "gdata/root_feed.json"));

  const std::string kResourceId = "file:1_orphanfile_resource_id";
  const std::string kParentResourceId = fake_service_.GetRootResourceId();

  GDataErrorCode error = GDATA_OTHER_ERROR;
  fake_service_.RemoveResourceFromDirectory(
      kParentResourceId,
      kResourceId,
      test_util::CreateCopyResultCallback(&error));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(HTTP_NOT_FOUND, error);
}

TEST_F(FakeDriveServiceTest, RemoveResourceFromDirectory_Offline) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "gdata/root_feed.json"));
  fake_service_.set_offline(true);

  const std::string kResourceId = "file:subdirectory_file_1_id";
  const std::string kParentResourceId = "folder:1_folder_resource_id";

  GDataErrorCode error = GDATA_OTHER_ERROR;
  fake_service_.RemoveResourceFromDirectory(
      kParentResourceId,
      kResourceId,
      test_util::CreateCopyResultCallback(&error));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(GDATA_NO_CONNECTION, error);
}

TEST_F(FakeDriveServiceTest, AddNewDirectory_EmptyParent) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "gdata/root_feed.json"));

  int64 old_largest_change_id = GetLargestChangeByAboutResource();

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceEntry> resource_entry;
  fake_service_.AddNewDirectory(
      std::string(),
      "new directory",
      DriveServiceInterface::AddNewDirectoryOptions(),
      test_util::CreateCopyResultCallback(&error, &resource_entry));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(HTTP_CREATED, error);
  ASSERT_TRUE(resource_entry);
  EXPECT_TRUE(resource_entry->is_folder());
  EXPECT_EQ("resource_id_1", resource_entry->resource_id());
  EXPECT_EQ("new directory", resource_entry->title());
  EXPECT_TRUE(HasParent(resource_entry->resource_id(),
                        fake_service_.GetRootResourceId()));
  // Should be incremented as a new directory was created.
  EXPECT_EQ(old_largest_change_id + 1,
            fake_service_.about_resource().largest_change_id());
  EXPECT_EQ(old_largest_change_id + 1, GetLargestChangeByAboutResource());
}

TEST_F(FakeDriveServiceTest, AddNewDirectory_ToRootDirectory) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "gdata/root_feed.json"));

  int64 old_largest_change_id = GetLargestChangeByAboutResource();

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceEntry> resource_entry;
  fake_service_.AddNewDirectory(
      fake_service_.GetRootResourceId(),
      "new directory",
      DriveServiceInterface::AddNewDirectoryOptions(),
      test_util::CreateCopyResultCallback(&error, &resource_entry));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(HTTP_CREATED, error);
  ASSERT_TRUE(resource_entry);
  EXPECT_TRUE(resource_entry->is_folder());
  EXPECT_EQ("resource_id_1", resource_entry->resource_id());
  EXPECT_EQ("new directory", resource_entry->title());
  EXPECT_TRUE(HasParent(resource_entry->resource_id(),
                        fake_service_.GetRootResourceId()));
  // Should be incremented as a new directory was created.
  EXPECT_EQ(old_largest_change_id + 1,
            fake_service_.about_resource().largest_change_id());
  EXPECT_EQ(old_largest_change_id + 1, GetLargestChangeByAboutResource());
}

TEST_F(FakeDriveServiceTest, AddNewDirectory_ToRootDirectoryOnEmptyFileSystem) {
  int64 old_largest_change_id = GetLargestChangeByAboutResource();

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceEntry> resource_entry;
  fake_service_.AddNewDirectory(
      fake_service_.GetRootResourceId(),
      "new directory",
      DriveServiceInterface::AddNewDirectoryOptions(),
      test_util::CreateCopyResultCallback(&error, &resource_entry));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(HTTP_CREATED, error);
  ASSERT_TRUE(resource_entry);
  EXPECT_TRUE(resource_entry->is_folder());
  EXPECT_EQ("resource_id_1", resource_entry->resource_id());
  EXPECT_EQ("new directory", resource_entry->title());
  EXPECT_TRUE(HasParent(resource_entry->resource_id(),
                        fake_service_.GetRootResourceId()));
  // Should be incremented as a new directory was created.
  EXPECT_EQ(old_largest_change_id + 1,
            fake_service_.about_resource().largest_change_id());
  EXPECT_EQ(old_largest_change_id + 1, GetLargestChangeByAboutResource());
}

TEST_F(FakeDriveServiceTest, AddNewDirectory_ToNonRootDirectory) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "gdata/root_feed.json"));

  int64 old_largest_change_id = GetLargestChangeByAboutResource();

  const std::string kParentResourceId = "folder:1_folder_resource_id";

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceEntry> resource_entry;
  fake_service_.AddNewDirectory(
      kParentResourceId,
      "new directory",
      DriveServiceInterface::AddNewDirectoryOptions(),
      test_util::CreateCopyResultCallback(&error, &resource_entry));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(HTTP_CREATED, error);
  ASSERT_TRUE(resource_entry);
  EXPECT_TRUE(resource_entry->is_folder());
  EXPECT_EQ("resource_id_1", resource_entry->resource_id());
  EXPECT_EQ("new directory", resource_entry->title());
  EXPECT_TRUE(HasParent(resource_entry->resource_id(), kParentResourceId));
  // Should be incremented as a new directory was created.
  EXPECT_EQ(old_largest_change_id + 1,
            fake_service_.about_resource().largest_change_id());
  EXPECT_EQ(old_largest_change_id + 1, GetLargestChangeByAboutResource());
}

TEST_F(FakeDriveServiceTest, AddNewDirectory_ToNonexistingDirectory) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "gdata/root_feed.json"));

  const std::string kParentResourceId = "folder:nonexisting_resource_id";

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceEntry> resource_entry;
  fake_service_.AddNewDirectory(
      kParentResourceId,
      "new directory",
      DriveServiceInterface::AddNewDirectoryOptions(),
      test_util::CreateCopyResultCallback(&error, &resource_entry));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(HTTP_NOT_FOUND, error);
  EXPECT_FALSE(resource_entry);
}

TEST_F(FakeDriveServiceTest, AddNewDirectory_Offline) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "gdata/root_feed.json"));
  fake_service_.set_offline(true);

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceEntry> resource_entry;
  fake_service_.AddNewDirectory(
      fake_service_.GetRootResourceId(),
      "new directory",
      DriveServiceInterface::AddNewDirectoryOptions(),
      test_util::CreateCopyResultCallback(&error, &resource_entry));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(GDATA_NO_CONNECTION, error);
  EXPECT_FALSE(resource_entry);
}

TEST_F(FakeDriveServiceTest, InitiateUploadNewFile_Offline) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "gdata/root_feed.json"));
  fake_service_.set_offline(true);

  GDataErrorCode error = GDATA_OTHER_ERROR;
  GURL upload_location;
  fake_service_.InitiateUploadNewFile(
      "test/foo",
      13,
      "folder:1_folder_resource_id",
      "new file.foo",
      FakeDriveService::InitiateUploadNewFileOptions(),
      test_util::CreateCopyResultCallback(&error, &upload_location));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(GDATA_NO_CONNECTION, error);
  EXPECT_TRUE(upload_location.is_empty());
}

TEST_F(FakeDriveServiceTest, InitiateUploadNewFile_NotFound) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "gdata/root_feed.json"));

  GDataErrorCode error = GDATA_OTHER_ERROR;
  GURL upload_location;
  fake_service_.InitiateUploadNewFile(
      "test/foo",
      13,
      "non_existent",
      "new file.foo",
      FakeDriveService::InitiateUploadNewFileOptions(),
      test_util::CreateCopyResultCallback(&error, &upload_location));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(HTTP_NOT_FOUND, error);
  EXPECT_TRUE(upload_location.is_empty());
}

TEST_F(FakeDriveServiceTest, InitiateUploadNewFile) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "gdata/root_feed.json"));

  GDataErrorCode error = GDATA_OTHER_ERROR;
  GURL upload_location;
  fake_service_.InitiateUploadNewFile(
      "test/foo",
      13,
      "folder:1_folder_resource_id",
      "new file.foo",
      FakeDriveService::InitiateUploadNewFileOptions(),
      test_util::CreateCopyResultCallback(&error, &upload_location));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_FALSE(upload_location.is_empty());
  EXPECT_NE(GURL("https://1_folder_resumable_create_media_link?mode=newfile"),
            upload_location);
}

TEST_F(FakeDriveServiceTest, InitiateUploadExistingFile_Offline) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "gdata/root_feed.json"));
  fake_service_.set_offline(true);

  GDataErrorCode error = GDATA_OTHER_ERROR;
  GURL upload_location;
  fake_service_.InitiateUploadExistingFile(
      "test/foo",
      13,
      "file:2_file_resource_id",
      FakeDriveService::InitiateUploadExistingFileOptions(),
      test_util::CreateCopyResultCallback(&error, &upload_location));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(GDATA_NO_CONNECTION, error);
  EXPECT_TRUE(upload_location.is_empty());
}

TEST_F(FakeDriveServiceTest, InitiateUploadExistingFile_NotFound) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "gdata/root_feed.json"));

  GDataErrorCode error = GDATA_OTHER_ERROR;
  GURL upload_location;
  fake_service_.InitiateUploadExistingFile(
      "test/foo",
      13,
      "non_existent",
      FakeDriveService::InitiateUploadExistingFileOptions(),
      test_util::CreateCopyResultCallback(&error, &upload_location));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(HTTP_NOT_FOUND, error);
  EXPECT_TRUE(upload_location.is_empty());
}

TEST_F(FakeDriveServiceTest, InitiateUploadExistingFile_WrongETag) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "gdata/root_feed.json"));

  FakeDriveService::InitiateUploadExistingFileOptions options;
  options.etag = "invalid_etag";

  GDataErrorCode error = GDATA_OTHER_ERROR;
  GURL upload_location;
  fake_service_.InitiateUploadExistingFile(
      "text/plain",
      13,
      "file:2_file_resource_id",
      options,
      test_util::CreateCopyResultCallback(&error, &upload_location));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(HTTP_PRECONDITION, error);
  EXPECT_TRUE(upload_location.is_empty());
}

TEST_F(FakeDriveServiceTest, InitiateUpload_ExistingFile) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "gdata/root_feed.json"));

  FakeDriveService::InitiateUploadExistingFileOptions options;
  options.etag = "\"HhMOFgxXHit7ImBr\"";

  GDataErrorCode error = GDATA_OTHER_ERROR;
  GURL upload_location;
  fake_service_.InitiateUploadExistingFile(
      "text/plain",
      13,
      "file:2_file_resource_id",
      options,
      test_util::CreateCopyResultCallback(&error, &upload_location));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_TRUE(upload_location.is_valid());
}

TEST_F(FakeDriveServiceTest, ResumeUpload_Offline) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "gdata/root_feed.json"));

  GDataErrorCode error = GDATA_OTHER_ERROR;
  GURL upload_location;
  fake_service_.InitiateUploadNewFile(
      "test/foo",
      15,
      "folder:1_folder_resource_id",
      "new file.foo",
      FakeDriveService::InitiateUploadNewFileOptions(),
      test_util::CreateCopyResultCallback(&error, &upload_location));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_FALSE(upload_location.is_empty());
  EXPECT_NE(GURL("https://1_folder_resumable_create_media_link"),
            upload_location);

  fake_service_.set_offline(true);

  UploadRangeResponse response;
  scoped_ptr<ResourceEntry> entry;
  fake_service_.ResumeUpload(
      upload_location,
      0, 13, 15, "test/foo",
      base::FilePath(),
      test_util::CreateCopyResultCallback(&response, &entry),
      ProgressCallback());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(GDATA_NO_CONNECTION, response.code);
  EXPECT_FALSE(entry.get());
}

TEST_F(FakeDriveServiceTest, ResumeUpload_NotFound) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "gdata/root_feed.json"));

  GDataErrorCode error = GDATA_OTHER_ERROR;
  GURL upload_location;
  fake_service_.InitiateUploadNewFile(
      "test/foo",
      15,
      "folder:1_folder_resource_id",
      "new file.foo",
      FakeDriveService::InitiateUploadNewFileOptions(),
      test_util::CreateCopyResultCallback(&error, &upload_location));
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(HTTP_SUCCESS, error);

  UploadRangeResponse response;
  scoped_ptr<ResourceEntry> entry;
  fake_service_.ResumeUpload(
      GURL("https://foo.com/"),
      0, 13, 15, "test/foo",
      base::FilePath(),
      test_util::CreateCopyResultCallback(&response, &entry),
      ProgressCallback());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(HTTP_NOT_FOUND, response.code);
  EXPECT_FALSE(entry.get());
}

TEST_F(FakeDriveServiceTest, ResumeUpload_ExistingFile) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath local_file_path =
      temp_dir.path().Append(FILE_PATH_LITERAL("File 1.txt"));
  std::string contents("hogefugapiyo");
  ASSERT_TRUE(test_util::WriteStringToFile(local_file_path, contents));

  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "gdata/root_feed.json"));

  FakeDriveService::InitiateUploadExistingFileOptions options;
  options.etag = "\"HhMOFgxXHit7ImBr\"";

  GDataErrorCode error = GDATA_OTHER_ERROR;
  GURL upload_location;
  fake_service_.InitiateUploadExistingFile(
      "text/plain",
      contents.size(),
      "file:2_file_resource_id",
      options,
      test_util::CreateCopyResultCallback(&error, &upload_location));
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(HTTP_SUCCESS, error);

  UploadRangeResponse response;
  scoped_ptr<ResourceEntry> entry;
  std::vector<test_util::ProgressInfo> upload_progress_values;
  fake_service_.ResumeUpload(
      upload_location,
      0, contents.size() / 2, contents.size(), "text/plain",
      local_file_path,
      test_util::CreateCopyResultCallback(&response, &entry),
      base::Bind(&test_util::AppendProgressCallbackResult,
                 &upload_progress_values));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(HTTP_RESUME_INCOMPLETE, response.code);
  EXPECT_FALSE(entry.get());
  ASSERT_TRUE(!upload_progress_values.empty());
  EXPECT_TRUE(base::STLIsSorted(upload_progress_values));
  EXPECT_LE(0, upload_progress_values.front().first);
  EXPECT_GE(static_cast<int64>(contents.size() / 2),
            upload_progress_values.back().first);

  upload_progress_values.clear();
  fake_service_.ResumeUpload(
      upload_location,
      contents.size() / 2, contents.size(), contents.size(), "text/plain",
      local_file_path,
      test_util::CreateCopyResultCallback(&response, &entry),
      base::Bind(&test_util::AppendProgressCallbackResult,
                 &upload_progress_values));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, response.code);
  EXPECT_TRUE(entry.get());
  EXPECT_EQ(static_cast<int64>(contents.size()),
            entry->file_size());
  EXPECT_TRUE(Exists(entry->resource_id()));
  ASSERT_TRUE(!upload_progress_values.empty());
  EXPECT_TRUE(base::STLIsSorted(upload_progress_values));
  EXPECT_LE(0, upload_progress_values.front().first);
  EXPECT_GE(static_cast<int64>(contents.size() - contents.size() / 2),
            upload_progress_values.back().first);
  EXPECT_EQ(base::MD5String(contents), entry->file_md5());
}

TEST_F(FakeDriveServiceTest, ResumeUpload_NewFile) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath local_file_path =
      temp_dir.path().Append(FILE_PATH_LITERAL("new file.foo"));
  std::string contents("hogefugapiyo");
  ASSERT_TRUE(test_util::WriteStringToFile(local_file_path, contents));

  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "gdata/root_feed.json"));

  GDataErrorCode error = GDATA_OTHER_ERROR;
  GURL upload_location;
  fake_service_.InitiateUploadNewFile(
      "test/foo",
      contents.size(),
      "folder:1_folder_resource_id",
      "new file.foo",
      FakeDriveService::InitiateUploadNewFileOptions(),
      test_util::CreateCopyResultCallback(&error, &upload_location));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_FALSE(upload_location.is_empty());
  EXPECT_NE(GURL("https://1_folder_resumable_create_media_link"),
            upload_location);

  UploadRangeResponse response;
  scoped_ptr<ResourceEntry> entry;
  std::vector<test_util::ProgressInfo> upload_progress_values;
  fake_service_.ResumeUpload(
      upload_location,
      0, contents.size() / 2, contents.size(), "test/foo",
      local_file_path,
      test_util::CreateCopyResultCallback(&response, &entry),
      base::Bind(&test_util::AppendProgressCallbackResult,
                 &upload_progress_values));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(HTTP_RESUME_INCOMPLETE, response.code);
  EXPECT_FALSE(entry.get());
  ASSERT_TRUE(!upload_progress_values.empty());
  EXPECT_TRUE(base::STLIsSorted(upload_progress_values));
  EXPECT_LE(0, upload_progress_values.front().first);
  EXPECT_GE(static_cast<int64>(contents.size() / 2),
            upload_progress_values.back().first);

  upload_progress_values.clear();
  fake_service_.ResumeUpload(
      upload_location,
      contents.size() / 2, contents.size(), contents.size(), "test/foo",
      local_file_path,
      test_util::CreateCopyResultCallback(&response, &entry),
      base::Bind(&test_util::AppendProgressCallbackResult,
                 &upload_progress_values));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(HTTP_CREATED, response.code);
  EXPECT_TRUE(entry.get());
  EXPECT_EQ(static_cast<int64>(contents.size()), entry->file_size());
  EXPECT_TRUE(Exists(entry->resource_id()));
  ASSERT_TRUE(!upload_progress_values.empty());
  EXPECT_TRUE(base::STLIsSorted(upload_progress_values));
  EXPECT_LE(0, upload_progress_values.front().first);
  EXPECT_GE(static_cast<int64>(contents.size() - contents.size() / 2),
            upload_progress_values.back().first);
  EXPECT_EQ(base::MD5String(contents), entry->file_md5());
}

TEST_F(FakeDriveServiceTest, AddNewFile_ToRootDirectory) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "gdata/root_feed.json"));

  int64 old_largest_change_id = GetLargestChangeByAboutResource();

  const std::string kContentType = "text/plain";
  const std::string kContentData = "This is some test content.";
  const std::string kTitle = "new file";

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceEntry> resource_entry;
  fake_service_.AddNewFile(
      kContentType,
      kContentData,
      fake_service_.GetRootResourceId(),
      kTitle,
      false,  // shared_with_me
      test_util::CreateCopyResultCallback(&error, &resource_entry));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(HTTP_CREATED, error);
  ASSERT_TRUE(resource_entry);
  EXPECT_TRUE(resource_entry->is_file());
  EXPECT_EQ(kContentType, resource_entry->content_mime_type());
  EXPECT_EQ(static_cast<int64>(kContentData.size()),
            resource_entry->file_size());
  EXPECT_EQ("resource_id_1", resource_entry->resource_id());
  EXPECT_EQ(kTitle, resource_entry->title());
  EXPECT_TRUE(HasParent(resource_entry->resource_id(),
                        fake_service_.GetRootResourceId()));
  // Should be incremented as a new directory was created.
  EXPECT_EQ(old_largest_change_id + 1,
            fake_service_.about_resource().largest_change_id());
  EXPECT_EQ(old_largest_change_id + 1, GetLargestChangeByAboutResource());
  EXPECT_EQ(base::MD5String(kContentData), resource_entry->file_md5());
}

TEST_F(FakeDriveServiceTest, AddNewFile_ToRootDirectoryOnEmptyFileSystem) {
  int64 old_largest_change_id = GetLargestChangeByAboutResource();

  const std::string kContentType = "text/plain";
  const std::string kContentData = "This is some test content.";
  const std::string kTitle = "new file";

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceEntry> resource_entry;
  fake_service_.AddNewFile(
      kContentType,
      kContentData,
      fake_service_.GetRootResourceId(),
      kTitle,
      false,  // shared_with_me
      test_util::CreateCopyResultCallback(&error, &resource_entry));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(HTTP_CREATED, error);
  ASSERT_TRUE(resource_entry);
  EXPECT_TRUE(resource_entry->is_file());
  EXPECT_EQ(kContentType, resource_entry->content_mime_type());
  EXPECT_EQ(static_cast<int64>(kContentData.size()),
            resource_entry->file_size());
  EXPECT_EQ("resource_id_1", resource_entry->resource_id());
  EXPECT_EQ(kTitle, resource_entry->title());
  EXPECT_TRUE(HasParent(resource_entry->resource_id(),
                        fake_service_.GetRootResourceId()));
  // Should be incremented as a new directory was created.
  EXPECT_EQ(old_largest_change_id + 1,
            fake_service_.about_resource().largest_change_id());
  EXPECT_EQ(old_largest_change_id + 1, GetLargestChangeByAboutResource());
  EXPECT_EQ(base::MD5String(kContentData), resource_entry->file_md5());
}

TEST_F(FakeDriveServiceTest, AddNewFile_ToNonRootDirectory) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "gdata/root_feed.json"));

  int64 old_largest_change_id = GetLargestChangeByAboutResource();

  const std::string kContentType = "text/plain";
  const std::string kContentData = "This is some test content.";
  const std::string kTitle = "new file";
  const std::string kParentResourceId = "folder:1_folder_resource_id";

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceEntry> resource_entry;
  fake_service_.AddNewFile(
      kContentType,
      kContentData,
      kParentResourceId,
      kTitle,
      false,  // shared_with_me
      test_util::CreateCopyResultCallback(&error, &resource_entry));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(HTTP_CREATED, error);
  ASSERT_TRUE(resource_entry);
  EXPECT_TRUE(resource_entry->is_file());
  EXPECT_EQ(kContentType, resource_entry->content_mime_type());
  EXPECT_EQ(static_cast<int64>(kContentData.size()),
            resource_entry->file_size());
  EXPECT_EQ("resource_id_1", resource_entry->resource_id());
  EXPECT_EQ(kTitle, resource_entry->title());
  EXPECT_TRUE(HasParent(resource_entry->resource_id(), kParentResourceId));
  // Should be incremented as a new directory was created.
  EXPECT_EQ(old_largest_change_id + 1,
            fake_service_.about_resource().largest_change_id());
  EXPECT_EQ(old_largest_change_id + 1, GetLargestChangeByAboutResource());
  EXPECT_EQ(base::MD5String(kContentData), resource_entry->file_md5());
}

TEST_F(FakeDriveServiceTest, AddNewFile_ToNonexistingDirectory) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "gdata/root_feed.json"));

  const std::string kContentType = "text/plain";
  const std::string kContentData = "This is some test content.";
  const std::string kTitle = "new file";
  const std::string kParentResourceId = "folder:nonexisting_resource_id";

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceEntry> resource_entry;
  fake_service_.AddNewFile(
      kContentType,
      kContentData,
      kParentResourceId,
      kTitle,
      false,  // shared_with_me
      test_util::CreateCopyResultCallback(&error, &resource_entry));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(HTTP_NOT_FOUND, error);
  EXPECT_FALSE(resource_entry);
}

TEST_F(FakeDriveServiceTest, AddNewFile_Offline) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "gdata/root_feed.json"));
  fake_service_.set_offline(true);

  const std::string kContentType = "text/plain";
  const std::string kContentData = "This is some test content.";
  const std::string kTitle = "new file";

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceEntry> resource_entry;
  fake_service_.AddNewFile(
      kContentType,
      kContentData,
      fake_service_.GetRootResourceId(),
      kTitle,
      false,  // shared_with_me
      test_util::CreateCopyResultCallback(&error, &resource_entry));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(GDATA_NO_CONNECTION, error);
  EXPECT_FALSE(resource_entry);
}

TEST_F(FakeDriveServiceTest, AddNewFile_SharedWithMeLabel) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "gdata/root_feed.json"));

  const std::string kContentType = "text/plain";
  const std::string kContentData = "This is some test content.";
  const std::string kTitle = "new file";

  int64 old_largest_change_id = GetLargestChangeByAboutResource();

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceEntry> resource_entry;
  fake_service_.AddNewFile(
      kContentType,
      kContentData,
      fake_service_.GetRootResourceId(),
      kTitle,
      true,  // shared_with_me
      test_util::CreateCopyResultCallback(&error, &resource_entry));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(HTTP_CREATED, error);
  ASSERT_TRUE(resource_entry);
  EXPECT_TRUE(resource_entry->is_file());
  EXPECT_EQ(kContentType, resource_entry->content_mime_type());
  EXPECT_EQ(static_cast<int64>(kContentData.size()),
            resource_entry->file_size());
  EXPECT_EQ("resource_id_1", resource_entry->resource_id());
  EXPECT_EQ(kTitle, resource_entry->title());
  EXPECT_TRUE(HasParent(resource_entry->resource_id(),
                        fake_service_.GetRootResourceId()));
  ASSERT_EQ(1U, resource_entry->labels().size());
  EXPECT_EQ("shared-with-me", resource_entry->labels()[0]);
  // Should be incremented as a new directory was created.
  EXPECT_EQ(old_largest_change_id + 1,
            fake_service_.about_resource().largest_change_id());
  EXPECT_EQ(old_largest_change_id + 1, GetLargestChangeByAboutResource());
  EXPECT_EQ(base::MD5String(kContentData), resource_entry->file_md5());
}

TEST_F(FakeDriveServiceTest, SetLastModifiedTime_ExistingFile) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "gdata/root_feed.json"));

  const std::string kResourceId = "file:2_file_resource_id";
  base::Time time;
  ASSERT_TRUE(base::Time::FromString("1 April 2013 12:34:56", &time));

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceEntry> resource_entry;
  fake_service_.SetLastModifiedTime(
      kResourceId,
      time,
      test_util::CreateCopyResultCallback(&error, &resource_entry));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);
  ASSERT_TRUE(resource_entry);
  EXPECT_EQ(time, resource_entry->updated_time());
}

TEST_F(FakeDriveServiceTest, SetLastModifiedTime_NonexistingFile) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "gdata/root_feed.json"));

  const std::string kResourceId = "file:nonexisting_resource_id";
  base::Time time;
  ASSERT_TRUE(base::Time::FromString("1 April 2013 12:34:56", &time));

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceEntry> resource_entry;
  fake_service_.SetLastModifiedTime(
      kResourceId,
      time,
      test_util::CreateCopyResultCallback(&error, &resource_entry));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(HTTP_NOT_FOUND, error);
  EXPECT_FALSE(resource_entry);
}

TEST_F(FakeDriveServiceTest, SetLastModifiedTime_Offline) {
  ASSERT_TRUE(fake_service_.LoadResourceListForWapi(
      "gdata/root_feed.json"));
  fake_service_.set_offline(true);

  const std::string kResourceId = "file:2_file_resource_id";
  base::Time time;
  ASSERT_TRUE(base::Time::FromString("1 April 2013 12:34:56", &time));

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<ResourceEntry> resource_entry;
  fake_service_.SetLastModifiedTime(
      kResourceId,
      time,
      test_util::CreateCopyResultCallback(&error, &resource_entry));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(GDATA_NO_CONNECTION, error);
  EXPECT_FALSE(resource_entry);
}

}  // namespace

}  // namespace drive
