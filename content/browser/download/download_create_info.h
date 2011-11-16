// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_CREATE_INFO_H_
#define CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_CREATE_INFO_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/time.h"
#include "content/browser/download/download_file.h"
#include "content/browser/download/download_id.h"
#include "content/browser/download/download_types.h"
#include "content/common/content_export.h"
#include "content/public/common/page_transition_types.h"
#include "googleurl/src/gurl.h"

// Used for informing the download manager of a new download, since we don't
// want to pass |DownloadItem|s between threads.
struct CONTENT_EXPORT DownloadCreateInfo {
  DownloadCreateInfo(const FilePath& path,
                     const GURL& url,
                     const base::Time& start_time,
                     int64 received_bytes,
                     int64 total_bytes,
                     int32 state,
                     const DownloadId& download_id,
                     bool has_user_gesture,
                     content::PageTransition transition_type);
  DownloadCreateInfo();
  ~DownloadCreateInfo();

  std::string DebugString() const;

  // The URL from which we are downloading. This is the final URL after any
  // redirection by the server for |url_chain|.
  const GURL& url() const;

  // DownloadItem fields
  // The path where we want to save the download file.
  FilePath path;

  // The chain of redirects that leading up to and including the final URL.
  std::vector<GURL> url_chain;

  // The URL that referred us.
  GURL referrer_url;

  // A number that should be added to the suggested path to make it unique.
  // 0 means no number should be appended.  Not actually stored in the db.
  int path_uniquifier;

  // The time when the download started.
  base::Time start_time;

  // The number of bytes that have been received.
  int64 received_bytes;

  // The total download size.
  int64 total_bytes;

  // The current state of the download.
  int32 state;

  // The (per-session) ID of the download.
  DownloadId download_id;

  // True if the download was initiated by user action.
  bool has_user_gesture;

  content::PageTransition transition_type;

  // The handle of the download in the history database.
  int64 db_handle;

  // The content-disposition string from the response header.
  std::string content_disposition;

  // The mime type string from the response header (may be overridden).
  std::string mime_type;

  // The value of the content type header sent with the downloaded item.  It
  // may be different from |mime_type|, which may be set based on heuristics
  // which may look at the file extension and first few bytes of the file.
  std::string original_mime_type;

  // True if we should display the 'save as...' UI and prompt the user
  // for the download location.
  // False if the UI should be supressed and the download performed to the
  // default location.
  bool prompt_user_for_save_location;

  // The original name for a dangerous download.
  FilePath original_name;

  // The charset of the referring page where the download request comes from.
  // It's used to construct a suggested filename.
  std::string referrer_charset;

  // The download file save info.
  DownloadSaveInfo save_info;
};

#endif  // CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_CREATE_INFO_H_
