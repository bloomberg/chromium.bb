// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_FILE_MONITOR_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_FILE_MONITOR_H_

#include <memory>
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
  // Deletes the files in storage directory that are not related to any entries
  // in either database.
  virtual void DeleteUnknownFiles(
      const Model::EntryList& known_entries,
      const std::vector<DriverEntry>& known_driver_entries) = 0;

  // Deletes the files for the database entries which have been completed and
  // ready for cleanup. Returns the entries eligible for clean up.
  virtual std::vector<Entry*> CleanupFilesForCompletedEntries(
      const Model::EntryList& entries) = 0;

  // Deletes a list of files and logs UMA.
  virtual void DeleteFiles(const std::vector<base::FilePath>& files_to_remove,
                           stats::FileCleanupReason reason) = 0;

  virtual ~FileMonitor() = default;
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_FILE_MONITOR_H_
