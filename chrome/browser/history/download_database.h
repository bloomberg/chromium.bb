// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_DOWNLOAD_DATABASE_H_
#define CHROME_BROWSER_HISTORY_DOWNLOAD_DATABASE_H_
#pragma once

#include <set>
#include <string>

#include "base/threading/platform_thread.h"
#include "chrome/browser/history/history_types.h"
#include "sql/meta_table.h"

class FilePath;

namespace content {
struct DownloadPersistentStoreInfo;
}

namespace sql {
class Connection;
}

namespace history {

// Maintains a table of downloads.
class DownloadDatabase {
 public:
  // Must call InitDownloadTable before using any other functions.
  DownloadDatabase();
  virtual ~DownloadDatabase();

  int next_download_id() const { return next_id_; }

  // Get all the downloads from the database.
  void QueryDownloads(
      std::vector<content::DownloadPersistentStoreInfo>* results);

  // Update the state of one download. Returns true if successful.
  // Does not update |url|, |start_time|, |total_bytes|; uses |db_handle| only
  // to select the row in the database table to update.
  bool UpdateDownload(const content::DownloadPersistentStoreInfo& data);

  // Update the path of one download. Returns true if successful.
  bool UpdateDownloadPath(const FilePath& path, DownloadID db_handle);

  // Fixes state of the download entries. Sometimes entries with IN_PROGRESS
  // state are not updated during browser shutdown (particularly when crashing).
  // On the next start such entries are considered canceled. This functions
  // fixes such entries.
  bool CleanUpInProgressEntries();

  // Create a new database entry for one download and return its primary db id.
  int64 CreateDownload(const content::DownloadPersistentStoreInfo& info);

  // Remove a download from the database.
  void RemoveDownload(DownloadID db_handle);

  // Remove all completed downloads that started after |remove_begin|
  // (inclusive) and before |remove_end|. You may use null Time values
  // to do an unbounded delete in either direction. This function ignores
  // all downloads that are in progress or are waiting to be cancelled.
  bool RemoveDownloadsBetween(base::Time remove_begin, base::Time remove_end);

 protected:
  // Returns the database for the functions in this interface.
  virtual sql::Connection& GetDB() = 0;

  // Returns the meta-table object for the functions in this interface.
  virtual sql::MetaTable& GetMetaTable() = 0;

  // Creates the downloads table if needed.
  bool InitDownloadTable();

  // Used to quickly clear the downloads. First you would drop it, then you
  // would re-initialize it.
  bool DropDownloadTable();

 private:
  // TODO(rdsmith): Remove after http://crbug.com/96627 has been resolved.
  void CheckThread();

  bool EnsureColumnExists(const std::string& name, const std::string& type);

  bool owning_thread_set_;
  base::PlatformThreadId owning_thread_;

  int next_id_;
  int next_db_handle_;

  DISALLOW_COPY_AND_ASSIGN(DownloadDatabase);
};

}  // namespace history

#endif  // CHROME_BROWSER_HISTORY_DOWNLOAD_DATABASE_H_
