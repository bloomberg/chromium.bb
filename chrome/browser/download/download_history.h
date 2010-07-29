// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_HISTORY_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_HISTORY_H_
#pragma once

#include <vector>

#include "base/basictypes.h"
#include "chrome/browser/cancelable_request.h"
#include "chrome/browser/history/history.h"

class DownloadItem;
class Profile;

namespace base {
class Time;
}

// Interacts with the HistoryService on behalf of the download subsystem.
class DownloadHistory {
 public:
  // Converts history database handles to download items.
  class DownloadItemMapper {
   public:
    virtual ~DownloadItemMapper() {}

    // Returns a DownloadItem* corresponding to |db_handle|, or NULL if no such
    // DownloadItem exists.
    virtual DownloadItem* GetDownloadItemFromDbHandle(int64 db_handle) = 0;
  };

  // A fake download table ID which represents a download that has started,
  // but is not yet in the table.
  static const int kUninitializedHandle;

  typedef Callback1<std::vector<DownloadItem*> >::Type DownloadSearchCallback;

  DownloadHistory(Profile* profile, DownloadItemMapper* mapper);

  // Retrieves DownloadCreateInfos saved in the history.
  void Load(HistoryService::DownloadQueryCallback* callback);

  // Starts a history search for downloads and invokes |callback| when done.
  void Search(const string16& query,
              DownloadSearchCallback* callback);

  // Adds a new entry for a download to the history database.
  void AddEntry(const DownloadCreateInfo& info,
                DownloadItem* download_item,
                HistoryService::DownloadCreateCallback* callback);

  // Updates the history entry for |download_item|.
  void UpdateEntry(DownloadItem* download_item);

  // Updates the download path for |download_item| to |new_path|.
  void UpdateDownloadPath(DownloadItem* download_item,
                          const FilePath& new_path);

  // Removes |download_item| from the history database.
  void RemoveEntry(DownloadItem* download_item);

  // Removes download-related history entries in the given time range.
  void RemoveEntriesBetween(const base::Time remove_begin,
                            const base::Time remove_end);

  // Returns a new unique database handle which will not collide with real ones.
  int64 GetNextFakeDbHandle();

 private:
  void OnSearchDownloadsComplete(HistoryService::Handle handle,
                                 std::vector<int64>* results);

  Profile* profile_;

  // In case we don't have a valid db_handle, we use |fake_db_handle_| instead.
  // This is useful for incognito mode or when the history database is offline.
  // Downloads are expected to have unique handles, so we decrement the next
  // fake handle value on every use.
  int64 next_fake_db_handle_;

  DownloadItemMapper* download_item_mapper_;

  CancelableRequestConsumerTSimple<DownloadSearchCallback*> history_consumer_;

  DISALLOW_COPY_AND_ASSIGN(DownloadHistory);
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_HISTORY_H_
