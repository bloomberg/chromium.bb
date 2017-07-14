// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_FILE_MONITOR_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_FILE_MONITOR_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "components/download/internal/model.h"
#include "components/download/internal/stats.h"

namespace base {
class FilePath;
}  // namespace base

namespace download {

struct DriverEntry;
struct Entry;

// An utility class containing various file cleanup methods.
class FileMonitor {
 public:
  using InitCallback = base::Callback<void(bool)>;

  // Creates the file directory for the downloads if it doesn't exist.
  virtual void Initialize(const InitCallback& callback) = 0;

  // Deletes the files in storage directory that are not related to any entries
  // in either database.
  virtual void DeleteUnknownFiles(
      const Model::EntryList& known_entries,
      const std::vector<DriverEntry>& known_driver_entries) = 0;

  // Deletes the files for the database entries which have been completed and
  // ready for cleanup. Returns the entries eligible for clean up.
  virtual std::vector<Entry*> CleanupFilesForCompletedEntries(
      const Model::EntryList& entries,
      const base::Closure& completion_callback) = 0;

  // Deletes a list of files and logs UMA.
  virtual void DeleteFiles(const std::set<base::FilePath>& files_to_remove,
                           stats::FileCleanupReason reason) = 0;

  // Deletes all files in the download service directory.  This is a hard reset
  // on this directory.
  virtual void HardRecover(const InitCallback& callback) = 0;

  virtual ~FileMonitor() = default;
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_FILE_MONITOR_H_
