// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/guid.h"
#include "base/memory/ptr_util.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/download/internal/driver_entry.h"
#include "components/download/internal/entry.h"
#include "components/download/internal/file_monitor_impl.h"
#include "components/download/internal/test/entry_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace download {

class FileMonitorTest : public testing::Test {
 public:
  FileMonitorTest()
      : task_runner_(new base::TestSimpleTaskRunner), handle_(task_runner_) {
    EXPECT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
    download_dir_ = scoped_temp_dir_.GetPath();
    base::TimeDelta keep_alive_time = base::TimeDelta::FromHours(12);
    monitor_ = base::MakeUnique<FileMonitorImpl>(download_dir_, task_runner_,
                                                 keep_alive_time);
  }
  ~FileMonitorTest() override = default;

 protected:
  base::FilePath CreateTemporaryFile(std::string file_name);

  base::ScopedTempDir scoped_temp_dir_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle handle_;
  base::FilePath download_dir_;
  std::unique_ptr<FileMonitor> monitor_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FileMonitorTest);
};

base::FilePath FileMonitorTest::CreateTemporaryFile(std::string file_name) {
  base::FilePath file_path = download_dir_.AppendASCII(file_name);
  base::File file(file_path, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  EXPECT_TRUE(file.IsValid());
  file.Close();

  return file_path;
}

TEST_F(FileMonitorTest, TestDeleteUnknownFiles) {
  Entry entry1 = test::BuildEntry(DownloadClient::TEST, base::GenerateGUID());
  entry1.target_file_path = CreateTemporaryFile(entry1.guid);

  Entry entry2 = test::BuildEntry(DownloadClient::TEST, base::GenerateGUID());
  entry2.target_file_path = CreateTemporaryFile(entry2.guid);

  DriverEntry driver_entry1;
  driver_entry1.guid = entry1.guid;
  driver_entry1.current_file_path = entry1.target_file_path;

  DriverEntry driver_entry2;
  driver_entry2.guid = base::GenerateGUID();
  driver_entry2.current_file_path = CreateTemporaryFile(driver_entry2.guid);

  base::FilePath temp_file1 = CreateTemporaryFile("temp1");
  base::FilePath temp_file2 = CreateTemporaryFile("temp2");

  auto check_file_existence = [&](bool e1, bool e2, bool de1, bool de2, bool t1,
                                  bool t2) {
    EXPECT_EQ(e1, base::PathExists(entry1.target_file_path));
    EXPECT_EQ(e2, base::PathExists(entry2.target_file_path));
    EXPECT_EQ(de1, base::PathExists(driver_entry1.current_file_path));
    EXPECT_EQ(de2, base::PathExists(driver_entry2.current_file_path));
    EXPECT_EQ(t1, base::PathExists(temp_file1));
    EXPECT_EQ(t2, base::PathExists(temp_file2));
  };

  check_file_existence(true, true, true, true, true, true);

  std::vector<Entry*> entries = {&entry1, &entry2};
  std::vector<DriverEntry> driver_entries = {driver_entry1, driver_entry2};

  monitor_->DeleteUnknownFiles(entries, driver_entries);
  task_runner_->RunUntilIdle();
  check_file_existence(true, true, true, true, false, false);

  entries = {&entry2};
  driver_entries = {driver_entry1, driver_entry2};
  monitor_->DeleteUnknownFiles(entries, driver_entries);
  task_runner_->RunUntilIdle();
  check_file_existence(true, true, true, true, false, false);

  entries = {&entry2};
  driver_entries = {driver_entry2};
  monitor_->DeleteUnknownFiles(entries, driver_entries);
  task_runner_->RunUntilIdle();
  check_file_existence(false, true, false, true, false, false);

  entries.clear();
  driver_entries.clear();
  monitor_->DeleteUnknownFiles(entries, driver_entries);
  task_runner_->RunUntilIdle();
  check_file_existence(false, false, false, false, false, false);
}

TEST_F(FileMonitorTest, TestCleanupFilesForCompletedEntries) {
  Entry entry1 = test::BuildEntry(DownloadClient::TEST, base::GenerateGUID());
  entry1.state = Entry::State::COMPLETE;
  entry1.completion_time = base::Time::Now() - base::TimeDelta::FromHours(20);
  EXPECT_TRUE(
      base::CreateTemporaryFileInDir(download_dir_, &entry1.target_file_path));

  Entry entry2 = test::BuildEntry(DownloadClient::TEST, base::GenerateGUID());
  entry2.state = Entry::State::ACTIVE;
  EXPECT_TRUE(
      base::CreateTemporaryFileInDir(download_dir_, &entry2.target_file_path));

  std::vector<Entry*> entries = {&entry1, &entry2};
  std::vector<Entry*> entries_to_remove =
      monitor_->CleanupFilesForCompletedEntries(entries);
  task_runner_->RunUntilIdle();

  EXPECT_EQ(1u, entries_to_remove.size());
  EXPECT_FALSE(base::PathExists(entry1.target_file_path));
  EXPECT_TRUE(base::PathExists(entry2.target_file_path));
}

}  // namespace download
