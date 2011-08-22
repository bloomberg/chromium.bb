// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/download_persistent_store_info.h"

#include "content/browser/download/download_item.h"

DownloadPersistentStoreInfo::DownloadPersistentStoreInfo()
    : received_bytes(0),
      total_bytes(0),
      state(0),
      db_handle(0) {
}

DownloadPersistentStoreInfo::DownloadPersistentStoreInfo(
    const FilePath& path,
    const GURL& url,
    const GURL& referrer,
    const base::Time& start,
    int64 received,
    int64 total,
    int32 download_state,
    int64 handle)
    : path(path),
      url(url),
      referrer_url(referrer),
      start_time(start),
      received_bytes(received),
      total_bytes(total),
      state(download_state),
      db_handle(handle) {
}

DownloadPersistentStoreInfo::~DownloadPersistentStoreInfo() {
}
