// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/change_list_loader.h"

#include "base/callback_helpers.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/testing_pref_service.h"
#include "base/run_loop.h"
#include "chrome/browser/chromeos/drive/change_list_loader_observer.h"
#include "chrome/browser/chromeos/drive/file_cache.h"
#include "chrome/browser/chromeos/drive/file_change.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/drive/job_scheduler.h"
#include "chrome/browser/chromeos/drive/resource_metadata.h"
#include "chrome/browser/chromeos/drive/test_util.h"
#include "chrome/browser/drive/event_logger.h"
#include "chrome/browser/drive/fake_drive_service.h"
#include "chrome/browser/drive/test_util.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "google_apis/drive/drive_api_parser.h"
#include "google_apis/drive/test_util.h"
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

  const FileChange& changed_files() const { return changed_files_; }
  void clear_changed_files() { changed_files_.ClearForTest(); }

  int load_from_server_complete_count() const {
    return load_from_server_complete_count_;
  }
  int initial_load_complete_count() const {
    return initial_load_complete_count_;
  }

  // ChageListObserver overrides:
  virtual void OnFileChanged(const FileChange& changed_files) OVERRIDE {
    changed_files_.Apply(changed_files);
  }
  virtual void OnLoadFromServerComplete() OVERRIDE {
    ++load_from_server_complete_count_;
  }
  virtual void OnInitialLoadComplete() OVERRIDE {
    ++initial_load_complete_count_;
  }

 private:
  ChangeListLoader* loader_;
  FileChange changed_files_;
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

    logger_.reset(new EventLogger);

    drive_service_.reset(new FakeDriveService);
    ASSERT_TRUE(test_util::SetUpTestEntries(drive_service_.get()));

    scheduler_.reset(new JobScheduler(pref_service_.get(),
                                      logger_.get(),
                                      drive_service_.get(),
                                      base::MessageLoopProxy::current().get()));
    metadata_storage_.reset(new ResourceMetadataStorage(
        temp_dir_.path(), base::MessageLoopProxy::current().get()));
    ASSERT_TRUE(metadata_storage_->Initialize());

    cache_.reset(new FileCache(metadata_storage_.get(),
                               temp_dir_.path(),
                               base::MessageLoopProxy::current().get(),
                               NULL /* free_disk_space_getter */));
    ASSERT_TRUE(cache_->Initialize());

    metadata_.reset(new ResourceMetadata(
        metadata_storage_.get(), cache_.get(),
        base::MessageLoopProxy::current().get()));
    ASSERT_EQ(FILE_ERROR_OK, metadata_->Initialize());

    about_resource_loader_.reset(new AboutResourceLoader(scheduler_.get()));
    loader_controller_.reset(new LoaderController);
    change_list_loader_.reset(
        new ChangeListLoader(logger_.get(),
                             base::MessageLoopProxy::current().get(),
                             metadata_.get(),
                             scheduler_.get(),
                             about_resource_loader_.get(),
                             loader_controller_.get()));
  }

  // Adds a new file to the root directory of the service.
  scoped_ptr<google_apis::FileResource> AddNewFile(const std::string& title) {
    google_apis::GDataErrorCode error = google_apis::GDATA_FILE_ERROR;
    scoped_ptr<google_apis::FileResource> entry;
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
  scoped_ptr<EventLogger> logger_;
  scoped_ptr<FakeDriveService> drive_service_;
  scoped_ptr<JobScheduler> scheduler_;
  scoped_ptr<ResourceMetadataStorage,
             test_util::DestroyHelperForTests> metadata_storage_;
  scoped_ptr<FileCache, test_util::DestroyHelperForTests> cache_;
  scoped_ptr<ResourceMetadata, test_util::DestroyHelperForTests> metadata_;
  scoped_ptr<AboutResourceLoader> about_resource_loader_;
  scoped_ptr<LoaderController> loader_controller_;
  scoped_ptr<ChangeListLoader> change_list_loader_;
};

