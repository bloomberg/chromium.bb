// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Download struct used for informing and querying the download system's
// persitent store.

#ifndef CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_PERSISTENT_STORE_INFO_H_
#define CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_PERSISTENT_STORE_INFO_H_
#pragma once

#include <vector>

#include "base/file_path.h"
#include "base/time.h"
#include "content/common/content_export.h"
#include "googleurl/src/gurl.h"

class DownloadItem;

// Contains the information that is stored in the download system's persistent
// store (or refers to it).  Managed by the DownloadItem.
struct CONTENT_EXPORT DownloadPersistentStoreInfo {
  // TODO(ahendrickson) -- Reduce the number of constructors.
  DownloadPersistentStoreInfo();
  DownloadPersistentStoreInfo(const FilePath& path,
                              const GURL& url,
                              const GURL& referrer,
                              const base::Time& start,
                              int64 received,
                              int64 total,
                              int32 download_state,
                              int64 handle);
  ~DownloadPersistentStoreInfo();  // For linux-clang.

  // The final path where the download is saved.
  FilePath path;

  // The URL from which we are downloading. This is the final URL after any
  // redirection by the server for |url_chain|.
  GURL url;

  // The URL that referred us.
  GURL referrer_url;

  // The time when the download started.
  base::Time start_time;

  // The number of bytes received (so far).
  int64 received_bytes;

  // The total number of bytes in the download.
  int64 total_bytes;

  // The current state of the download.
  int32 state;

  // The handle of the download in the database.
  int64 db_handle;
};

#endif  // CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_PERSISTENT_STORE_INFO_H_
