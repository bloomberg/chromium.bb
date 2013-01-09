// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google_apis/fake_drive_service.h"

#include "base/message_loop.h"
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

  // Returns true if the resource identified by |resource_id| exists.
  bool Exists(const std::string& resource_id) {
    GDataErrorCode error = GDATA_OTHER_ERROR;
    scoped_ptr<ResourceEntry> resource_entry;
    fake_service_.GetResourceEntry(
        resource_id,
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

TEST_F(FakeDriveServiceTest, GetResourceList) {
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

}  // namespace google_apis
