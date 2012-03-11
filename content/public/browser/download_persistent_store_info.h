// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBlIC_BROWSER_DOWNLOAD_PERSISTENT_STORE_INFO_H_
#define CONTENT_PUBlIC_BROWSER_DOWNLOAD_PERSISTENT_STORE_INFO_H_
#pragma once

#include "base/file_path.h"
#include "base/time.h"
#include "content/common/content_export.h"
#include "googleurl/src/gurl.h"

namespace content {

// Contains the information that is stored in the download system's persistent
// store (or refers to it).  Managed by the DownloadItem.  When used to create a
// history entry, all fields except for |db_handle| are set by DownloadItem and
// read by DownloadDatabase.  When used to update a history entry, DownloadItem
// sets all fields, but DownloadDatabase only reads |end_time|,
// |received_bytes|, |state|, and |opened|, and uses |db_handle| to select the
// row in the database table to update.  When used to load DownloadItems from
// the history, all fields except |referrer_url| are set by the DownloadDatabase
// and read by the DownloadItem.
struct CONTENT_EXPORT DownloadPersistentStoreInfo {
  DownloadPersistentStoreInfo();
  DownloadPersistentStoreInfo(const FilePath& path,
                              const GURL& url,
                              const GURL& referrer,
                              const base::Time& start,
                              const base::Time& end,
                              int64 received,
                              int64 total,
                              int32 download_state,
                              int64 handle,
                              bool download_opened);
  ~DownloadPersistentStoreInfo();  // For linux-clang.

  // The final path where the download is saved.
  FilePath path;

  // The URL from which we are downloading. This is the final URL after any
  // redirection by the server for |url_chain|. Is not changed by UpdateEntry().
  GURL url;

  // The URL that referred us.
  GURL referrer_url;

  // The time when the download started. Is not changed by UpdateEntry().
  base::Time start_time;

  // The time when the download completed.
  base::Time end_time;

  // The number of bytes received (so far).
  int64 received_bytes;

  // The total number of bytes in the download. Is not changed by UpdateEntry().
  int64 total_bytes;

  // The current state of the download.
  int32 state;

  // The handle of the download in the database. Is not changed by
  // UpdateEntry().
  int64 db_handle;

  // Whether this download has ever been opened from the browser.
  bool opened;
};

}  // namespace content

#endif  // CONTENT_PUBlIC_BROWSER_DOWNLOAD_PERSISTENT_STORE_INFO_H_
