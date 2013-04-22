// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google_apis/drive_api_url_generator.h"

#include "chrome/browser/google_apis/test_util.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace google_apis {

class DriveApiUrlGeneratorTest : public testing::Test {
 public:
  DriveApiUrlGeneratorTest()
      : url_generator_(GURL(DriveApiUrlGenerator::kBaseUrlForProduction)),
        test_url_generator_(test_util::GetBaseUrlForTesting(12345)) {
  }

 protected:
  DriveApiUrlGenerator url_generator_;
  DriveApiUrlGenerator test_url_generator_;
};

// Make sure the hard-coded urls are returned.
TEST_F(DriveApiUrlGeneratorTest, GetAboutUrl) {
  EXPECT_EQ("https://www.googleapis.com/drive/v2/about",
            url_generator_.GetAboutUrl().spec());
  EXPECT_EQ("http://127.0.0.1:12345/drive/v2/about",
            test_url_generator_.GetAboutUrl().spec());
}

TEST_F(DriveApiUrlGeneratorTest, GetApplistUrl) {
  EXPECT_EQ("https://www.googleapis.com/drive/v2/apps",
            url_generator_.GetApplistUrl().spec());
  EXPECT_EQ("http://127.0.0.1:12345/drive/v2/apps",
            test_url_generator_.GetApplistUrl().spec());
}

TEST_F(DriveApiUrlGeneratorTest, GetChangelistUrl) {
  // Do not add startChangeId parameter if |start_changestamp| is 0.
  EXPECT_EQ("https://www.googleapis.com/drive/v2/changes?maxResults=500",
            url_generator_.GetChangelistUrl(true, 0, 500).spec());
  EXPECT_EQ("http://127.0.0.1:12345/drive/v2/changes?maxResults=500",
            test_url_generator_.GetChangelistUrl(true, 0, 500).spec());

  // Set includeDeleted parameter if |include_deleted| is set to false.
  EXPECT_EQ("https://www.googleapis.com/drive/v2/changes"
            "?includeDeleted=false&maxResults=500",
            url_generator_.GetChangelistUrl(false, 0, 500).spec());
  EXPECT_EQ("http://127.0.0.1:12345/drive/v2/changes"
            "?includeDeleted=false&maxResults=500",
            test_url_generator_.GetChangelistUrl(false, 0, 500).spec());

  // Set startChangeId parameter if |start_changestamp| is given.
  EXPECT_EQ("https://www.googleapis.com/drive/v2/changes"
            "?startChangeId=100&maxResults=500",
            url_generator_.GetChangelistUrl(true, 100, 500).spec());
  EXPECT_EQ("http://127.0.0.1:12345/drive/v2/changes"
            "?startChangeId=100&maxResults=500",
            test_url_generator_.GetChangelistUrl(true, 100, 500).spec());

  // includeDeleted and startChangeId parameter can be set at the same time.
  EXPECT_EQ(
      "https://www.googleapis.com/drive/v2/changes"
      "?includeDeleted=false&startChangeId=100&maxResults=500",
      url_generator_.GetChangelistUrl(false, 100, 500).spec());
  EXPECT_EQ("http://127.0.0.1:12345/drive/v2/changes?"
            "includeDeleted=false&startChangeId=100&maxResults=500",
            test_url_generator_.GetChangelistUrl(false, 100, 500).spec());
}

TEST_F(DriveApiUrlGeneratorTest, GetFilesUrl) {
  EXPECT_EQ("https://www.googleapis.com/drive/v2/files",
            url_generator_.GetFilesUrl().spec());
  EXPECT_EQ("http://127.0.0.1:12345/drive/v2/files",
            test_url_generator_.GetFilesUrl().spec());
}

TEST_F(DriveApiUrlGeneratorTest, GetFilelistUrl) {
  // Do not add q parameter if |search_string| is empty.
  EXPECT_EQ("https://www.googleapis.com/drive/v2/files?maxResults=50",
            url_generator_.GetFilelistUrl(std::string(), 50).spec());
  EXPECT_EQ("http://127.0.0.1:12345/drive/v2/files?maxResults=50",
            test_url_generator_.GetFilelistUrl(std::string(), 50).spec());

  // Set q parameter if non-empty |search_string| is given.
  EXPECT_EQ("https://www.googleapis.com/drive/v2/files?maxResults=50&q=query",
            url_generator_.GetFilelistUrl("query", 50).spec());
  EXPECT_EQ("http://127.0.0.1:12345/drive/v2/files?maxResults=50&q=query",
            test_url_generator_.GetFilelistUrl("query", 50).spec());
}

