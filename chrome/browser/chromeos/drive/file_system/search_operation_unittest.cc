// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/file_system/search_operation.h"

#include "chrome/browser/chromeos/drive/file_system/operation_test_base.h"
#include "chrome/browser/drive/fake_drive_service.h"
#include "chrome/browser/google_apis/gdata_wapi_parser.h"
#include "chrome/browser/google_apis/test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drive {
namespace file_system {

namespace {

struct SearchResultPair {
  const char* path;
  const bool is_directory;
};

}  // namespace

typedef OperationTestBase SearchOperationTest;

TEST_F(SearchOperationTest, ContentSearch) {
  SearchOperation operation(blocking_task_runner(), scheduler(), metadata());

  const SearchResultPair kExpectedResults[] = {
    { "drive/root/Directory 1/Sub Directory Folder/Sub Sub Directory Folder",
      true },
    { "drive/root/Directory 1/Sub Directory Folder", true },
    { "drive/root/Directory 1/SubDirectory File 1.txt", false },
    { "drive/root/Directory 1", true },
    { "drive/root/Directory 2 excludeDir-test", true },
  };

  FileError error = FILE_ERROR_FAILED;
  GURL next_link;
  scoped_ptr<std::vector<SearchResultInfo> > results;

  operation.Search("Directory", GURL(),
                   google_apis::test_util::CreateCopyResultCallback(
                       &error, &next_link, &results));
  test_util::RunBlockingPoolTask();

  EXPECT_EQ(FILE_ERROR_OK, error);
  EXPECT_TRUE(next_link.is_empty());
  EXPECT_EQ(ARRAYSIZE_UNSAFE(kExpectedResults), results->size());
  for (size_t i = 0; i < results->size(); i++) {
    EXPECT_EQ(kExpectedResults[i].path, results->at(i).path.AsUTF8Unsafe());
    EXPECT_EQ(kExpectedResults[i].is_directory, results->at(i).is_directory);
  }
}

TEST_F(SearchOperationTest, ContentSearchWithNewEntry) {
  SearchOperation operation(blocking_task_runner(), scheduler(), metadata());

  // Create a new directory in the drive service.
  google_apis::GDataErrorCode gdata_error = google_apis::GDATA_OTHER_ERROR;
  scoped_ptr<google_apis::ResourceEntry> resource_entry;
  fake_service()->AddNewDirectory(
      fake_service()->GetRootResourceId(),
      "New Directory 1!",
      google_apis::test_util::CreateCopyResultCallback(
          &gdata_error, &resource_entry));
  test_util::RunBlockingPoolTask();
  ASSERT_EQ(google_apis::HTTP_CREATED, gdata_error);

  // As the result of the first Search(), only entries in the current file
  // system snapshot are expected to be returned in the "right" path. New
  // entries like "New Directory 1!" is temporarily added to "drive/other".
  const SearchResultPair kExpectedResultsBeforeLoad[] = {
      { "drive/root/Directory 1", true },
      { "drive/other/New Directory 1!", true },
  };

  FileError error = FILE_ERROR_FAILED;
  GURL next_link;
  scoped_ptr<std::vector<SearchResultInfo> > results;

  operation.Search("\"Directory 1\"", GURL(),
                   google_apis::test_util::CreateCopyResultCallback(
                       &error, &next_link, &results));
  test_util::RunBlockingPoolTask();

  EXPECT_EQ(FILE_ERROR_OK, error);
  EXPECT_TRUE(next_link.is_empty());
  ASSERT_EQ(ARRAYSIZE_UNSAFE(kExpectedResultsBeforeLoad), results->size());
  for (size_t i = 0; i < results->size(); i++) {
    EXPECT_EQ(kExpectedResultsBeforeLoad[i].path,
              results->at(i).path.AsUTF8Unsafe());
    EXPECT_EQ(kExpectedResultsBeforeLoad[i].is_directory,
              results->at(i).is_directory);
  }

  // Load the change from FakeDriveService.
  ASSERT_EQ(FILE_ERROR_OK, CheckForUpdates());

  // Now the new entry must be reported to be in the right directory.
  const SearchResultPair kExpectedResultsAfterLoad[] = {
      { "drive/root/Directory 1", true },
      { "drive/root/New Directory 1!", true },
  };
  error = FILE_ERROR_FAILED;
  operation.Search("\"Directory 1\"", GURL(),
                   google_apis::test_util::CreateCopyResultCallback(
                       &error, &next_link, &results));
  test_util::RunBlockingPoolTask();

  EXPECT_EQ(FILE_ERROR_OK, error);
  EXPECT_TRUE(next_link.is_empty());
  ASSERT_EQ(ARRAYSIZE_UNSAFE(kExpectedResultsAfterLoad), results->size());
  for (size_t i = 0; i < results->size(); i++) {
    EXPECT_EQ(kExpectedResultsAfterLoad[i].path,
              results->at(i).path.AsUTF8Unsafe());
    EXPECT_EQ(kExpectedResultsAfterLoad[i].is_directory,
              results->at(i).is_directory);
  }
}

TEST_F(SearchOperationTest, ContentSearchEmptyResult) {
  SearchOperation operation(blocking_task_runner(), scheduler(), metadata());

  FileError error = FILE_ERROR_FAILED;
  GURL next_link;
  scoped_ptr<std::vector<SearchResultInfo> > results;

  operation.Search("\"no-match query\"", GURL(),
                   google_apis::test_util::CreateCopyResultCallback(
                       &error, &next_link, &results));
  test_util::RunBlockingPoolTask();

  EXPECT_EQ(FILE_ERROR_OK, error);
  EXPECT_TRUE(next_link.is_empty());
  EXPECT_EQ(0U, results->size());
}

}  // namespace file_system
}  // namespace drive
