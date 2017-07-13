// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/file_monitor_impl.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_restrictions.h"

namespace download {

namespace {

bool CreateDirectoryIfNotExists(const base::FilePath& dir_path) {
  bool success = base::PathExists(dir_path);
  if (!success) {
    base::File::Error error;
    success = base::CreateDirectoryAndGetError(dir_path, &error);
  }
  return success;
}

void GetFilesInDirectory(const base::FilePath& directory,
                         std::set<base::FilePath>& paths_out) {
  base::ThreadRestrictions::AssertIOAllowed();
  base::FileEnumerator file_enumerator(directory, false /* recursive */,
                                       base::FileEnumerator::FILES);

  for (base::FilePath path = file_enumerator.Next(); !path.value().empty();
       path = file_enumerator.Next()) {
    paths_out.insert(path);
  }
}

void DeleteFilesOnFileThread(const std::set<base::FilePath>& paths,
                             stats::FileCleanupReason reason) {
  base::ThreadRestrictions::AssertIOAllowed();
  int num_delete_attempted = 0;
  int num_delete_failed = 0;
  int num_delete_by_external = 0;
  for (const base::FilePath& path : paths) {
    if (!base::PathExists(path)) {
      num_delete_by_external++;
      continue;
    }

    num_delete_attempted++;
    DCHECK(!base::DirectoryExists(path));

    if (!base::DeleteFile(path, false /* recursive */)) {
      num_delete_failed++;
    }
  }

  stats::LogFileCleanupStatus(reason, num_delete_attempted, num_delete_failed,
                              num_delete_by_external);
}

void DeleteUnknownFilesOnFileThread(
    const base::FilePath& directory,
    const std::set<base::FilePath>& download_file_paths) {
  base::ThreadRestrictions::AssertIOAllowed();
  std::set<base::FilePath> files_in_dir;
  GetFilesInDirectory(directory, files_in_dir);

  std::set<base::FilePath> files_to_remove =
      base::STLSetDifference<std::set<base::FilePath>>(files_in_dir,
                                                       download_file_paths);
  DeleteFilesOnFileThread(files_to_remove, stats::FileCleanupReason::UNKNOWN);
}

bool HardRecoverOnFileThread(const base::FilePath& directory) {
  base::ThreadRestrictions::AssertIOAllowed();
  std::set<base::FilePath> files_in_dir;
  GetFilesInDirectory(directory, files_in_dir);
  DeleteFilesOnFileThread(files_in_dir,
                          stats::FileCleanupReason::HARD_RECOVERY);
  return CreateDirectoryIfNotExists(directory);
}

}  // namespace

FileMonitorImpl::FileMonitorImpl(
    const base::FilePath& download_file_dir,
    const scoped_refptr<base::SequencedTaskRunner>& file_thread_task_runner,
    base::TimeDelta file_keep_alive_time)
    : download_file_dir_(download_file_dir),
      file_keep_alive_time_(file_keep_alive_time),
      file_thread_task_runner_(file_thread_task_runner),
      weak_factory_(this) {}

FileMonitorImpl::~FileMonitorImpl() = default;

void FileMonitorImpl::Initialize(const InitCallback& callback) {
  base::PostTaskAndReplyWithResult(
      file_thread_task_runner_.get(), FROM_HERE,
      base::Bind(&CreateDirectoryIfNotExists, download_file_dir_), callback);
}

void FileMonitorImpl::DeleteUnknownFiles(
    const Model::EntryList& known_entries,
    const std::vector<DriverEntry>& known_driver_entries) {
  std::set<base::FilePath> download_file_paths;
  for (Entry* entry : known_entries) {
    download_file_paths.insert(entry->target_file_path);
  }

  for (const DriverEntry& driver_entry : known_driver_entries) {
    download_file_paths.insert(driver_entry.current_file_path);
  }

  file_thread_task_runner_->PostTask(
      FROM_HERE, base::Bind(&DeleteUnknownFilesOnFileThread, download_file_dir_,
                            download_file_paths));
}

std::vector<Entry*> FileMonitorImpl::CleanupFilesForCompletedEntries(
    const Model::EntryList& entries) {
  std::vector<Entry*> entries_to_remove;
  std::set<base::FilePath> files_to_remove;
  for (auto* entry : entries) {
    if (!ReadyForCleanup(entry))
      continue;

    entries_to_remove.push_back(entry);
    files_to_remove.insert(entry->target_file_path);
  }

  file_thread_task_runner_->PostTask(
      FROM_HERE, base::Bind(&DeleteFilesOnFileThread, files_to_remove,
                            stats::FileCleanupReason::TIMEOUT));
  return entries_to_remove;
}

void FileMonitorImpl::DeleteFiles(
    const std::set<base::FilePath>& files_to_remove,
    stats::FileCleanupReason reason) {
  file_thread_task_runner_->PostTask(
      FROM_HERE, base::Bind(&DeleteFilesOnFileThread, files_to_remove, reason));
}

void FileMonitorImpl::HardRecover(const InitCallback& callback) {
  base::PostTaskAndReplyWithResult(
      file_thread_task_runner_.get(), FROM_HERE,
      base::Bind(&HardRecoverOnFileThread, download_file_dir_), callback);
}

bool FileMonitorImpl::ReadyForCleanup(const Entry* entry) {
  return entry->state == Entry::State::COMPLETE &&
         (base::Time::Now() - entry->completion_time) > file_keep_alive_time_;
}

}  // namespace download
