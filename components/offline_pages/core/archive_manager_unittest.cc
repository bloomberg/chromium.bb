// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/archive_manager.h"

#include <algorithm>
#include <memory>
#include <set>
#include <vector>

#include "base/bind.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/sys_info.h"
#include "base/test/histogram_tester.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

namespace {
const base::FilePath::CharType kMissingArchivePath[] =
    FILE_PATH_LITERAL("missing_archive.path");
}  // namespace

enum class CallbackStatus {
  NOT_CALLED,
  CALLED_FALSE,
  CALLED_TRUE,
};

class ArchiveManagerTest : public testing::Test {
 public:
  ArchiveManagerTest();
  void SetUp() override;

  void PumpLoop();
  void ResetResults();

  void ResetManager(const base::FilePath& temporary_dir,
                    const base::FilePath& private_archive_dir,
                    const base::FilePath& public_archive_dir);
  void Callback(bool result);
  void GetAllArchivesCallback(const std::set<base::FilePath>& archive_paths);
  void GetStorageStatsCallback(
      const ArchiveManager::StorageStats& storage_sizes);

  ArchiveManager* manager() { return manager_.get(); }
  const base::FilePath& temporary_path() const {
    return manager_->GetTemporaryArchivesDir();
  }
  const base::FilePath& private_archive_path() const {
    return manager_->GetPrivateArchivesDir();
  }
  const base::FilePath& public_archive_path() const {
    return manager_->GetPublicArchivesDir();
  }
  CallbackStatus callback_status() const { return callback_status_; }
  const std::set<base::FilePath>& last_archive_paths() const {
    return last_archive_paths_;
  }
  ArchiveManager::StorageStats last_storage_sizes() const {
    return last_storage_sizes_;
  }
  base::HistogramTester* histogram_tester() { return histogram_tester_.get(); }

 private:
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle task_runner_handle_;
  base::ScopedTempDir temporary_dir_;
  base::ScopedTempDir private_archive_dir_;
  base::ScopedTempDir public_archive_dir_;