TEST_F(ChangeListLoaderTest, AboutResourceLoader) {
  google_apis::GDataErrorCode error[6] = {};
  scoped_ptr<google_apis::AboutResource> about[6];

  // No resource is cached at the beginning.
  ASSERT_FALSE(about_resource_loader_->cached_about_resource());

  // Since no resource is cached, this "Get" should trigger the update.
  about_resource_loader_->GetAboutResource(
      google_apis::test_util::CreateCopyResultCallback(error + 0, about + 0));
  // Since there is one in-flight update, the next "Get" just wait for it.
  about_resource_loader_->GetAboutResource(
      google_apis::test_util::CreateCopyResultCallback(error + 1, about + 1));

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(google_apis::HTTP_SUCCESS, error[0]);
  EXPECT_EQ(google_apis::HTTP_SUCCESS, error[1]);
  const int64 first_changestamp = about[0]->largest_change_id();
  EXPECT_EQ(first_changestamp, about[1]->largest_change_id());
  ASSERT_TRUE(about_resource_loader_->cached_about_resource());
  EXPECT_EQ(
      first_changestamp,
      about_resource_loader_->cached_about_resource()->largest_change_id());

  // Increment changestamp by 1.
  AddNewFile("temp");
  // Explicitly calling UpdateAboutResource will start another API call.
  about_resource_loader_->UpdateAboutResource(
      google_apis::test_util::CreateCopyResultCallback(error + 2, about + 2));
  // It again waits for the in-flight UpdateAboutResoure call, even though this
  // time there is a cached result.
  about_resource_loader_->GetAboutResource(
      google_apis::test_util::CreateCopyResultCallback(error + 3, about + 3));

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(google_apis::HTTP_SUCCESS, error[2]);
  EXPECT_EQ(google_apis::HTTP_SUCCESS, error[3]);
  EXPECT_EQ(first_changestamp + 1, about[2]->largest_change_id());
  EXPECT_EQ(first_changestamp + 1, about[3]->largest_change_id());
  EXPECT_EQ(
      first_changestamp + 1,
      about_resource_loader_->cached_about_resource()->largest_change_id());

  // Increment changestamp by 1.
  AddNewFile("temp2");
  // Now no UpdateAboutResource task is running. Returns the cached result.
  about_resource_loader_->GetAboutResource(
      google_apis::test_util::CreateCopyResultCallback(error + 4, about + 4));
  // Explicitly calling UpdateAboutResource will start another API call.
  about_resource_loader_->UpdateAboutResource(
      google_apis::test_util::CreateCopyResultCallback(error + 5, about + 5));

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(google_apis::HTTP_NO_CONTENT, error[4]);
  EXPECT_EQ(google_apis::HTTP_SUCCESS, error[5]);
  EXPECT_EQ(first_changestamp + 1, about[4]->largest_change_id());
  EXPECT_EQ(first_changestamp + 2, about[5]->largest_change_id());
  EXPECT_EQ(
      first_changestamp + 2,
      about_resource_loader_->cached_about_resource()->largest_change_id());

  EXPECT_EQ(3, drive_service_->about_resource_load_count());
}

TEST_F(ChangeListLoaderTest, Load) {
  EXPECT_FALSE(change_list_loader_->IsRefreshing());

  // Start initial load.
  TestChangeListLoaderObserver observer(change_list_loader_.get());

  EXPECT_EQ(0, drive_service_->about_resource_load_count());

  FileError error = FILE_ERROR_FAILED;
  change_list_loader_->LoadIfNeeded(
      google_apis::test_util::CreateCopyResultCallback(&error));
  EXPECT_TRUE(change_list_loader_->IsRefreshing());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(FILE_ERROR_OK, error);

  EXPECT_FALSE(change_list_loader_->IsRefreshing());
  int64 changestamp = 0;
  EXPECT_EQ(FILE_ERROR_OK, metadata_->GetLargestChangestamp(&changestamp));
  EXPECT_LT(0, changestamp);
  EXPECT_EQ(1, drive_service_->file_list_load_count());
  EXPECT_EQ(1, drive_service_->about_resource_load_count());
  EXPECT_EQ(1, observer.initial_load_complete_count());
  EXPECT_EQ(1, observer.load_from_server_complete_count());
  EXPECT_TRUE(observer.changed_files().empty());

  base::FilePath file_path =
      util::GetDriveMyDriveRootPath().AppendASCII("File 1.txt");
  ResourceEntry entry;
  EXPECT_EQ(FILE_ERROR_OK,
            metadata_->GetResourceEntryByPath(file_path, &entry));
}

