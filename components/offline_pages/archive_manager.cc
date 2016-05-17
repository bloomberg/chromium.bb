// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "components/offline_pages/archive_manager.h"

namespace offline_pages {

namespace {

void EnsureArchivesDirCreatedImpl(const base::FilePath& archives_dir) {
  CHECK(base::CreateDirectory(archives_dir));
}

void ExistsArchiveImpl(const base::FilePath& file_path, bool* exists) {
  DCHECK(exists);
  *exists = base::PathExists(file_path);
}

void DeleteArchivesImpl(const std::vector<base::FilePath>& file_paths,
                        bool* result) {
  DCHECK(result);
  for (const auto& file_path : file_paths) {
    // Make sure delete happens on the left of || so that it is always executed.
    *result = base::DeleteFile(file_path, false) || *result;
  }
}

void CompleteBooleanCallback(const base::Callback<void(bool)>& callback,
                             bool* exists) {
  callback.Run(*exists);
}
}  // namespace

ArchiveManager::ArchiveManager(
    const base::FilePath& archives_dir,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : archives_dir_(archives_dir), task_runner_(task_runner) {}

ArchiveManager::~ArchiveManager() {}

void ArchiveManager::EnsureArchivesDirCreated(const base::Closure& callback) {
  task_runner_->PostTaskAndReply(
      FROM_HERE, base::Bind(EnsureArchivesDirCreatedImpl, archives_dir_),
      callback);
}

void ArchiveManager::ExistsArchive(const base::FilePath& archive_path,
                                   const base::Callback<void(bool)>& callback) {
  bool* result = new bool(false);
  task_runner_->PostTaskAndReply(
      FROM_HERE, base::Bind(ExistsArchiveImpl, archive_path, result),
      base::Bind(CompleteBooleanCallback, callback, base::Owned(result)));
}

void ArchiveManager::DeleteArchive(const base::FilePath& archive_path,
                                   const base::Callback<void(bool)>& callback) {
  std::vector<base::FilePath> archive_paths = {archive_path};
  DeleteMultipleArchives(archive_paths, callback);
}

void ArchiveManager::DeleteMultipleArchives(
    const std::vector<base::FilePath>& archive_paths,
    const base::Callback<void(bool)>& callback) {
  bool* result = new bool(false);
  task_runner_->PostTaskAndReply(
      FROM_HERE, base::Bind(DeleteArchivesImpl, archive_paths, result),
      base::Bind(CompleteBooleanCallback, callback, base::Owned(result)));
}

}  // namespace offline_pages
