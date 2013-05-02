// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/drive_webapps_registry.h"
#include "chrome/browser/chromeos/drive/fake_free_disk_space_getter.h"
#include "chrome/browser/chromeos/drive/file_system.h"
#include "chrome/browser/chromeos/drive/job_scheduler.h"
#include "chrome/browser/chromeos/drive/stale_cache_files_remover.h"
#include "chrome/browser/chromeos/drive/test_util.h"
#include "chrome/browser/google_apis/fake_drive_service.h"
#include "chrome/browser/google_apis/test_util.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drive {
namespace {

const int64 kLotsOfSpace = kMinFreeSpace * 10;

}  // namespace

class StaleCacheFilesRemoverTest : public testing::Test {
 protected:
  StaleCacheFilesRemoverTest()
      : ui_thread_(content::BrowserThread::UI, &message_loop_) {
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

    scheduler_.reset(new JobScheduler(profile_.get(),
                                      fake_drive_service_.get()));

    scoped_refptr<base::SequencedWorkerPool> pool =
        content::BrowserThread::GetBlockingPool();
    blocking_task_runner_ =
        pool->GetSequencedTaskRunner(pool->GetSequenceToken());

    cache_.reset(new FileCache(util::GetCacheRootPath(profile_.get()),
                               blocking_task_runner_,
                               fake_free_disk_space_getter_.get()));

    drive_webapps_registry_.reset(new DriveWebAppsRegistry);

    resource_metadata_.reset(new internal::ResourceMetadata(
        cache_->GetCacheDirectoryPath(FileCache::CACHE_TYPE_META),
        blocking_task_runner_));

    file_system_.reset(new FileSystem(profile_.get(),
                                      cache_.get(),
                                      fake_drive_service_.get(),
                                      scheduler_.get(),
                                      drive_webapps_registry_.get(),
                                      resource_metadata_.get(),
                                      blocking_task_runner_));

    file_system_->Initialize();
    cache_->RequestInitializeForTesting();
    google_apis::test_util::RunBlockingPoolTask();

    FileError error = FILE_ERROR_FAILED;
    resource_metadata_->Initialize(
        google_apis::test_util::CreateCopyResultCallback(&error));
    google_apis::test_util::RunBlockingPoolTask();
    ASSERT_EQ(FILE_ERROR_OK, error);

    stale_cache_files_remover_.reset(
        new StaleCacheFilesRemover(file_system_.get(), cache_.get()));

    google_apis::test_util::RunBlockingPoolTask();
  }

  virtual void TearDown() OVERRIDE {
    stale_cache_files_remover_.reset();
    file_system_.reset();
    cache_.reset();
    profile_.reset();
  }

  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;

  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  scoped_ptr<TestingProfile> profile_;
  scoped_ptr<FileCache, test_util::DestroyHelperForTests> cache_;
  scoped_ptr<FileSystem> file_system_;
  scoped_ptr<google_apis::FakeDriveService> fake_drive_service_;
  scoped_ptr<JobScheduler> scheduler_;
  scoped_ptr<DriveWebAppsRegistry> drive_webapps_registry_;
  scoped_ptr<internal::ResourceMetadata, test_util::DestroyHelperForTests>
      resource_metadata_;
  scoped_ptr<FakeFreeDiskSpaceGetter> fake_free_disk_space_getter_;
  scoped_ptr<StaleCacheFilesRemover> stale_cache_files_remover_;
};

TEST_F(StaleCacheFilesRemoverTest, RemoveStaleCacheFiles) {
  base::FilePath dummy_file =
      google_apis::test_util::GetTestFilePath("chromeos/gdata/root_feed.json");
  std::string resource_id("pdf:1a2b3c");
  std::string md5("abcdef0123456789");

  fake_free_disk_space_getter_->set_fake_free_disk_space(kLotsOfSpace);

  // Create a stale cache file.
  FileError error = FILE_ERROR_OK;
  cache_->Store(resource_id, md5, dummy_file, FileCache::FILE_OPERATION_COPY,
                google_apis::test_util::CreateCopyResultCallback(&error));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);

  // Verify that the cache entry exists.
  bool success = false;
  FileCacheEntry cache_entry;
  cache_->GetCacheEntry(resource_id, md5,
                        google_apis::test_util::CreateCopyResultCallback(
                            &success, &cache_entry));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_TRUE(success);

  base::FilePath unused;
  scoped_ptr<ResourceEntry> entry;
  file_system_->GetEntryInfoByResourceId(
      resource_id,
      google_apis::test_util::CreateCopyResultCallback(
          &error, &unused, &entry));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, error);
  EXPECT_FALSE(entry.get());

  // Load a root feed again to kick the StaleCacheFilesRemover.
  file_system_->Reload();

  // Wait for StaleCacheFilesRemover to finish cleaning up the stale file.
  google_apis::test_util::RunBlockingPoolTask();

  // Verify that the cache entry is deleted.
  cache_->GetCacheEntry(resource_id, md5,
                        google_apis::test_util::CreateCopyResultCallback(
                            &success, &cache_entry));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_FALSE(success);
}

}   // namespace drive
