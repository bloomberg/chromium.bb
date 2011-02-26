// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Download creation struct used for querying the history service.

#ifndef CHROME_BROWSER_HISTORY_DOWNLOAD_CREATE_INFO_H_
#define CHROME_BROWSER_HISTORY_DOWNLOAD_CREATE_INFO_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/time.h"
#include "chrome/browser/download/download_file.h"
#include "googleurl/src/gurl.h"

// Used for informing the download database of a new download, where we don't
// want to pass DownloadItems between threads. The history service also uses a
// vector of these structs for passing us the state of all downloads at
// initialization time (see DownloadQueryInfo below).
struct DownloadCreateInfo {
  DownloadCreateInfo(const FilePath& path,
                     const GURL& url,
                     base::Time start_time,
                     int64 received_bytes,
                     int64 total_bytes,
                     int32 state,
                     int32 download_id,
                     bool has_user_gesture);
  DownloadCreateInfo();
  ~DownloadCreateInfo();

  // Indicates if the download is dangerous.
  bool IsDangerous();

  std::string DebugString() const;

  // DownloadItem fields
  FilePath path;
  // The URL from which we are downloading. This is the final URL after any
  // redirection by the server for |original_url_|.
  GURL url;
  // The original URL before any redirection by the server for this URL.
  GURL original_url;
  GURL referrer_url;
  FilePath suggested_path;
  // A number that should be added to the suggested path to make it unique.
  // 0 means no number should be appended.  Not actually stored in the db.
  int path_uniquifier;
  base::Time start_time;
  int64 received_bytes;
  int64 total_bytes;
  int32 state;
  int32 download_id;
  bool has_user_gesture;
  int child_id;
  int render_view_id;
  int request_id;
  int64 db_handle;
  std::string content_disposition;
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
  // Whether this download file is potentially dangerous (ex: exe, dll, ...).
  bool is_dangerous_file;
  // If safebrowsing believes this URL leads to malware.
  bool is_dangerous_url;
  // The original name for a dangerous download.
  FilePath original_name;
  // Whether this download is for extension install or not.
  bool is_extension_install;
  // The charset of the referring page where the download request comes from.
  // It's used to construct a suggested filename.
  std::string referrer_charset;
  // The download file save info.
  DownloadSaveInfo save_info;
};

#endif  // CHROME_BROWSER_HISTORY_DOWNLOAD_CREATE_INFO_H_
