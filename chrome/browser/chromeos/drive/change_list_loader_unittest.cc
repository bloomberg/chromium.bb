// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/change_list_loader.h"

#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/testing_pref_service.h"
#include "base/run_loop.h"
#include "chrome/browser/chromeos/drive/change_list_loader_observer.h"
#include "chrome/browser/chromeos/drive/change_list_processor.h"
#include "chrome/browser/chromeos/drive/file_cache.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/drive/job_scheduler.h"
#include "chrome/browser/chromeos/drive/resource_metadata.h"
#include "chrome/browser/chromeos/drive/test_util.h"
#include "chrome/browser/drive/fake_drive_service.h"
#include "chrome/browser/google_apis/test_util.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drive {
namespace internal {

class TestChangeListLoaderObserver : public ChangeListLoaderObserver {
 public:
  explicit TestChangeListLoaderObserver(ChangeListLoader* loader)
      : loader_(loader),
        load_from_server_complete_count_(0),
        initial_load_complete_count_(0) {
    loader_->AddObserver(this);
  }

  virtual ~TestChangeListLoaderObserver() {
    loader_->RemoveObserver(this);
  }

  const std::set<base::FilePath>& changed_directories() const {
    return changed_directories_;
  }
  void clear_changed_directories() { changed_directories_.clear(); }

  int load_from_server_complete_count() const {
    return load_from_server_complete_count_;
  }
  int initial_load_complete_count() const {
    return initial_load_complete_count_;
  }

  // ChageListObserver overrides:
  virtual void OnDirectoryChanged(
      const base::FilePath& directory_path) OVERRIDE {
    changed_directories_.insert(directory_path);
  }
  virtual void OnLoadFromServerComplete() OVERRIDE {
    ++load_from_server_complete_count_;
  }
  virtual void OnInitialLoadComplete() OVERRIDE {
    ++initial_load_complete_count_;
  }

 private:
  ChangeListLoader* loader_;
  std::set<base::FilePath> changed_directories_;
  int load_from_server_complete_count_;
  int initial_load_complete_count_;

  DISALLOW_COPY_AND_ASSIGN(TestChangeListLoaderObserver);
};

class ChangeListLoaderTest : public testing::Test {
 protected:
  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    pref_service_.reset(new TestingPrefServiceSimple);
    test_util::RegisterDrivePrefs(pref_service_->registry());

    drive_service_.reset(new FakeDriveService);
    ASSERT_TRUE(drive_service_->LoadResourceListForWapi(
        "gdata/root_feed.json"));
    ASSERT_TRUE(drive_service_->LoadAccountMetadataForWapi(
        "gdata/account_metadata.json"));

    scheduler_.reset(new JobScheduler(pref_service_.get(),
                                      drive_service_.get(),
                                      base::MessageLoopProxy::current().get()));
    metadata_storage_.reset(new ResourceMetadataStorage(
        temp_dir_.path(), base::MessageLoopProxy::current().get()));
    ASSERT_TRUE(metadata_storage_->Initialize());

    metadata_.reset(new ResourceMetadata(
        metadata_storage_.get(), base::MessageLoopProxy::current().get()));
    ASSERT_EQ(FILE_ERROR_OK, metadata_->Initialize());

    cache_.reset(new FileCache(metadata_storage_.get(),
                               temp_dir_.path(),
                               base::MessageLoopProxy::current().get(),
                               NULL /* free_disk_space_getter */));
    ASSERT_TRUE(cache_->Initialize());

    change_list_loader_.reset(
        new ChangeListLoader(base::MessageLoopProxy::current().get(),
                             metadata_.get(),
                             scheduler_.get(),
                             drive_service_.get()));
  }

  // Adds a new file to the root directory of the service.
  scoped_ptr<google_apis::ResourceEntry> AddNewFile(const std::string& title) {
    google_apis::GDataErrorCode error = google_apis::GDATA_FILE_ERROR;
    scoped_ptr<google_apis::ResourceEntry> entry;
    drive_service_->AddNewFile(
        "text/plain",
        "content text",
        drive_service_->GetRootResourceId(),
        title,
        false,  // shared_with_me
        google_apis::test_util::CreateCopyResultCallback(&error, &entry));
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(google_apis::HTTP_CREATED, error);
    return entry.Pass();
  }

  content::TestBrowserThreadBundle thread_bundle_;
  base::ScopedTempDir temp_dir_;
  scoped_ptr<TestingPrefServiceSimple> pref_service_;
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
  TestChangeListLoaderObserver observer(change_list_loader_.get());

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
  EXPECT_EQ(1, observer.initial_load_complete_count());
  EXPECT_EQ(1, observer.load_from_server_complete_count());
  EXPECT_TRUE(observer.changed_directories().empty());

