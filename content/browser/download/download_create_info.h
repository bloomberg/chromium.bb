// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_CREATE_INFO_H_
#define CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_CREATE_INFO_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "content/browser/download/download_file.h"
#include "content/browser/download/download_request_handle.h"
#include "content/common/content_export.h"
#include "content/public/browser/download_interrupt_reasons.h"
#include "content/public/browser/download_save_info.h"
#include "net/http/http_response_info.h"
#include "net/log/net_log_with_source.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace net {
class HttpResponseHeaders;
}

namespace content {

// Used for informing the download manager of a new download, since we don't
// want to pass |DownloadItem|s between threads.
struct CONTENT_EXPORT DownloadCreateInfo {
  DownloadCreateInfo(const base::Time& start_time,
                     const net::NetLogWithSource& net_log,
                     std::unique_ptr<DownloadSaveInfo> save_info);
  DownloadCreateInfo();
  ~DownloadCreateInfo();

  // The URL from which we are downloading. This is the final URL after any
  // redirection by the server for |url_chain|.
  const GURL& url() const;

  // The ID of the download. (Deprecated)
  uint32_t download_id;

  // The unique identifier for the download.
  std::string guid;

  // The chain of redirects that leading up to and including the final URL.
  std::vector<GURL> url_chain;

  // The URL that referred us.
  GURL referrer_url;

  // Site URL for the site instance that initiated the download.
  GURL site_url;

  // The URL of the tab that started us.
  GURL tab_url;

  // The referrer URL of the tab that started us.
  GURL tab_referrer_url;

  // The time when the download started.
  base::Time start_time;

  // The size of the response body. If content-length response header is not
  // presented or can't be parse, set to 0.
  int64_t total_bytes;

  // The starting position of the initial request.
  // This value matches the offset in DownloadSaveInfo.
  // TODO(xingliu): Refactor to remove |offset| and |length|.
  int64_t offset;

  // True if the download was initiated by user action.
  bool has_user_gesture;

  // Whether the download should be transient. A transient download is
  // short-lived and is not shown in the UI.
  bool transient;

  base::Optional<ui::PageTransition> transition_type;

  // The HTTP response headers. This contains a nullptr when the response has
  // not yet been received. Only for consuming headers.
  scoped_refptr<const net::HttpResponseHeaders> response_headers;

  // The remote IP address where the download was fetched from.  Copied from
  // UrlRequest::GetSocketAddress().
  std::string remote_address;

  // If the download is initially created in an interrupted state (because the
  // response was in error), then |result| would be something other than
  // INTERRUPT_REASON_NONE.
  DownloadInterruptReason result;

  // The download file save info.
  std::unique_ptr<DownloadSaveInfo> save_info;

  // The handle to the URLRequest sourcing this download.
  std::unique_ptr<DownloadRequestHandleInterface> request_handle;

  // The request's |NetLogWithSource|, for "source_dependency" linking with the
  // download item's.
  const net::NetLogWithSource request_net_log;

  // ---------------------------------------------------------------------------
  // The remaining fields are Entity-body properties. These are only set if
  // |result| is DOWNLOAD_INTERRUPT_REASON_NONE.
  // ---------------------------------------------------------------------------

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

  // For continuing a download, the ETag of the file.
  std::string etag;

  // If "Accept-Ranges:bytes" header presents in the response header.
  bool accept_range;

  // The HTTP connection type.
  net::HttpResponseInfo::ConnectionInfo connection_info;

  // The HTTP request method.
  std::string method;

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadCreateInfo);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_CREATE_INFO_H_
