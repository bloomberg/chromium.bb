// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_DOWNLOAD_ROW_H_
#define CHROME_BROWSER_HISTORY_DOWNLOAD_ROW_H_

#include "base/file_path.h"
#include "base/time.h"
#include "content/public/browser/download_item.h"
#include "googleurl/src/gurl.h"

namespace history {

// Contains the information that is stored in the download system's persistent
// store (or refers to it). DownloadHistory uses this to communicate with the
// DownloadDatabase through the HistoryService.
struct DownloadRow {
  DownloadRow();
  DownloadRow(
      const FilePath& path,
      const GURL& url,
      const GURL& referrer,
      const base::Time& start,
      const base::Time& end,
      int64 received,
      int64 total,
      content::DownloadItem::DownloadState download_state,
      int64 handle,
      bool download_opened);
  ~DownloadRow();

  // The current path to the downloaded file.
  // TODO(benjhayden/asanka): Persist the target filename as well.
  FilePath path;

  // The URL from which we are downloading. This is the final URL after any
  // redirection by the server for |url_chain|. Is not changed by
  // UpdateDownload().
  GURL url;

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

  // The handle of the download in the database. Is not changed by
  // UpdateDownload().
  int64 db_handle;

  // Whether this download has ever been opened from the browser.
  bool opened;
};

}  // namespace history

#endif  // CHROME_BROWSER_HISTORY_DOWNLOAD_ROW_H_