TEST_F(DriveApiUrlGeneratorTest, GetFileUrl) {
  // |file_id| should be embedded into the url.
  EXPECT_EQ("https://www.googleapis.com/drive/v2/files/0ADK06pfg",
            url_generator_.GetFileUrl("0ADK06pfg").spec());
  EXPECT_EQ("https://www.googleapis.com/drive/v2/files/0Bz0bd074",
            url_generator_.GetFileUrl("0Bz0bd074").spec());
  EXPECT_EQ("https://www.googleapis.com/drive/v2/files/file%3Afile_id",
            url_generator_.GetFileUrl("file:file_id").spec());

  EXPECT_EQ("http://127.0.0.1:12345/drive/v2/files/0ADK06pfg",
            test_url_generator_.GetFileUrl("0ADK06pfg").spec());
  EXPECT_EQ("http://127.0.0.1:12345/drive/v2/files/0Bz0bd074",
            test_url_generator_.GetFileUrl("0Bz0bd074").spec());
  EXPECT_EQ("http://127.0.0.1:12345/drive/v2/files/file%3Afile_id",
            test_url_generator_.GetFileUrl("file:file_id").spec());
}

TEST_F(DriveApiUrlGeneratorTest, GetFileCopyUrl) {
  // |file_id| should be embedded into the url.
  EXPECT_EQ("https://www.googleapis.com/drive/v2/files/0ADK06pfg/copy",
            url_generator_.GetFileCopyUrl("0ADK06pfg").spec());
  EXPECT_EQ("https://www.googleapis.com/drive/v2/files/0Bz0bd074/copy",
            url_generator_.GetFileCopyUrl("0Bz0bd074").spec());
  EXPECT_EQ("https://www.googleapis.com/drive/v2/files/file%3Afile_id/copy",
            url_generator_.GetFileCopyUrl("file:file_id").spec());

  EXPECT_EQ("http://127.0.0.1:12345/drive/v2/files/0ADK06pfg/copy",
            test_url_generator_.GetFileCopyUrl("0ADK06pfg").spec());
  EXPECT_EQ("http://127.0.0.1:12345/drive/v2/files/0Bz0bd074/copy",
            test_url_generator_.GetFileCopyUrl("0Bz0bd074").spec());
  EXPECT_EQ("http://127.0.0.1:12345/drive/v2/files/file%3Afile_id/copy",
            test_url_generator_.GetFileCopyUrl("file:file_id").spec());
}

TEST_F(DriveApiUrlGeneratorTest, GetChildrenUrl) {
  // |file_id| should be embedded into the url.
  EXPECT_EQ("https://www.googleapis.com/drive/v2/files/0ADK06pfg/children",
            url_generator_.GetChildrenUrl("0ADK06pfg").spec());
  EXPECT_EQ("https://www.googleapis.com/drive/v2/files/0Bz0bd074/children",
            url_generator_.GetChildrenUrl("0Bz0bd074").spec());
  EXPECT_EQ(
      "https://www.googleapis.com/drive/v2/files/file%3Afolder_id/children",
      url_generator_.GetChildrenUrl("file:folder_id").spec());

  EXPECT_EQ("http://127.0.0.1:12345/drive/v2/files/0ADK06pfg/children",
            test_url_generator_.GetChildrenUrl("0ADK06pfg").spec());
  EXPECT_EQ("http://127.0.0.1:12345/drive/v2/files/0Bz0bd074/children",
            test_url_generator_.GetChildrenUrl("0Bz0bd074").spec());
  EXPECT_EQ("http://127.0.0.1:12345/drive/v2/files/file%3Afolder_id/children",
            test_url_generator_.GetChildrenUrl("file:folder_id").spec());
}

