// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_CREATE_INFO_H_
#define CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_CREATE_INFO_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/time.h"
#include "content/browser/download/download_file.h"
#include "content/browser/download/download_request_handle.h"
#include "content/common/content_export.h"
#include "content/public/browser/download_id.h"
#include "content/public/browser/download_save_info.h"
#include "content/public/common/page_transition_types.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_log.h"

// Used for informing the download manager of a new download, since we don't
// want to pass |DownloadItem|s between threads.
struct CONTENT_EXPORT DownloadCreateInfo {
  DownloadCreateInfo(const base::Time& start_time,
                     int64 received_bytes,
                     int64 total_bytes,
                     int32 state,
                     const net::BoundNetLog& bound_net_log,
                     bool has_user_gesture,
                     content::PageTransition transition_type);
  DownloadCreateInfo();
  ~DownloadCreateInfo();

  std::string DebugString() const;

  // The URL from which we are downloading. This is the final URL after any
  // redirection by the server for |url_chain|.
  const GURL& url() const;

  // The chain of redirects that leading up to and including the final URL.
  std::vector<GURL> url_chain;

  // The URL that referred us.
  GURL referrer_url;

  // The time when the download started.
  base::Time start_time;

  // The number of bytes that have been received.
  int64 received_bytes;

  // The total download size.
  int64 total_bytes;

  // The current state of the download.
  int32 state;

  // The (per-session) ID of the download.
  content::DownloadId download_id;

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

  // For continuing a download, the modification time of the file.
  // Storing as a string for exact match to server format on
  // "If-Unmodified-Since" comparison.
  std::string last_modified;

  // For continuing a download, the ETAG of the file.
  std::string etag;

  // True if we should display the 'save as...' UI and prompt the user
  // for the download location.
  // False if the UI should be suppressed and the download performed to the
  // default location.
  bool prompt_user_for_save_location;

  // The charset of the referring page where the download request comes from.
  // It's used to construct a suggested filename.
  std::string referrer_charset;

  // The download file save info.
  content::DownloadSaveInfo save_info;

  // The remote IP address where the download was fetched from.  Copied from
  // UrlRequest::GetSocketAddress().
  std::string remote_address;

  // The handle to the URLRequest sourcing this download.
  DownloadRequestHandle request_handle;

  // Default directory to use for this download. The final target path may not
  // be determined until much later. In the meantime, this directory (if
  // non-empty) should be used to store teh download file.
  // TODO(asanka,rdsmith): Get rid of this when we start creating the
  //                       DownloadFile on the UI thread.
  FilePath default_download_directory;

  // The request's |BoundNetLog|, for "source_dependency" linking with the
  // download item's.
  const net::BoundNetLog request_bound_net_log;
};

#endif  // CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_CREATE_INFO_H_
