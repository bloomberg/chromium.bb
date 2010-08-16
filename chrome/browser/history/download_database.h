// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_DOWNLOAD_DATABASE_H_
#define CHROME_BROWSER_HISTORY_DOWNLOAD_DATABASE_H_
#pragma once

#include "chrome/browser/history/history_types.h"

struct DownloadCreateInfo;
class FilePath;

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

  // Get all the downloads from the database.
  void QueryDownloads(std::vector<DownloadCreateInfo>* results);

  // Update the state of one download. Returns true if successful.
  bool UpdateDownload(int64 received_bytes, int32 state, DownloadID db_handle);

  // Update the path of one download. Returns true if successful.
  bool UpdateDownloadPath(const FilePath& path, DownloadID db_handle);

  // Fixes state of the download entries. Sometimes entries with IN_PROGRESS
  // state are not updated during browser shutdown (particularly when crashing).
  // On the next start such entries are considered canceled. This functions
  // fixes such entries.
  bool CleanUpInProgressEntries();

  // Create a new database entry for one download and return its primary db id.
  int64 CreateDownload(const DownloadCreateInfo& info);

  // Remove a download from the database.
  void RemoveDownload(DownloadID db_handle);

  // Remove all completed downloads that started after |remove_begin|
  // (inclusive) and before |remove_end|. You may use null Time values
  // to do an unbounded delete in either direction. This function ignores
  // all downloads that are in progress or are waiting to be cancelled.
  void RemoveDownloadsBetween(base::Time remove_begin, base::Time remove_end);

 protected:
  // Returns the database for the functions in this interface.
  virtual sql::Connection& GetDB() = 0;

  // Creates the downloads table if needed.
  bool InitDownloadTable();

  // Used to quickly clear the downloads. First you would drop it, then you
  // would re-initialize it.
  bool DropDownloadTable();

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadDatabase);
};

}  // namespace history

#endif  // CHROME_BROWSER_HISTORY_DOWNLOAD_DATABASE_H_
