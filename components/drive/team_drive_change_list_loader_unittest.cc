// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/drive/chromeos/team_drive_change_list_loader.h"
#include "base/command_line.h"
#include "base/files/scoped_temp_dir.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/drive/chromeos/drive_test_util.h"
#include "components/drive/chromeos/file_cache.h"
#include "components/drive/chromeos/loader_controller.h"
#include "components/drive/chromeos/resource_metadata.h"
#include "components/drive/event_logger.h"
#include "components/drive/file_system_core_util.h"
#include "components/drive/job_scheduler.h"
#include "components/drive/resource_metadata_storage.h"
#include "components/drive/service/fake_drive_service.h"
#include "components/drive/service/test_util.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drive {
namespace internal {

namespace {

constexpr char kTeamDriveId[] = "team_drive_id";
constexpr char kTeamDriveName[] = "team_drive_name";
constexpr char kTestStartPageToken[] = "1";

}  // namespace

class TeamDriveChangeListLoaderTest : public testing::Test {
 protected:
  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        google_apis::kEnableTeamDrives);
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    pref_service_.reset(new TestingPrefServiceSimple);
    test_util::RegisterDrivePrefs(pref_service_->registry());

    logger_.reset(new EventLogger);

    drive_service_.reset(new FakeDriveService);
    ASSERT_TRUE(test_util::SetUpTestEntries(drive_service_.get()));

    scheduler_.reset(new JobScheduler(
        pref_service_.get(), logger_.get(), drive_service_.get(),
        base::ThreadTaskRunnerHandle::Get().get(), nullptr));
    metadata_storage_.reset(new ResourceMetadataStorage(
        temp_dir_.GetPath(), base::ThreadTaskRunnerHandle::Get().get()));
    ASSERT_TRUE(metadata_storage_->Initialize());

    cache_.reset(new FileCache(metadata_storage_.get(), temp_dir_.GetPath(),
                               base::ThreadTaskRunnerHandle::Get().get(),
                               NULL /* free_disk_space_getter */));
    ASSERT_TRUE(cache_->Initialize());

    metadata_.reset(
        new ResourceMetadata(metadata_storage_.get(), cache_.get(),
                             base::ThreadTaskRunnerHandle::Get().get()));
    ASSERT_EQ(FILE_ERROR_OK, metadata_->Initialize());

    loader_controller_.reset(new LoaderController);
    base::FilePath root_entry_path =
        util::GetDriveTeamDrivesRootPath().Append(kTeamDriveName);

    team_drive_change_list_loader_ =
        std::make_unique<TeamDriveChangeListLoader>(
            kTeamDriveId, root_entry_path, logger_.get(),
            base::ThreadTaskRunnerHandle::Get().get(), metadata_.get(),
            scheduler_.get(), loader_controller_.get());
  }

  void AddTeamDriveEntry(const std::string& id,
                         const std::string& name,
                         const std::string& start_page_token) {
    // Setup the team drive in the fake drive service and metadata.
    drive_service_->AddTeamDrive(id, name, start_page_token);

    std::string root_local_id;
    ASSERT_EQ(FILE_ERROR_OK,
              metadata_->GetIdByPath(util::GetDriveTeamDrivesRootPath(),
                                     &root_local_id));

    std::string local_id;
    ASSERT_EQ(FILE_ERROR_OK, metadata_->AddEntry(
                                 CreateDirectoryEntryWithResourceId(
                                     name, id, root_local_id, start_page_token),
                                 &local_id));
  }

  // Creates a ResourceEntry for a directory with explicitly set resource_id.
  ResourceEntry CreateDirectoryEntryWithResourceId(
      const std::string& title,
      const std::string& resource_id,
      const std::string& parent_local_id,
      const std::string& start_page_token) {
    ResourceEntry entry;
    entry.set_title(title);
    entry.set_resource_id(resource_id);
    entry.set_parent_local_id(parent_local_id);
    entry.mutable_file_info()->set_is_directory(true);
    entry.mutable_directory_specific_info()->set_start_page_token(
        start_page_token);
    return entry;
  }

  content::TestBrowserThreadBundle thread_bundle_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  std::unique_ptr<EventLogger> logger_;
  std::unique_ptr<FakeDriveService> drive_service_;
  std::unique_ptr<JobScheduler> scheduler_;
  std::unique_ptr<ResourceMetadataStorage, test_util::DestroyHelperForTests>
      metadata_storage_;
  std::unique_ptr<FileCache, test_util::DestroyHelperForTests> cache_;
  std::unique_ptr<ResourceMetadata, test_util::DestroyHelperForTests> metadata_;
  std::unique_ptr<LoaderController> loader_controller_;
  std::unique_ptr<TeamDriveChangeListLoader> team_drive_change_list_loader_;
};

// When loading, if the local resource metadata contains a start page token then
// we do not load from the server, just use local metadata.
TEST_F(TeamDriveChangeListLoaderTest, MetadataRefresh) {
  AddTeamDriveEntry(kTeamDriveId, kTeamDriveName, kTestStartPageToken);
  EXPECT_FALSE(team_drive_change_list_loader_->IsRefreshing());

  FileError error = FILE_ERROR_FAILED;
  team_drive_change_list_loader_->LoadIfNeeded(
      google_apis::test_util::CreateCopyResultCallback(&error));
  EXPECT_TRUE(team_drive_change_list_loader_->IsRefreshing());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(FILE_ERROR_OK, error);

  EXPECT_FALSE(team_drive_change_list_loader_->IsRefreshing());
  EXPECT_EQ(1, drive_service_->start_page_token_load_count());
  EXPECT_EQ(0, drive_service_->change_list_load_count());
}

// TODO(slangley): Add tests for processing team drive change lists.

}  // namespace internal
}  // namespace drive
