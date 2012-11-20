// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/download_row.h"

namespace history {

DownloadRow::DownloadRow()
    : received_bytes(0),
      total_bytes(0),
      state(content::DownloadItem::IN_PROGRESS),
      db_handle(0),
      opened(false) {
}

DownloadRow::DownloadRow(
    const FilePath& path,
    const GURL& url,
    const GURL& referrer,
    const base::Time& start,
    const base::Time& end,
    int64 received,
    int64 total,
    content::DownloadItem::DownloadState download_state,
    int64 handle,
    bool download_opened)
    : path(path),
      url(url),
      referrer_url(referrer),
      start_time(start),
      end_time(end),
      received_bytes(received),
      total_bytes(total),
      state(download_state),
      db_handle(handle),
      opened(download_opened) {
}

DownloadRow::~DownloadRow() {
}

}  // namespace history