  base::FilePath file_path =
      util::GetDriveMyDriveRootPath().AppendASCII("File 1.txt");
  ResourceEntry entry;
  EXPECT_EQ(FILE_ERROR_OK,
            metadata_->GetResourceEntryByPath(file_path, &entry));

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

TEST_F(ChangeListLoaderTest, LoadIfNeeded_LocalMetadataAvailable) {
  // Prepare metadata.
  FileError error = FILE_ERROR_FAILED;
  change_list_loader_->LoadIfNeeded(
      DirectoryFetchInfo(),
      google_apis::test_util::CreateCopyResultCallback(&error));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(FILE_ERROR_OK, error);

  // Reset loader.
  change_list_loader_.reset(
      new ChangeListLoader(base::MessageLoopProxy::current().get(),
                           metadata_.get(),
                           scheduler_.get(),
                           drive_service_.get()));

  // Add a file to the service.
  scoped_ptr<google_apis::ResourceEntry> gdata_entry = AddNewFile("New File");
  ASSERT_TRUE(gdata_entry);

  // Start loading. Because local metadata is available, the load results in
  // returning FILE_ERROR_OK without fetching full list of resources.
  const int previous_resource_list_load_count =
      drive_service_->resource_list_load_count();
  TestChangeListLoaderObserver observer(change_list_loader_.get());

  change_list_loader_->LoadIfNeeded(
      DirectoryFetchInfo(),
      google_apis::test_util::CreateCopyResultCallback(&error));
  EXPECT_TRUE(change_list_loader_->IsRefreshing());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(FILE_ERROR_OK, error);
  EXPECT_EQ(previous_resource_list_load_count,
            drive_service_->resource_list_load_count());
  EXPECT_EQ(1, observer.initial_load_complete_count());

  // Update should be checked by LoadIfNeeded().
  EXPECT_EQ(drive_service_->largest_changestamp(),
            metadata_->GetLargestChangestamp());
  EXPECT_EQ(1, drive_service_->change_list_load_count());
  EXPECT_EQ(1, observer.load_from_server_complete_count());
  EXPECT_EQ(1U, observer.changed_directories().count(
      util::GetDriveMyDriveRootPath()));

  base::FilePath file_path =
      util::GetDriveMyDriveRootPath().AppendASCII(gdata_entry->title());
  ResourceEntry entry;
  EXPECT_EQ(FILE_ERROR_OK,
            metadata_->GetResourceEntryByPath(file_path, &entry));
}

TEST_F(ChangeListLoaderTest, LoadIfNeeded_MyDrive) {
  TestChangeListLoaderObserver observer(change_list_loader_.get());

  // Emulate the slowness of GetAllResourceList().
  drive_service_->set_never_return_all_resource_list(true);

  // Load grand root.
  FileError error = FILE_ERROR_FAILED;
  change_list_loader_->LoadIfNeeded(
      DirectoryFetchInfo(util::kDriveGrandRootSpecialResourceId, 0),
      google_apis::test_util::CreateCopyResultCallback(&error));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(FILE_ERROR_OK, error);
  EXPECT_EQ(1U, observer.changed_directories().count(
      util::GetDriveGrandRootPath()));
  observer.clear_changed_directories();

  // GetAllResourceList() was called.
  EXPECT_EQ(1, drive_service_->blocked_resource_list_load_count());

  // My Drive is present in the local metadata, but its child is not.
  ResourceEntry entry;
  EXPECT_EQ(FILE_ERROR_OK,
            metadata_->GetResourceEntryByPath(util::GetDriveMyDriveRootPath(),
                                              &entry));
  const int64 mydrive_changestamp =
      entry.directory_specific_info().changestamp();

  base::FilePath file_path =
      util::GetDriveMyDriveRootPath().AppendASCII("File 1.txt");
  EXPECT_EQ(FILE_ERROR_NOT_FOUND,
            metadata_->GetResourceEntryByPath(file_path, &entry));

  // Load My Drive.
  change_list_loader_->LoadIfNeeded(
      DirectoryFetchInfo(drive_service_->GetRootResourceId(),
                         mydrive_changestamp),
      google_apis::test_util::CreateCopyResultCallback(&error));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(FILE_ERROR_OK, error);
  EXPECT_EQ(1U, observer.changed_directories().count(
      util::GetDriveMyDriveRootPath()));

  // Now the file is present.
  EXPECT_EQ(FILE_ERROR_OK,
            metadata_->GetResourceEntryByPath(file_path, &entry));
}

TEST_F(ChangeListLoaderTest, LoadIfNeeded_NewDirectories) {
  // Make local metadata up to date.
  FileError error = FILE_ERROR_FAILED;
  change_list_loader_->LoadIfNeeded(
      DirectoryFetchInfo(),
      google_apis::test_util::CreateCopyResultCallback(&error));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(FILE_ERROR_OK, error);

  // Add a new file.
  scoped_ptr<google_apis::ResourceEntry> file = AddNewFile("New File");
  ASSERT_TRUE(file);

  // Emulate the slowness of GetAllResourceList().
  drive_service_->set_never_return_all_resource_list(true);

  // Enter refreshing state.
  FileError check_for_updates_error = FILE_ERROR_FAILED;
  change_list_loader_->CheckForUpdates(
      google_apis::test_util::CreateCopyResultCallback(
          &check_for_updates_error));
  EXPECT_TRUE(change_list_loader_->IsRefreshing());

  // Load My Drive.
  TestChangeListLoaderObserver observer(change_list_loader_.get());
  change_list_loader_->LoadIfNeeded(
      DirectoryFetchInfo(drive_service_->GetRootResourceId(),
                         metadata_->GetLargestChangestamp()),
      google_apis::test_util::CreateCopyResultCallback(&error));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(FILE_ERROR_OK, error);
  EXPECT_EQ(1U, observer.changed_directories().count(
      util::GetDriveMyDriveRootPath()));

  // The new file is present in the local metadata.
  base::FilePath file_path =
      util::GetDriveMyDriveRootPath().AppendASCII(file->title());
  ResourceEntry entry;
  EXPECT_EQ(FILE_ERROR_OK,
            metadata_->GetResourceEntryByPath(file_path, &entry));
}

TEST_F(ChangeListLoaderTest, LoadIfNeeded_MultipleCalls) {
  TestChangeListLoaderObserver observer(change_list_loader_.get());

  // Load grand root.
  FileError error = FILE_ERROR_FAILED;
  change_list_loader_->LoadIfNeeded(
      DirectoryFetchInfo(util::kDriveGrandRootSpecialResourceId, 0),
      google_apis::test_util::CreateCopyResultCallback(&error));

  // Load grand root again without waiting for the result.
  FileError error2 = FILE_ERROR_FAILED;
  change_list_loader_->LoadIfNeeded(
      DirectoryFetchInfo(util::kDriveGrandRootSpecialResourceId, 0),
      google_apis::test_util::CreateCopyResultCallback(&error2));
  base::RunLoop().RunUntilIdle();

  // Callback is called for each method call.
  EXPECT_EQ(FILE_ERROR_OK, error);
  EXPECT_EQ(FILE_ERROR_OK, error2);

  // No duplicated resource list load and observer events.
  EXPECT_EQ(1, drive_service_->resource_list_load_count());
  EXPECT_EQ(1, observer.initial_load_complete_count());
  EXPECT_EQ(1, observer.load_from_server_complete_count());
}

TEST_F(ChangeListLoaderTest, CheckForUpdates) {
  // CheckForUpdates() results in no-op before load.
  FileError check_for_updates_error = FILE_ERROR_FAILED;
  change_list_loader_->CheckForUpdates(
      google_apis::test_util::CreateCopyResultCallback(
          &check_for_updates_error));
  EXPECT_FALSE(change_list_loader_->IsRefreshing());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(FILE_ERROR_FAILED,
            check_for_updates_error);  // Callback was not run.
  EXPECT_EQ(0, metadata_->GetLargestChangestamp());
  EXPECT_EQ(0, drive_service_->resource_list_load_count());

  // Start initial load.
  FileError load_error = FILE_ERROR_FAILED;
  change_list_loader_->LoadIfNeeded(
      DirectoryFetchInfo(),
      google_apis::test_util::CreateCopyResultCallback(&load_error));
  EXPECT_TRUE(change_list_loader_->IsRefreshing());

  // CheckForUpdates() while loading.
  change_list_loader_->CheckForUpdates(
      google_apis::test_util::CreateCopyResultCallback(
          &check_for_updates_error));

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(change_list_loader_->IsRefreshing());
  EXPECT_EQ(FILE_ERROR_OK, load_error);
  EXPECT_EQ(FILE_ERROR_OK, check_for_updates_error);
  EXPECT_LT(0, metadata_->GetLargestChangestamp());
  EXPECT_EQ(1, drive_service_->resource_list_load_count());

  int64 previous_changestamp = metadata_->GetLargestChangestamp();
  // CheckForUpdates() results in no update.
  change_list_loader_->CheckForUpdates(
      google_apis::test_util::CreateCopyResultCallback(
          &check_for_updates_error));
  EXPECT_TRUE(change_list_loader_->IsRefreshing());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(change_list_loader_->IsRefreshing());
  EXPECT_EQ(previous_changestamp, metadata_->GetLargestChangestamp());

  // Add a file to the service.
  scoped_ptr<google_apis::ResourceEntry> gdata_entry = AddNewFile("New File");
  ASSERT_TRUE(gdata_entry);

  // CheckForUpdates() results in update.
  TestChangeListLoaderObserver observer(change_list_loader_.get());
  change_list_loader_->CheckForUpdates(
      google_apis::test_util::CreateCopyResultCallback(
          &check_for_updates_error));
  EXPECT_TRUE(change_list_loader_->IsRefreshing());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(change_list_loader_->IsRefreshing());
  EXPECT_LT(previous_changestamp, metadata_->GetLargestChangestamp());
  EXPECT_EQ(1, observer.load_from_server_complete_count());
  EXPECT_EQ(1U, observer.changed_directories().count(
      util::GetDriveMyDriveRootPath()));

  // The new file is found in the local metadata.
  base::FilePath new_file_path =
      util::GetDriveMyDriveRootPath().AppendASCII(gdata_entry->title());
  ResourceEntry entry;
  EXPECT_EQ(FILE_ERROR_OK,
            metadata_->GetResourceEntryByPath(new_file_path, &entry));
}

}  // namespace internal
}  // namespace drive
