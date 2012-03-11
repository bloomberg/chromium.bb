// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/download_persistent_store_info.h"

namespace content {

DownloadPersistentStoreInfo::DownloadPersistentStoreInfo()
    : received_bytes(0),
      total_bytes(0),
      state(0),
      db_handle(0),
      opened(false) {
}

DownloadPersistentStoreInfo::DownloadPersistentStoreInfo(
    const FilePath& path,
    const GURL& url,
    const GURL& referrer,
    const base::Time& start,
    const base::Time& end,
    int64 received,
    int64 total,
    int32 download_state,
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

DownloadPersistentStoreInfo::~DownloadPersistentStoreInfo() {
}

}  // namespace content
