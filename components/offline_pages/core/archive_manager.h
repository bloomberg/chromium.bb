// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_ARCHIVE_MANAGER_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_ARCHIVE_MANAGER_H_

#include <set>
#include <vector>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace offline_pages {

// Class that manages all archive files for offline pages. They are stored in
// different archive directories based on their lifetime types (persistent or
// temporary).
// All tasks are performed using |task_runner_|.
// TODO(romax): When P2P sharing comes and pages saved in internal storage get
// moved out to external storage, make sure nothing breaks (and it's necessary
// to check if both internal/external are on the volume, in case any logic
// breaks.)
class ArchiveManager {
 public:
  // Used by metrics collection and clearing storage of temporary pages. The
  // |free_disk_space| will be the free disk space of the volume that contains
  // temporary archives directory. Another field may be added if the free disk
  // space of persistent archives is needed in future.
  struct StorageStats {
    int64_t total_archives_size() const {
      return temporary_archives_size + private_archives_size;
    }
    int64_t free_disk_space;
    int64_t temporary_archives_size;
    int64_t private_archives_size;
  };

  ArchiveManager(const base::FilePath& temporary_archives_dir,
                 const base::FilePath& private_archives_dir_,
                 const base::FilePath& public_archives_dir,
                 const scoped_refptr<base::SequencedTaskRunner>& task_runner);
  virtual ~ArchiveManager();

  // Creates archives directory if one does not exist yet;
  virtual void EnsureArchivesDirCreated(const base::Closure& callback);

  // Checks whether an archive with specified |archive_path| exists.
  virtual void ExistsArchive(const base::FilePath& archive_path,
                             const base::Callback<void(bool)>& callback);

  // Deletes an archive with specified |archive_path|.
  // It is considered successful to attempt to delete a file that does not
  // exist.
  virtual void DeleteArchive(const base::FilePath& archive_path,
                             const base::Callback<void(bool)>& callback);

  // Deletes multiple archives that are specified in |archive_paths|.
  // It is considered successful to attempt to delete a file that does not
  // exist.
  virtual void DeleteMultipleArchives(
      const std::vector<base::FilePath>& archive_paths,
      const base::Callback<void(bool)>& callback);

  // Lists all archive files in both temporary and persistent archive
  // directories.
  virtual void GetAllArchives(
      const base::Callback<void(const std::set<base::FilePath>&)>& callback)
      const;

  // Gets stats about archive storage, i.e. sizes of temporary and persistent
  // archive files and free disk space.
  virtual void GetStorageStats(
      const base::Callback<void(const StorageStats& storage_sizes)>& callback)
      const;

  // Gets the archive directories.
  const base::FilePath& GetTemporaryArchivesDir() const;
  const base::FilePath& GetPrivateArchivesDir() const;
  const base::FilePath& GetPublicArchivesDir() const;

 protected:
  ArchiveManager();

 private:
  // Path under which all of the temporary archives should be stored.
  base::FilePath temporary_archives_dir_;
  // Path under which all of the persistent archives should be saved initially.
  base::FilePath private_archives_dir_;
  // Publically accessible path where archives should be moved once ready.
  base::FilePath public_archives_dir_;
  // Task runner for running file operations.
  // Since the task_runner is a SequencedTaskRunner, it's guaranteed that the
  // second task will start after the first one. This is an important assumption
  // for |ArchiveManager::EnsureArchivesDirCreated|.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(ArchiveManager);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_ARCHIVE_MANAGER_H_
