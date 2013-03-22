// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/stringprintf.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/values.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/drive_file_system.h"
#include "chrome/browser/chromeos/drive/drive_test_util.h"
#include "chrome/browser/chromeos/drive/drive_webapps_registry.h"
#include "chrome/browser/chromeos/drive/fake_free_disk_space_getter.h"
#include "chrome/browser/chromeos/drive/mock_directory_change_observer.h"
#include "chrome/browser/chromeos/drive/mock_drive_cache_observer.h"
#include "chrome/browser/chromeos/drive/stale_cache_files_remover.h"
#include "chrome/browser/google_apis/drive_api_parser.h"
#include "chrome/browser/google_apis/fake_drive_service.h"
#include "chrome/browser/google_apis/test_util.h"
#include "chrome/browser/google_apis/time_util.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::AtLeast;
using ::testing::Eq;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::_;

namespace drive {
namespace {

const int64 kLotsOfSpace = kMinFreeSpace * 10;

}  // namespace

class StaleCacheFilesRemoverTest : public testing::Test {
 protected:
  StaleCacheFilesRemoverTest()
      : ui_thread_(content::BrowserThread::UI, &message_loop_),
        cache_(NULL),
        file_system_(NULL),
        fake_drive_service_(NULL),
        drive_webapps_registry_(NULL),
        root_feed_changestamp_(0) {
  }

  virtual void SetUp() OVERRIDE {
    profile_.reset(new TestingProfile);

    fake_drive_service_.reset(new google_apis::FakeDriveService);
    fake_drive_service_->LoadResourceListForWapi(
        "chromeos/gdata/root_feed.json");
    fake_drive_service_->LoadAccountMetadataForWapi(
        "chromeos/gdata/account_metadata.json");
    fake_drive_service_->LoadAppListForDriveApi("chromeos/drive/applist.json");

    fake_free_disk_space_getter_.reset(new FakeFreeDiskSpaceGetter);

    scoped_refptr<base::SequencedWorkerPool> pool =
        content::BrowserThread::GetBlockingPool();
    blocking_task_runner_ =
        pool->GetSequencedTaskRunner(pool->GetSequenceToken());

    // Likewise, this will be owned by DriveFileSystem.
    cache_.reset(new DriveCache(DriveCache::GetCacheRootPath(profile_.get()),
                                blocking_task_runner_,
                                fake_free_disk_space_getter_.get()));

    drive_webapps_registry_.reset(new DriveWebAppsRegistry);

    resource_metadata_.reset(new DriveResourceMetadata(
        fake_drive_service_->GetRootResourceId(),
        cache_->GetCacheDirectoryPath(DriveCache::CACHE_TYPE_META),
        blocking_task_runner_));

    ASSERT_FALSE(file_system_);
    file_system_ = new DriveFileSystem(profile_.get(),
                                       cache_.get(),
                                       fake_drive_service_.get(),
                                       NULL,  // drive_uploader
                                       drive_webapps_registry_.get(),
                                       resource_metadata_.get(),
                                       blocking_task_runner_);

    mock_cache_observer_.reset(new StrictMock<MockDriveCacheObserver>);
    cache_->AddObserver(mock_cache_observer_.get());

    mock_directory_observer_.reset(new StrictMock<MockDirectoryChangeObserver>);
    file_system_->AddObserver(mock_directory_observer_.get());

    file_system_->Initialize();
    cache_->RequestInitializeForTesting();
    google_apis::test_util::RunBlockingPoolTask();

    DriveFileError error = DRIVE_FILE_ERROR_FAILED;
    resource_metadata_->Initialize(
        google_apis::test_util::CreateCopyResultCallback(&error));
    google_apis::test_util::RunBlockingPoolTask();
    ASSERT_EQ(DRIVE_FILE_OK, error);

    stale_cache_files_remover_.reset(new StaleCacheFilesRemover(file_system_,
                                                                cache_.get()));

    google_apis::test_util::RunBlockingPoolTask();
  }

  virtual void TearDown() OVERRIDE {
    ASSERT_TRUE(file_system_);
    stale_cache_files_remover_.reset();
    delete file_system_;
    file_system_ = NULL;
    cache_.reset();
    profile_.reset(NULL);
  }

  MessageLoopForUI message_loop_;
  // The order of the test threads is important, do not change the order.
  // See also content/browser/browser_thread_impl.cc.
  content::TestBrowserThread ui_thread_;
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  scoped_ptr<TestingProfile> profile_;
  scoped_ptr<DriveCache, test_util::DestroyHelperForTests> cache_;
  DriveFileSystem* file_system_;
  scoped_ptr<google_apis::FakeDriveService> fake_drive_service_;
  scoped_ptr<DriveWebAppsRegistry> drive_webapps_registry_;
  scoped_ptr<DriveResourceMetadata, test_util::DestroyHelperForTests>
      resource_metadata_;
  scoped_ptr<FakeFreeDiskSpaceGetter> fake_free_disk_space_getter_;
  scoped_ptr<StrictMock<MockDriveCacheObserver> > mock_cache_observer_;
  scoped_ptr<StrictMock<MockDirectoryChangeObserver> > mock_directory_observer_;
  scoped_ptr<StaleCacheFilesRemover> stale_cache_files_remover_;

  int root_feed_changestamp_;
};

TEST_F(StaleCacheFilesRemoverTest, RemoveStaleCacheFiles) {
  base::FilePath dummy_file =
      google_apis::test_util::GetTestFilePath("chromeos/gdata/root_feed.json");
  std::string resource_id("pdf:1a2b3c");
  std::string md5("abcdef0123456789");

  fake_free_disk_space_getter_->set_fake_free_disk_space(kLotsOfSpace);

  // Create a stale cache file.
  DriveFileError error = DRIVE_FILE_OK;
  cache_->Store(resource_id, md5, dummy_file, DriveCache::FILE_OPERATION_COPY,
                google_apis::test_util::CreateCopyResultCallback(&error));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);

  // Verify that the cache entry exists.
  bool success = false;
  DriveCacheEntry cache_entry;
  cache_->GetCacheEntry(
      resource_id, md5,
      base::Bind(&test_util::CopyResultsFromGetCacheEntryCallback,
                 &success,
                 &cache_entry));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_TRUE(success);

  base::FilePath unused;
  scoped_ptr<DriveEntryProto> entry_proto;
  file_system_->GetEntryInfoByResourceId(
      resource_id,
      base::Bind(&test_util::CopyResultsFromGetEntryInfoWithFilePathCallback,
                 &error,
                 &unused,
                 &entry_proto));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_ERROR_NOT_FOUND, error);
  EXPECT_FALSE(entry_proto.get());

  // Load a root feed again to kick the StaleCacheFilesRemover.
  file_system_->Reload();

  // Wait for StaleCacheFilesRemover to finish cleaning up the stale file.
  google_apis::test_util::RunBlockingPoolTask();

  // Verify that the cache entry is deleted.
  cache_->GetCacheEntry(
      resource_id, md5,
      base::Bind(&test_util::CopyResultsFromGetCacheEntryCallback,
                 &success,
                 &cache_entry));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_FALSE(success);
}

}   // namespace drive