  std::unique_ptr<ArchiveManager> manager_;
  CallbackStatus callback_status_;
  std::set<base::FilePath> last_archive_paths_;
  ArchiveManager::StorageStats last_storage_sizes_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

ArchiveManagerTest::ArchiveManagerTest()
    : task_runner_(new base::TestSimpleTaskRunner),
      task_runner_handle_(task_runner_),
      callback_status_(CallbackStatus::NOT_CALLED),
      last_storage_sizes_({0, 0, 0}) {}

void ArchiveManagerTest::SetUp() {
  ASSERT_TRUE(temporary_dir_.CreateUniqueTempDir());
  ASSERT_TRUE(private_archive_dir_.CreateUniqueTempDir());
  ASSERT_TRUE(public_archive_dir_.CreateUniqueTempDir());
  ResetManager(temporary_dir_.GetPath(), private_archive_dir_.GetPath(),
               public_archive_dir_.GetPath());
  histogram_tester_.reset(new base::HistogramTester());
}

void ArchiveManagerTest::PumpLoop() {
  task_runner_->RunUntilIdle();
}

void ArchiveManagerTest::ResetResults() {
  callback_status_ = CallbackStatus::NOT_CALLED;
  last_archive_paths_.clear();
}

void ArchiveManagerTest::ResetManager(
    const base::FilePath& temporary_dir,
    const base::FilePath& private_archive_dir,
    const base::FilePath& public_archive_dir) {
  manager_.reset(new ArchiveManager(temporary_dir, private_archive_dir,
                                    public_archive_dir,
                                    base::ThreadTaskRunnerHandle::Get()));
}

void ArchiveManagerTest::Callback(bool result) {
  callback_status_ =
      result ? CallbackStatus::CALLED_TRUE : CallbackStatus::CALLED_FALSE;
}

void ArchiveManagerTest::GetAllArchivesCallback(
    const std::set<base::FilePath>& archive_paths) {
  last_archive_paths_ = archive_paths;
}

void ArchiveManagerTest::GetStorageStatsCallback(
    const ArchiveManager::StorageStats& storage_sizes) {
  last_storage_sizes_ = storage_sizes;
}

TEST_F(ArchiveManagerTest, EnsureArchivesDirCreated) {
  base::FilePath temporary_archive_dir =
      temporary_path().Append(FILE_PATH_LITERAL("test_path"));
  base::FilePath private_archive_dir =
      private_archive_path().Append(FILE_PATH_LITERAL("test_path"));
  base::FilePath public_archive_dir(FILE_PATH_LITERAL("/sdcard/Download"));
  ResetManager(temporary_archive_dir, private_archive_dir, public_archive_dir);
  EXPECT_FALSE(base::PathExists(temporary_archive_dir));
  EXPECT_FALSE(base::PathExists(private_archive_dir));

  // Ensure archives dir exists, when it doesn't.
  manager()->EnsureArchivesDirCreated(
      base::Bind(&ArchiveManagerTest::Callback, base::Unretained(this), true));
  PumpLoop();
  EXPECT_EQ(CallbackStatus::CALLED_TRUE, callback_status());
  EXPECT_TRUE(base::PathExists(temporary_archive_dir));
  EXPECT_TRUE(base::PathExists(private_archive_dir));
  // The public dir does not get created by us, so we don't test its creation.
  histogram_tester()->ExpectUniqueSample(
      "OfflinePages.ArchiveManager.ArchiveDirsCreationResult2.Persistent",
      -base::File::Error::FILE_OK, 1);
  histogram_tester()->ExpectUniqueSample(
      "OfflinePages.ArchiveManager.ArchiveDirsCreationResult2.Temporary",
      -base::File::Error::FILE_OK, 1);

  // Try again when the file already exists.
  ResetResults();
  manager()->EnsureArchivesDirCreated(
      base::Bind(&ArchiveManagerTest::Callback, base::Unretained(this), true));
  PumpLoop();
  EXPECT_EQ(CallbackStatus::CALLED_TRUE, callback_status());
  EXPECT_TRUE(base::PathExists(temporary_archive_dir));
  EXPECT_TRUE(base::PathExists(private_archive_dir));
  histogram_tester()->ExpectTotalCount(
      "OfflinePages.ArchiveManager.ArchiveDirsCreationResult2.Persistent", 1);
  histogram_tester()->ExpectTotalCount(
      "OfflinePages.ArchiveManager.ArchiveDirsCreationResult2.Temporary", 1);
}

TEST_F(ArchiveManagerTest, ExistsArchive) {
  base::FilePath archive_path = temporary_path().Append(kMissingArchivePath);
  manager()->ExistsArchive(
      archive_path,
      base::Bind(&ArchiveManagerTest::Callback, base::Unretained(this)));
  PumpLoop();
  EXPECT_EQ(CallbackStatus::CALLED_FALSE, callback_status());

  ResetResults();
  EXPECT_TRUE(base::CreateTemporaryFileInDir(temporary_path(), &archive_path));

  manager()->ExistsArchive(
      archive_path,
      base::Bind(&ArchiveManagerTest::Callback, base::Unretained(this)));
  PumpLoop();
  EXPECT_EQ(CallbackStatus::CALLED_TRUE, callback_status());
}

TEST_F(ArchiveManagerTest, DeleteMultipleArchives) {
  base::FilePath archive_path_1;
  EXPECT_TRUE(
      base::CreateTemporaryFileInDir(temporary_path(), &archive_path_1));
  base::FilePath archive_path_2;
  EXPECT_TRUE(
      base::CreateTemporaryFileInDir(temporary_path(), &archive_path_2));
  base::FilePath archive_path_3;
  EXPECT_TRUE(
      base::CreateTemporaryFileInDir(temporary_path(), &archive_path_3));
  base::FilePath archive_path_4;
  EXPECT_TRUE(
      base::CreateTemporaryFileInDir(private_archive_path(), &archive_path_4));

  std::vector<base::FilePath> archive_paths = {archive_path_1, archive_path_2,
                                               archive_path_4};

  manager()->DeleteMultipleArchives(
      archive_paths,
      base::Bind(&ArchiveManagerTest::Callback, base::Unretained(this)));
  PumpLoop();
  EXPECT_EQ(CallbackStatus::CALLED_TRUE, callback_status());
  EXPECT_FALSE(base::PathExists(archive_path_1));
  EXPECT_FALSE(base::PathExists(archive_path_2));
  EXPECT_TRUE(base::PathExists(archive_path_3));
  EXPECT_FALSE(base::PathExists(archive_path_4));
}

TEST_F(ArchiveManagerTest, DeleteMultipleArchivesSomeDoNotExist) {
  base::FilePath archive_path_1 = temporary_path().Append(kMissingArchivePath);
  base::FilePath archive_path_2;
  EXPECT_TRUE(
      base::CreateTemporaryFileInDir(temporary_path(), &archive_path_2));
  base::FilePath archive_path_3;
  EXPECT_TRUE(
      base::CreateTemporaryFileInDir(temporary_path(), &archive_path_3));
  base::FilePath archive_path_4;
  EXPECT_TRUE(
      base::CreateTemporaryFileInDir(private_archive_path(), &archive_path_4));

  std::vector<base::FilePath> archive_paths = {archive_path_1, archive_path_2,
                                               archive_path_4};

  EXPECT_FALSE(base::PathExists(archive_path_1));

  manager()->DeleteMultipleArchives(
      archive_paths,
      base::Bind(&ArchiveManagerTest::Callback, base::Unretained(this)));
  PumpLoop();
  EXPECT_EQ(CallbackStatus::CALLED_TRUE, callback_status());
  EXPECT_FALSE(base::PathExists(archive_path_1));
  EXPECT_FALSE(base::PathExists(archive_path_2));
  EXPECT_TRUE(base::PathExists(archive_path_3));
  EXPECT_FALSE(base::PathExists(archive_path_4));
}

TEST_F(ArchiveManagerTest, DeleteMultipleArchivesNoneExist) {
  base::FilePath archive_path_1 = temporary_path().Append(kMissingArchivePath);
  base::FilePath archive_path_2 =
      temporary_path().Append(FILE_PATH_LITERAL("other_missing_file.mhtml"));
  base::FilePath archive_path_3;
  EXPECT_TRUE(
      base::CreateTemporaryFileInDir(temporary_path(), &archive_path_3));
  base::FilePath archive_path_4;
  EXPECT_TRUE(
      base::CreateTemporaryFileInDir(private_archive_path(), &archive_path_4));

  std::vector<base::FilePath> archive_paths = {archive_path_1, archive_path_2,
                                               archive_path_4};

  EXPECT_FALSE(base::PathExists(archive_path_1));
  EXPECT_FALSE(base::PathExists(archive_path_2));

  manager()->DeleteMultipleArchives(
      archive_paths,
      base::Bind(&ArchiveManagerTest::Callback, base::Unretained(this)));
  PumpLoop();
  EXPECT_EQ(CallbackStatus::CALLED_TRUE, callback_status());
  EXPECT_FALSE(base::PathExists(archive_path_1));
  EXPECT_FALSE(base::PathExists(archive_path_2));
  EXPECT_TRUE(base::PathExists(archive_path_3));
  EXPECT_FALSE(base::PathExists(archive_path_4));
}

TEST_F(ArchiveManagerTest, DeleteArchive) {
  base::FilePath archive_path;
  EXPECT_TRUE(base::CreateTemporaryFileInDir(temporary_path(), &archive_path));

  manager()->DeleteArchive(
      archive_path,
      base::Bind(&ArchiveManagerTest::Callback, base::Unretained(this)));
  PumpLoop();
  EXPECT_EQ(CallbackStatus::CALLED_TRUE, callback_status());
  EXPECT_FALSE(base::PathExists(archive_path));
}

TEST_F(ArchiveManagerTest, DeleteArchiveThatDoesNotExist) {
  base::FilePath archive_path = temporary_path().Append(kMissingArchivePath);
  EXPECT_FALSE(base::PathExists(archive_path));

  manager()->DeleteArchive(
      archive_path,
      base::Bind(&ArchiveManagerTest::Callback, base::Unretained(this)));
  PumpLoop();
  EXPECT_EQ(CallbackStatus::CALLED_TRUE, callback_status());
  EXPECT_FALSE(base::PathExists(archive_path));
}

TEST_F(ArchiveManagerTest, GetAllArchives) {
  base::FilePath archive_path_1;
  EXPECT_TRUE(
      base::CreateTemporaryFileInDir(temporary_path(), &archive_path_1));
  base::FilePath archive_path_2;
  EXPECT_TRUE(
      base::CreateTemporaryFileInDir(temporary_path(), &archive_path_2));
  base::FilePath archive_path_3;
  EXPECT_TRUE(
      base::CreateTemporaryFileInDir(temporary_path(), &archive_path_3));
  base::FilePath archive_path_4;
  EXPECT_TRUE(
      base::CreateTemporaryFileInDir(private_archive_path(), &archive_path_4));
  std::vector<base::FilePath> expected_paths{archive_path_1, archive_path_2,
                                             archive_path_3, archive_path_4};
  std::sort(expected_paths.begin(), expected_paths.end());

  manager()->GetAllArchives(base::Bind(
      &ArchiveManagerTest::GetAllArchivesCallback, base::Unretained(this)));
  PumpLoop();
  ASSERT_EQ(4UL, last_archive_paths().size());
  std::vector<base::FilePath> actual_paths(last_archive_paths().begin(),
                                           last_archive_paths().end());
  // Comparing one to one works because last_archive_paths set is sorted.
  // Because some windows bots provide abbreviated path (e.g. chrome-bot becomes
  // CHROME~2), this test only focuses on file name.
  EXPECT_EQ(expected_paths[0].BaseName(), actual_paths[0].BaseName());
  EXPECT_EQ(expected_paths[1].BaseName(), actual_paths[1].BaseName());
  EXPECT_EQ(expected_paths[2].BaseName(), actual_paths[2].BaseName());
  EXPECT_EQ(expected_paths[3].BaseName(), actual_paths[3].BaseName());
}

TEST_F(ArchiveManagerTest, GetStorageStats) {
  base::FilePath archive_path_1;
  EXPECT_TRUE(
      base::CreateTemporaryFileInDir(temporary_path(), &archive_path_1));
  base::FilePath archive_path_2;
  EXPECT_TRUE(
      base::CreateTemporaryFileInDir(private_archive_path(), &archive_path_2));

  manager()->GetStorageStats(base::Bind(
      &ArchiveManagerTest::GetStorageStatsCallback, base::Unretained(this)));
  PumpLoop();
  EXPECT_GT(last_storage_sizes().free_disk_space, 0);
  EXPECT_EQ(last_storage_sizes().private_archives_size,
            base::ComputeDirectorySize(private_archive_path()));
  EXPECT_EQ(last_storage_sizes().temporary_archives_size,
            base::ComputeDirectorySize(temporary_path()));
}

TEST_F(ArchiveManagerTest, TryWithInvalidTemporaryPath) {
  base::FilePath invalid_path;
  ResetManager(invalid_path, private_archive_path(), public_archive_path());

  manager()->GetStorageStats(base::Bind(
      &ArchiveManagerTest::GetStorageStatsCallback, base::Unretained(this)));
  PumpLoop();
  EXPECT_EQ(base::SysInfo::AmountOfFreeDiskSpace(temporary_path()),
            last_storage_sizes().free_disk_space);
  EXPECT_EQ(base::ComputeDirectorySize(private_archive_path()),
            last_storage_sizes().total_archives_size());
  EXPECT_EQ(0, last_storage_sizes().temporary_archives_size);
}

}  // namespace offline_pages
