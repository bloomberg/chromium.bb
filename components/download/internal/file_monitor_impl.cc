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
#include "base/threading/thread_restrictions.h"

namespace download {

FileMonitorImpl::FileMonitorImpl(
    const base::FilePath& download_file_dir,
    const scoped_refptr<base::SequencedTaskRunner>& file_thread_task_runner,
    base::TimeDelta file_keep_alive_time)
    : download_file_dir_(download_file_dir),
      file_keep_alive_time_(file_keep_alive_time),
      file_thread_task_runner_(file_thread_task_runner),
      weak_factory_(this) {}

FileMonitorImpl::~FileMonitorImpl() = default;

void FileMonitorImpl::DeleteUnknownFiles(
    const Model::EntryList& known_entries,
    const std::vector<DriverEntry>& known_driver_entries) {
  std::set<base::FilePath> download_file_paths;
  for (Entry* entry : known_entries) {
    download_file_paths.insert(entry->target_file_path);
  }

  for (const DriverEntry& driver_entry : known_driver_entries) {
    download_file_paths.insert(driver_entry.temporary_physical_file_path);
  }

  file_thread_task_runner_->PostTask(
      FROM_HERE, base::Bind(&FileMonitorImpl::DeleteUnknownFilesOnFileThread,
                            weak_factory_.GetWeakPtr(), download_file_paths));
}

std::vector<Entry*> FileMonitorImpl::CleanupFilesForCompletedEntries(
    const Model::EntryList& entries) {
  std::vector<Entry*> entries_to_remove;
  std::vector<base::FilePath> files_to_remove;
  for (auto* entry : entries) {
    if (!ReadyForCleanup(entry))
      continue;

    entries_to_remove.push_back(entry);
    files_to_remove.push_back(entry->target_file_path);
  }

  file_thread_task_runner_->PostTask(
      FROM_HERE, base::Bind(&FileMonitorImpl::DeleteFilesOnFileThread,
                            weak_factory_.GetWeakPtr(), files_to_remove,
                            stats::FileCleanupReason::TIMEOUT));
  return entries_to_remove;
}

void FileMonitorImpl::DeleteFiles(
    const std::vector<base::FilePath>& files_to_remove,
    stats::FileCleanupReason reason) {
  file_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&FileMonitorImpl::DeleteFilesOnFileThread,
                 weak_factory_.GetWeakPtr(), files_to_remove, reason));
}

void FileMonitorImpl::DeleteUnknownFilesOnFileThread(
    const std::set<base::FilePath>& download_file_paths) {
  base::ThreadRestrictions::AssertIOAllowed();
  std::set<base::FilePath> files_in_dir;
  base::FileEnumerator file_enumerator(
      download_file_dir_, false /* recursive */, base::FileEnumerator::FILES);

  for (base::FilePath path = file_enumerator.Next(); !path.value().empty();
       path = file_enumerator.Next()) {
    files_in_dir.insert(path);
  }

  std::vector<base::FilePath> files_to_remove =
      base::STLSetDifference<std::vector<base::FilePath>>(files_in_dir,
                                                          download_file_paths);
  DeleteFilesOnFileThread(files_to_remove, stats::FileCleanupReason::UNKNOWN);
}

void FileMonitorImpl::DeleteFilesOnFileThread(
    const std::vector<base::FilePath>& paths,
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

bool FileMonitorImpl::ReadyForCleanup(const Entry* entry) {
  return entry->state == Entry::State::COMPLETE &&
         (base::Time::Now() - entry->completion_time) > file_keep_alive_time_;
}

}  // namespace download
