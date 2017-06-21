// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_FILE_MONITOR_IMPL_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_FILE_MONITOR_IMPL_H_

#include "components/download/internal/file_monitor.h"

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "components/download/internal/driver_entry.h"
#include "components/download/internal/model.h"
#include "components/download/internal/stats.h"

namespace download {

struct Entry;

// An utility class containing various file cleanup methods.
class FileMonitorImpl : public FileMonitor {
 public:
  FileMonitorImpl(
      const base::FilePath& download_file_dir,
      const scoped_refptr<base::SequencedTaskRunner>& file_thread_task_runner,
      base::TimeDelta file_keep_alive_time);
  ~FileMonitorImpl() override;

  void Initialize(const InitCallback& callback) override;
  void DeleteUnknownFiles(
      const Model::EntryList& known_entries,
      const std::vector<DriverEntry>& known_driver_entries) override;
  std::vector<Entry*> CleanupFilesForCompletedEntries(
      const Model::EntryList& entries) override;
  void DeleteFiles(const std::vector<base::FilePath>& files_to_remove,
                   stats::FileCleanupReason reason) override;

 private:
  void DeleteUnknownFilesOnFileThread(
      const std::set<base::FilePath>& paths_in_db);
  void DeleteFilesOnFileThread(const std::vector<base::FilePath>& paths,
                               stats::FileCleanupReason reason);
  bool ReadyForCleanup(const Entry* entry);

  const base::FilePath download_file_dir_;
  const base::TimeDelta file_keep_alive_time_;

  scoped_refptr<base::SequencedTaskRunner> file_thread_task_runner_;
  base::WeakPtrFactory<FileMonitorImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FileMonitorImpl);
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_FILE_MONITOR_IMPL_H_
