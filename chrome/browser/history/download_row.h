// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_DOWNLOAD_ROW_H_
#define CHROME_BROWSER_HISTORY_DOWNLOAD_ROW_H_

#include <vector>

#include "base/files/file_path.h"
#include "base/time.h"
#include "content/public/browser/download_danger_type.h"
#include "content/public/browser/download_interrupt_reasons.h"
#include "content/public/browser/download_item.h"
#include "googleurl/src/gurl.h"

namespace history {

// Contains the information that is stored in the download system's persistent
// store (or refers to it). DownloadHistory uses this to communicate with the
// DownloadDatabase through the HistoryService.
struct DownloadRow {
  DownloadRow();
  DownloadRow(
      const base::FilePath& current_path,
      const base::FilePath& target_path,
      const std::vector<GURL>& url_chain,
      const GURL& referrer,
      const base::Time& start,
      const base::Time& end,
      int64 received,
      int64 total,
      content::DownloadItem::DownloadState download_state,
      content::DownloadDangerType danger_type,
      content::DownloadInterruptReason interrupt_reason,
      int64 handle,
      bool download_opened);
  ~DownloadRow();

  // The current path to the download (potentially different from final if
  // download is in progress or interrupted).
  base::FilePath current_path;

  // The target path where the download will go when it's complete.
  base::FilePath target_path;

  // The URL redirect chain through which we are downloading.  The front
  // is the url that the initial request went to, and the back is the
  // url from which we ended up getting data.  This is not changed by
  // UpdateDownload().
  std::vector<GURL> url_chain;

  // The URL that referred us. Is not changed by UpdateDownload().
  GURL referrer_url;

  // The time when the download started. Is not changed by UpdateDownload().
  base::Time start_time;

  // The time when the download completed.
  base::Time end_time;

  // The number of bytes received (so far).
  int64 received_bytes;

  // The total number of bytes in the download. Is not changed by
  // UpdateDownload().
  int64 total_bytes;

  // The current state of the download.
  content::DownloadItem::DownloadState state;

  // Whether and how the download is dangerous.
  content::DownloadDangerType danger_type;

  // The reason the download was interrupted, if
  // state == DownloadItem::INTERRUPTED
  content::DownloadInterruptReason interrupt_reason;

  // The handle of the download in the database. Is not changed by
  // UpdateDownload().
  int64 db_handle;

  // Whether this download has ever been opened from the browser.
  bool opened;
};

}  // namespace history

#endif  // CHROME_BROWSER_HISTORY_DOWNLOAD_ROW_H_