TEST_F(ChangeListLoaderTest, Load_LocalMetadataAvailable) {
  // Prepare metadata.
  FileError error = FILE_ERROR_FAILED;
  change_list_loader_->LoadIfNeeded(
      google_apis::test_util::CreateCopyResultCallback(&error));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(FILE_ERROR_OK, error);

  // Reset loader.
  change_list_loader_.reset(
      new ChangeListLoader(logger_.get(),
                           base::MessageLoopProxy::current().get(),
                           metadata_.get(),
                           scheduler_.get(),
                           about_resource_loader_.get(),
                           loader_controller_.get()));

  // Add a file to the service.
  scoped_ptr<google_apis::FileResource> gdata_entry = AddNewFile("New File");
  ASSERT_TRUE(gdata_entry);

  // Start loading. Because local metadata is available, the load results in
  // returning FILE_ERROR_OK without fetching full list of resources.
  const int previous_file_list_load_count =
      drive_service_->file_list_load_count();
  TestChangeListLoaderObserver observer(change_list_loader_.get());

  change_list_loader_->LoadIfNeeded(
      google_apis::test_util::CreateCopyResultCallback(&error));
  EXPECT_TRUE(change_list_loader_->IsRefreshing());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(FILE_ERROR_OK, error);
  EXPECT_EQ(previous_file_list_load_count,
            drive_service_->file_list_load_count());
  EXPECT_EQ(1, observer.initial_load_complete_count());

  // Update should be checked by Load().
  int64 changestamp = 0;
  EXPECT_EQ(FILE_ERROR_OK, metadata_->GetLargestChangestamp(&changestamp));
  EXPECT_EQ(drive_service_->about_resource().largest_change_id(), changestamp);
  EXPECT_EQ(1, drive_service_->change_list_load_count());
  EXPECT_EQ(1, observer.load_from_server_complete_count());
  EXPECT_TRUE(
      observer.changed_files().CountDirectory(util::GetDriveMyDriveRootPath()));

  base::FilePath file_path =
      util::GetDriveMyDriveRootPath().AppendASCII(gdata_entry->title());
  ResourceEntry entry;
  EXPECT_EQ(FILE_ERROR_OK,
            metadata_->GetResourceEntryByPath(file_path, &entry));
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
  int64 changestamp = 0;
  EXPECT_EQ(FILE_ERROR_OK, metadata_->GetLargestChangestamp(&changestamp));
  EXPECT_EQ(0, changestamp);
  EXPECT_EQ(0, drive_service_->file_list_load_count());
  EXPECT_EQ(0, drive_service_->about_resource_load_count());

  // Start initial load.
  FileError load_error = FILE_ERROR_FAILED;
  change_list_loader_->LoadIfNeeded(
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
  EXPECT_EQ(FILE_ERROR_OK, metadata_->GetLargestChangestamp(&changestamp));
  EXPECT_LT(0, changestamp);
  EXPECT_EQ(1, drive_service_->file_list_load_count());

  int64 previous_changestamp = 0;
  EXPECT_EQ(FILE_ERROR_OK,
            metadata_->GetLargestChangestamp(&previous_changestamp));
  // CheckForUpdates() results in no update.
  change_list_loader_->CheckForUpdates(
      google_apis::test_util::CreateCopyResultCallback(
          &check_for_updates_error));
  EXPECT_TRUE(change_list_loader_->IsRefreshing());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(change_list_loader_->IsRefreshing());
  EXPECT_EQ(FILE_ERROR_OK, metadata_->GetLargestChangestamp(&changestamp));
  EXPECT_EQ(previous_changestamp, changestamp);

  // Add a file to the service.
  scoped_ptr<google_apis::FileResource> gdata_entry = AddNewFile("New File");
  ASSERT_TRUE(gdata_entry);

  // CheckForUpdates() results in update.
  TestChangeListLoaderObserver observer(change_list_loader_.get());
  change_list_loader_->CheckForUpdates(
      google_apis::test_util::CreateCopyResultCallback(
          &check_for_updates_error));
  EXPECT_TRUE(change_list_loader_->IsRefreshing());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(change_list_loader_->IsRefreshing());
  EXPECT_EQ(FILE_ERROR_OK, metadata_->GetLargestChangestamp(&changestamp));
  EXPECT_LT(previous_changestamp, changestamp);
  EXPECT_EQ(1, observer.load_from_server_complete_count());
  EXPECT_TRUE(
      observer.changed_files().CountDirectory(util::GetDriveMyDriveRootPath()));

  // The new file is found in the local metadata.
  base::FilePath new_file_path =
      util::GetDriveMyDriveRootPath().AppendASCII(gdata_entry->title());
  ResourceEntry entry;
  EXPECT_EQ(FILE_ERROR_OK,
            metadata_->GetResourceEntryByPath(new_file_path, &entry));
}

TEST_F(ChangeListLoaderTest, Lock) {
  FileError error = FILE_ERROR_FAILED;
  change_list_loader_->LoadIfNeeded(
      google_apis::test_util::CreateCopyResultCallback(&error));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(FILE_ERROR_OK, error);

  // Add a new file.
  scoped_ptr<google_apis::FileResource> file = AddNewFile("New File");
  ASSERT_TRUE(file);

  // Lock the loader.
  scoped_ptr<base::ScopedClosureRunner> lock = loader_controller_->GetLock();

  // Start update.
  TestChangeListLoaderObserver observer(change_list_loader_.get());
  FileError check_for_updates_error = FILE_ERROR_FAILED;
  change_list_loader_->CheckForUpdates(
      google_apis::test_util::CreateCopyResultCallback(
          &check_for_updates_error));
  base::RunLoop().RunUntilIdle();

  // Update is pending due to the lock.
  EXPECT_TRUE(observer.changed_files().empty());

  // Unlock the loader, this should resume the pending udpate.
  lock.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(
      observer.changed_files().CountDirectory(util::GetDriveMyDriveRootPath()));
}

}  // namespace internal
}  // namespace drive