TEST_F(DriveApiUrlGeneratorTest, GetChildrenUrlForRemoval) {
  // |file_id| should be embedded into the url.
  EXPECT_EQ(
      "https://www.googleapis.com/drive/v2/files/0ADK06pfg/children/0Bz0bd074",
      url_generator_.GetChildrenUrlForRemoval(
          "0ADK06pfg", "0Bz0bd074").spec());
  EXPECT_EQ(
      "https://www.googleapis.com/drive/v2/files/0Bz0bd074/children/0ADK06pfg",
      url_generator_.GetChildrenUrlForRemoval(
          "0Bz0bd074", "0ADK06pfg").spec());
  EXPECT_EQ(
      "https://www.googleapis.com/drive/v2/files/file%3Afolder_id/children"
      "/file%3Achild_id",
      url_generator_.GetChildrenUrlForRemoval(
          "file:folder_id", "file:child_id").spec());

  EXPECT_EQ(
      "http://127.0.0.1:12345/drive/v2/files/0ADK06pfg/children/0Bz0bd074",
      test_url_generator_.GetChildrenUrlForRemoval(
          "0ADK06pfg", "0Bz0bd074").spec());
  EXPECT_EQ(
      "http://127.0.0.1:12345/drive/v2/files/0Bz0bd074/children/0ADK06pfg",
      test_url_generator_.GetChildrenUrlForRemoval(
          "0Bz0bd074", "0ADK06pfg").spec());
  EXPECT_EQ(
      "http://127.0.0.1:12345/drive/v2/files/file%3Afolder_id/children/"
      "file%3Achild_id",
      test_url_generator_.GetChildrenUrlForRemoval(
          "file:folder_id", "file:child_id").spec());
}

TEST_F(DriveApiUrlGeneratorTest, GetFileTrashUrl) {
  // |file_id| should be embedded into the url.
  EXPECT_EQ("https://www.googleapis.com/drive/v2/files/0ADK06pfg/trash",
            url_generator_.GetFileTrashUrl("0ADK06pfg").spec());
  EXPECT_EQ("https://www.googleapis.com/drive/v2/files/0Bz0bd074/trash",
            url_generator_.GetFileTrashUrl("0Bz0bd074").spec());
  EXPECT_EQ("https://www.googleapis.com/drive/v2/files/file%3Afile_id/trash",
            url_generator_.GetFileTrashUrl("file:file_id").spec());

  EXPECT_EQ("http://127.0.0.1:12345/drive/v2/files/0ADK06pfg/trash",
            test_url_generator_.GetFileTrashUrl("0ADK06pfg").spec());
  EXPECT_EQ("http://127.0.0.1:12345/drive/v2/files/0Bz0bd074/trash",
            test_url_generator_.GetFileTrashUrl("0Bz0bd074").spec());
  EXPECT_EQ("http://127.0.0.1:12345/drive/v2/files/file%3Afile_id/trash",
            test_url_generator_.GetFileTrashUrl("file:file_id").spec());
}

TEST_F(DriveApiUrlGeneratorTest, GetInitiateUploadNewFileUrl) {
  EXPECT_EQ(
      "https://www.googleapis.com/upload/drive/v2/files?uploadType=resumable",
      url_generator_.GetInitiateUploadNewFileUrl().spec());

  EXPECT_EQ(
      "http://127.0.0.1:12345/upload/drive/v2/files?uploadType=resumable",
      test_url_generator_.GetInitiateUploadNewFileUrl().spec());
}

TEST_F(DriveApiUrlGeneratorTest, GetInitiateUploadExistingFileUrl) {
  // |resource_id| should be embedded into the url.
  EXPECT_EQ(
      "https://www.googleapis.com/upload/drive/v2/files/0ADK06pfg"
      "?uploadType=resumable",
      url_generator_.GetInitiateUploadExistingFileUrl("0ADK06pfg").spec());
  EXPECT_EQ(
      "https://www.googleapis.com/upload/drive/v2/files/0Bz0bd074"
      "?uploadType=resumable",
      url_generator_.GetInitiateUploadExistingFileUrl("0Bz0bd074").spec());
  EXPECT_EQ(
      "https://www.googleapis.com/upload/drive/v2/files/file%3Afile_id"
      "?uploadType=resumable",
      url_generator_.GetInitiateUploadExistingFileUrl("file:file_id").spec());

  EXPECT_EQ(
      "http://127.0.0.1:12345/upload/drive/v2/files/0ADK06pfg"
      "?uploadType=resumable",
      test_url_generator_.GetInitiateUploadExistingFileUrl(
          "0ADK06pfg").spec());
  EXPECT_EQ(
      "http://127.0.0.1:12345/upload/drive/v2/files/0Bz0bd074"
      "?uploadType=resumable",
      test_url_generator_.GetInitiateUploadExistingFileUrl(
          "0Bz0bd074").spec());
  EXPECT_EQ(
      "http://127.0.0.1:12345/upload/drive/v2/files/file%3Afile_id"
      "?uploadType=resumable",
      test_url_generator_.GetInitiateUploadExistingFileUrl(
          "file:file_id").spec());
}

}  // namespace google_apis
