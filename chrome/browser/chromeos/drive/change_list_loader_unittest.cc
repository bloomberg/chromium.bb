// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/change_list_loader.h"

#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "chrome/browser/chromeos/drive/file_cache.h"
#include "chrome/browser/chromeos/drive/job_scheduler.h"
#include "chrome/browser/chromeos/drive/resource_metadata.h"
#include "chrome/browser/chromeos/drive/test_util.h"
#include "chrome/browser/drive/fake_drive_service.h"
#include "chrome/browser/google_apis/test_util.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drive {
namespace internal {

class ChangeListLoaderTest : public testing::Test {
 protected:
  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    profile_.reset(new TestingProfile);

    drive_service_.reset(new FakeDriveService);
    ASSERT_TRUE(drive_service_->LoadResourceListForWapi(
        "chromeos/gdata/root_feed.json"));
    ASSERT_TRUE(drive_service_->LoadAccountMetadataForWapi(
        "chromeos/gdata/account_metadata.json"));

    scheduler_.reset(new JobScheduler(profile_.get(), drive_service_.get(),
                                      base::MessageLoopProxy::current()));
    metadata_storage_.reset(new ResourceMetadataStorage(
        temp_dir_.path(), base::MessageLoopProxy::current()));
    ASSERT_TRUE(metadata_storage_->Initialize());

    metadata_.reset(new ResourceMetadata(metadata_storage_.get(),
                                         base::MessageLoopProxy::current()));
    ASSERT_EQ(FILE_ERROR_OK, metadata_->Initialize());

    cache_.reset(new FileCache(metadata_storage_.get(),
                               temp_dir_.path(),
                               base::MessageLoopProxy::current(),
                               NULL /* free_disk_space_getter */));
    ASSERT_TRUE(cache_->Initialize());

    change_list_loader_.reset(new ChangeListLoader(
        base::MessageLoopProxy::current(), metadata_.get(), scheduler_.get()));
  }

  content::TestBrowserThreadBundle thread_bundle_;
  base::ScopedTempDir temp_dir_;
  scoped_ptr<TestingProfile> profile_;
  scoped_ptr<FakeDriveService> drive_service_;
  scoped_ptr<JobScheduler> scheduler_;
  scoped_ptr<ResourceMetadataStorage,
             test_util::DestroyHelperForTests> metadata_storage_;
  scoped_ptr<ResourceMetadata, test_util::DestroyHelperForTests> metadata_;
  scoped_ptr<FileCache, test_util::DestroyHelperForTests> cache_;
  scoped_ptr<ChangeListLoader> change_list_loader_;
};

TEST_F(ChangeListLoaderTest, LoadIfNeeded) {
  EXPECT_FALSE(change_list_loader_->IsRefreshing());

  // Start initial load.
  FileError error = FILE_ERROR_FAILED;
  change_list_loader_->LoadIfNeeded(
      DirectoryFetchInfo(),
      google_apis::test_util::CreateCopyResultCallback(&error));
  EXPECT_TRUE(change_list_loader_->IsRefreshing());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(FILE_ERROR_OK, error);

  EXPECT_FALSE(change_list_loader_->IsRefreshing());
  EXPECT_LT(0, metadata_->GetLargestChangestamp());
  EXPECT_EQ(1, drive_service_->resource_list_load_count());

  // Reload. This should result in no-op.
  int64 previous_changestamp = metadata_->GetLargestChangestamp();
  int previous_resource_list_load_count =
      drive_service_->resource_list_load_count();
  change_list_loader_->LoadIfNeeded(
      DirectoryFetchInfo(),
      google_apis::test_util::CreateCopyResultCallback(&error));
  EXPECT_FALSE(change_list_loader_->IsRefreshing());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(FILE_ERROR_OK, error);

  EXPECT_FALSE(change_list_loader_->IsRefreshing());
  EXPECT_EQ(previous_changestamp, metadata_->GetLargestChangestamp());
  EXPECT_EQ(previous_resource_list_load_count,
            drive_service_->resource_list_load_count());
}

}  // namespace internal
}  // namespace drive
