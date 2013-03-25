// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/search_metadata.h"

#include "base/message_loop.h"
#include "base/prefs/pref_service.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/chromeos/drive/drive_cache.h"
#include "chrome/browser/chromeos/drive/drive_file_system.h"
#include "chrome/browser/chromeos/drive/drive_test_util.h"
#include "chrome/browser/chromeos/drive/drive_webapps_registry.h"
#include "chrome/browser/chromeos/drive/fake_free_disk_space_getter.h"
#include "chrome/browser/google_apis/fake_drive_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drive {

namespace {
const int kDefaultAtMostNumMatches = 10;
}

class SearchMetadataTest : public testing::Test {
 protected:
  SearchMetadataTest()
      : ui_thread_(content::BrowserThread::UI, &message_loop_) {
  }

  virtual void SetUp() OVERRIDE {
    profile_.reset(new TestingProfile);

    scoped_refptr<base::SequencedWorkerPool> pool =
        content::BrowserThread::GetBlockingPool();
    blocking_task_runner_ =
        pool->GetSequencedTaskRunner(pool->GetSequenceToken());

    fake_drive_service_.reset(new google_apis::FakeDriveService);
    fake_drive_service_->LoadResourceListForWapi(
        "chromeos/gdata/root_feed.json");
    fake_drive_service_->LoadAccountMetadataForWapi(
        "chromeos/gdata/account_metadata.json");
    fake_drive_service_->LoadAppListForDriveApi("chromeos/drive/applist.json");

    fake_free_disk_space_getter_.reset(new FakeFreeDiskSpaceGetter);

    drive_cache_.reset(new DriveCache(
        DriveCache::GetCacheRootPath(profile_.get()),
        blocking_task_runner_,
        fake_free_disk_space_getter_.get()));

    drive_webapps_registry_.reset(new DriveWebAppsRegistry);

    resource_metadata_.reset(new DriveResourceMetadata(
        fake_drive_service_->GetRootResourceId(),
        drive_cache_->GetCacheDirectoryPath(DriveCache::CACHE_TYPE_META),
        blocking_task_runner_));

    file_system_.reset(new DriveFileSystem(profile_.get(),
                                           drive_cache_.get(),
                                           fake_drive_service_.get(),
                                           NULL,  // uploader
                                           drive_webapps_registry_.get(),
                                           resource_metadata_.get(),
                                           blocking_task_runner_));
    file_system_->Initialize();

    DriveFileError error = DRIVE_FILE_ERROR_FAILED;
    resource_metadata_->Initialize(
        google_apis::test_util::CreateCopyResultCallback(&error));
    google_apis::test_util::RunBlockingPoolTask();
    ASSERT_EQ(DRIVE_FILE_OK, error);

    file_system_->change_list_loader()->LoadFromServerIfNeeded(
        DirectoryFetchInfo(),
        google_apis::test_util::CreateCopyResultCallback(&error));
    google_apis::test_util::RunBlockingPoolTask();
    ASSERT_EQ(DRIVE_FILE_OK, error);
  }

  virtual void TearDown() OVERRIDE {
    drive_cache_.reset();
  }

 protected:
  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  scoped_ptr<TestingProfile> profile_;
  scoped_ptr<google_apis::FakeDriveService> fake_drive_service_;
  scoped_ptr<FakeFreeDiskSpaceGetter> fake_free_disk_space_getter_;
  scoped_ptr<DriveCache, test_util::DestroyHelperForTests> drive_cache_;
  scoped_ptr<DriveWebAppsRegistry> drive_webapps_registry_;
  scoped_ptr<DriveResourceMetadata, test_util::DestroyHelperForTests>
      resource_metadata_;
  scoped_ptr<DriveFileSystem> file_system_;
};

TEST_F(SearchMetadataTest, SearchMetadata_ZeroMatches) {
  DriveFileError error = DRIVE_FILE_ERROR_FAILED;
  scoped_ptr<MetadataSearchResultVector> result;

  SearchMetadata(file_system_.get(),
                 "NonExistent",
                 kDefaultAtMostNumMatches,
                 google_apis::test_util::CreateCopyResultCallback(
                     &error, &result));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);
  ASSERT_EQ(0U, result->size());
}

