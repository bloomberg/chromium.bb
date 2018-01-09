// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/archive_manager.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequenced_task_runner.h"
#include "base/sys_info.h"
#include "base/threading/thread_task_runner_handle.h"

namespace offline_pages {

namespace {

using StorageStatsCallback =
    base::Callback<void(const ArchiveManager::StorageStats& storage_stats)>;

void EnsureArchivesDirCreatedImpl(const base::FilePath& archives_dir,
                                  bool is_temp) {
  base::File::Error error = base::File::FILE_OK;
  if (!base::DirectoryExists(archives_dir)) {
    if (!base::CreateDirectoryAndGetError(archives_dir, &error)) {
      LOG(ERROR) << "Failed to create offline pages archive directory: "
                 << base::File::ErrorToString(error);
    }
    if (is_temp) {
      UMA_HISTOGRAM_ENUMERATION(
          "OfflinePages.ArchiveManager.ArchiveDirsCreationResult2.Temporary",
          -error, -base::File::FILE_ERROR_MAX);
    } else {
      UMA_HISTOGRAM_ENUMERATION(
          "OfflinePages.ArchiveManager.ArchiveDirsCreationResult2.Persistent",
          -error, -base::File::FILE_ERROR_MAX);
    }
  }
}

void ExistsArchiveImpl(const base::FilePath& file_path,
                       scoped_refptr<base::SequencedTaskRunner> task_runner,
                       const base::Callback<void(bool)>& callback) {
  task_runner->PostTask(FROM_HERE,
                        base::Bind(callback, base::PathExists(file_path)));
}

void DeleteArchivesImpl(const std::vector<base::FilePath>& file_paths,
                        scoped_refptr<base::SequencedTaskRunner> task_runner,
                        const base::Callback<void(bool)>& callback) {
  bool result = false;
  for (const auto& file_path : file_paths) {
    // Make sure delete happens on the left of || so that it is always executed.
    result = base::DeleteFile(file_path, false) || result;
  }
  task_runner->PostTask(FROM_HERE, base::Bind(callback, result));
}

void GetAllArchivesImpl(
    const std::vector<base::FilePath>& archives_dirs,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    const base::Callback<void(const std::set<base::FilePath>&)>& callback) {
  std::set<base::FilePath> archive_paths;
  for (const auto& archives_dir : archives_dirs) {
    base::FileEnumerator file_enumerator(archives_dir, false,
                                         base::FileEnumerator::FILES);
    for (base::FilePath archive_path = file_enumerator.Next();
         !archive_path.empty(); archive_path = file_enumerator.Next()) {
      archive_paths.insert(archive_path);
    }
  }
  task_runner->PostTask(FROM_HERE, base::Bind(callback, archive_paths));
}

void GetStorageStatsImpl(const base::FilePath& temporary_archives_dir,
                         const base::FilePath& persistent_archives_dir,
                         scoped_refptr<base::SequencedTaskRunner> task_runner,
                         const StorageStatsCallback& callback) {
  ArchiveManager::StorageStats storage_stats = {0, 0, 0};

  // Getting the free disk space of the volume that contains the temporary
  // archives directory. This value will be -1 if the directory is invalid.
  // Currently both temporary and persistent archives directories are in the
  // internal storage.
  // In the future temporary and persistent archives directories may be on
  // different volumes, then another field may be added to StorageStats.
  storage_stats.free_disk_space =
      base::SysInfo::AmountOfFreeDiskSpace(temporary_archives_dir);
  if (!persistent_archives_dir.empty()) {
    storage_stats.private_archives_size =
        base::ComputeDirectorySize(persistent_archives_dir);
  }
  if (!temporary_archives_dir.empty()) {
    storage_stats.temporary_archives_size =
        base::ComputeDirectorySize(temporary_archives_dir);
  }
  task_runner->PostTask(FROM_HERE, base::Bind(callback, storage_stats));
}

}  // namespace

// protected and used for testing.
ArchiveManager::ArchiveManager() {}

ArchiveManager::ArchiveManager(
    const base::FilePath& temporary_archives_dir,
    const base::FilePath& private_archives_dir,
    const base::FilePath& public_archives_dir,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : temporary_archives_dir_(temporary_archives_dir),
      private_archives_dir_(private_archives_dir),
      public_archives_dir_(public_archives_dir),
      task_runner_(task_runner) {}

ArchiveManager::~ArchiveManager() {}

void ArchiveManager::EnsureArchivesDirCreated(const base::Closure& callback) {
  // The callback will only be invoked once both directories are created.
  if (!temporary_archives_dir_.empty()) {
    task_runner_->PostTask(
        FROM_HERE, base::Bind(EnsureArchivesDirCreatedImpl,
                              temporary_archives_dir_, true /* is_temp */));
  }
  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(EnsureArchivesDirCreatedImpl, private_archives_dir_,
                 false /* is_temp */),
      callback);
}

void ArchiveManager::ExistsArchive(const base::FilePath& archive_path,
                                   const base::Callback<void(bool)>& callback) {
  task_runner_->PostTask(
      FROM_HERE, base::Bind(ExistsArchiveImpl, archive_path,
                            base::ThreadTaskRunnerHandle::Get(), callback));
}

void ArchiveManager::DeleteArchive(const base::FilePath& archive_path,
                                   const base::Callback<void(bool)>& callback) {
  std::vector<base::FilePath> archive_paths = {archive_path};
  DeleteMultipleArchives(archive_paths, callback);
}

void ArchiveManager::DeleteMultipleArchives(
    const std::vector<base::FilePath>& archive_paths,
    const base::Callback<void(bool)>& callback) {
  task_runner_->PostTask(
      FROM_HERE, base::Bind(DeleteArchivesImpl, archive_paths,
                            base::ThreadTaskRunnerHandle::Get(), callback));
}

void ArchiveManager::GetAllArchives(
    const base::Callback<void(const std::set<base::FilePath>&)>& callback)
    const {
  std::vector<base::FilePath> archives_dirs = {private_archives_dir_};
  if (!temporary_archives_dir_.empty())
    archives_dirs.push_back(temporary_archives_dir_);
  task_runner_->PostTask(
      FROM_HERE, base::Bind(GetAllArchivesImpl, archives_dirs,
                            base::ThreadTaskRunnerHandle::Get(), callback));
}

void ArchiveManager::GetStorageStats(
    const StorageStatsCallback& callback) const {
  task_runner_->PostTask(
      FROM_HERE, base::Bind(GetStorageStatsImpl, temporary_archives_dir_,
                            private_archives_dir_,
                            base::ThreadTaskRunnerHandle::Get(), callback));
}

const base::FilePath& ArchiveManager::GetTemporaryArchivesDir() const {
  return temporary_archives_dir_;
}

const base::FilePath& ArchiveManager::GetPrivateArchivesDir() const {
  return private_archives_dir_;
}

const base::FilePath& ArchiveManager::GetPublicArchivesDir() const {
  return public_archives_dir_;
}

}  // namespace offline_pages