TEST_F(SearchMetadataTest, SearchMetadata_RegularFile) {
  DriveFileError error = DRIVE_FILE_ERROR_FAILED;
  scoped_ptr<MetadataSearchResultVector> result;

  SearchMetadata(file_system_.get(),
                 "SubDirectory File 1.txt",
                 kDefaultAtMostNumMatches,
                 google_apis::test_util::CreateCopyResultCallback(
                     &error, &result));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);
  ASSERT_EQ(1U, result->size());
  EXPECT_EQ(base::FilePath::FromUTF8Unsafe(
      "drive/Directory 1/SubDirectory File 1.txt"), result->at(0).path);
}

// This test checks if |FindAndHighlight| does case-insensitive search.
// Tricker test cases for |FindAndHighlight| can be found below.
TEST_F(SearchMetadataTest, SearchMetadata_CaseInsensitiveSearch) {
  DriveFileError error = DRIVE_FILE_ERROR_FAILED;
  scoped_ptr<MetadataSearchResultVector> result;

  // The query is all in lower case.
  SearchMetadata(file_system_.get(),
                 "subdirectory file 1.txt",
                 kDefaultAtMostNumMatches,
                 google_apis::test_util::CreateCopyResultCallback(
                     &error, &result));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);
  ASSERT_EQ(1U, result->size());
  EXPECT_EQ(base::FilePath::FromUTF8Unsafe(
      "drive/Directory 1/SubDirectory File 1.txt"), result->at(0).path);
}

TEST_F(SearchMetadataTest, SearchMetadata_RegularFiles) {
  DriveFileError error = DRIVE_FILE_ERROR_FAILED;
  scoped_ptr<MetadataSearchResultVector> result;

  SearchMetadata(file_system_.get(),
                 "SubDir",
                 kDefaultAtMostNumMatches,
                 google_apis::test_util::CreateCopyResultCallback(
                     &error, &result));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);
  ASSERT_EQ(2U, result->size());

  // The results should be sorted by the last accessed time.
  EXPECT_EQ(12970368000000000LL,
            result->at(0).entry_proto.file_info().last_accessed());
  EXPECT_EQ(12969849600000000LL,
            result->at(1).entry_proto.file_info().last_accessed());

  // All base names should contain "File".
  EXPECT_EQ(
      base::FilePath::FromUTF8Unsafe(
          "drive/Slash \xE2\x88\x95 in directory/Slash SubDir File.txt"),
      result->at(0).path);
  EXPECT_EQ(base::FilePath::FromUTF8Unsafe(
      "drive/Directory 1/SubDirectory File 1.txt"), result->at(1).path);
}

TEST_F(SearchMetadataTest, SearchMetadata_AtMostOneFile) {
  DriveFileError error = DRIVE_FILE_ERROR_FAILED;
  scoped_ptr<MetadataSearchResultVector> result;

  // There are two files matching "SubDir" but only one file should be
  // returned.
  SearchMetadata(file_system_.get(),
                 "SubDir",
                 1,  // at_most_num_matches
                 google_apis::test_util::CreateCopyResultCallback(
                     &error, &result));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);
  ASSERT_EQ(1U, result->size());
  EXPECT_EQ(
      base::FilePath::FromUTF8Unsafe(
          "drive/Slash \xE2\x88\x95 in directory/Slash SubDir File.txt"),
      result->at(0).path);
}

TEST_F(SearchMetadataTest, SearchMetadata_Directory) {
  DriveFileError error = DRIVE_FILE_ERROR_FAILED;
  scoped_ptr<MetadataSearchResultVector> result;

  SearchMetadata(file_system_.get(),
                 "Directory 1",
                 kDefaultAtMostNumMatches,
                 google_apis::test_util::CreateCopyResultCallback(
                     &error, &result));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);
  ASSERT_EQ(1U, result->size());
  EXPECT_EQ(
      base::FilePath::FromUTF8Unsafe("drive/Directory 1"),
      result->at(0).path);
}

TEST_F(SearchMetadataTest, SearchMetadata_HostedDocument) {
  DriveFileError error = DRIVE_FILE_ERROR_FAILED;
  scoped_ptr<MetadataSearchResultVector> result;

  SearchMetadata(file_system_.get(),
                 "Document",
                 kDefaultAtMostNumMatches,
                 google_apis::test_util::CreateCopyResultCallback(
                     &error, &result));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);
  ASSERT_EQ(1U, result->size());

  EXPECT_EQ(base::FilePath::FromUTF8Unsafe("drive/Document 1.gdoc"),
            result->at(0).path);
}

TEST_F(SearchMetadataTest, SearchMetadata_HideHostedDocument) {
  // This test is similar to HostedDocument test above, but change a
  // preference to hide hosted documents from the result.
  PrefService* pref_service = profile_->GetPrefs();
  pref_service->SetBoolean(prefs::kDisableDriveHostedFiles, true);

  DriveFileError error = DRIVE_FILE_ERROR_FAILED;
  scoped_ptr<MetadataSearchResultVector> result;

  SearchMetadata(file_system_.get(),
                 "Document",
                 kDefaultAtMostNumMatches,
                 google_apis::test_util::CreateCopyResultCallback(
                     &error, &result));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);
  ASSERT_EQ(0U, result->size());
}

TEST(SearchMetadataSimpleTest, FindAndHighlight_ZeroMatches) {
  std::string highlighted_text;
  EXPECT_FALSE(FindAndHighlight("text", "query", &highlighted_text));
}

TEST(SearchMetadataSimpleTest, FindAndHighlight_EmptyQuery) {
  std::string highlighted_text;
  EXPECT_FALSE(FindAndHighlight("text", "", &highlighted_text));
}

TEST(SearchMetadataSimpleTest, FindAndHighlight_EmptyText) {
  std::string highlighted_text;
  EXPECT_FALSE(FindAndHighlight("", "query", &highlighted_text));
}

TEST(SearchMetadataSimpleTest, FindAndHighlight_EmptyTextAndQuery) {
  std::string highlighted_text;
  EXPECT_FALSE(FindAndHighlight("", "", &highlighted_text));
}

TEST(SearchMetadataSimpleTest, FindAndHighlight_FullMatch) {
  std::string highlighted_text;
  EXPECT_TRUE(FindAndHighlight("hello", "hello", &highlighted_text));
  EXPECT_EQ("<b>hello</b>", highlighted_text);
}

TEST(SearchMetadataSimpleTest, FindAndHighlight_StartWith) {
  std::string highlighted_text;
  EXPECT_TRUE(FindAndHighlight("hello, world", "hello", &highlighted_text));
  EXPECT_EQ("<b>hello</b>, world", highlighted_text);
}

TEST(SearchMetadataSimpleTest, FindAndHighlight_EndWith) {
  std::string highlighted_text;
  EXPECT_TRUE(FindAndHighlight("hello, world", "world", &highlighted_text));
  EXPECT_EQ("hello, <b>world</b>", highlighted_text);
}

TEST(SearchMetadataSimpleTest, FindAndHighlight_InTheMiddle) {
  std::string highlighted_text;
  EXPECT_TRUE(FindAndHighlight("yo hello, world", "hello", &highlighted_text));
  EXPECT_EQ("yo <b>hello</b>, world", highlighted_text);
}

TEST(SearchMetadataSimpleTest, FindAndHighlight_MultipeMatches) {
  std::string highlighted_text;
  EXPECT_TRUE(FindAndHighlight("yoyoyoyoy", "yoy", &highlighted_text));
  EXPECT_EQ("<b>yoy</b>o<b>yoy</b>oy", highlighted_text);
}

TEST(SearchMetadataSimpleTest, FindAndHighlight_IgnoreCase) {
  std::string highlighted_text;
  EXPECT_TRUE(FindAndHighlight("HeLLo", "hello", &highlighted_text));
  EXPECT_EQ("<b>HeLLo</b>", highlighted_text);
}

TEST(SearchMetadataSimpleTest, FindAndHighlight_MetaChars) {
  std::string highlighted_text;
  EXPECT_TRUE(FindAndHighlight("<hello>", "hello", &highlighted_text));
  EXPECT_EQ("&lt;<b>hello</b>&gt;", highlighted_text);
}

TEST(SearchMetadataSimpleTest, FindAndHighlight_MoreMetaChars) {
  std::string highlighted_text;
  EXPECT_TRUE(FindAndHighlight("a&b&c&d", "b&c", &highlighted_text));
  EXPECT_EQ("a&amp;<b>b&amp;c</b>&amp;d", highlighted_text);
}

}   // namespace drive
