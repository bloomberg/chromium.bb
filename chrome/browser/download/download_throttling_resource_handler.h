// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_THROTTLING_RESOURCE_HANDLER_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_THROTTLING_RESOURCE_HANDLER_H_
#pragma once

#include <string>

#include "chrome/browser/download/download_request_limiter.h"
#include "content/browser/renderer_host/resource_handler.h"
#include "googleurl/src/gurl.h"

class ResourceDispatcherHost;

namespace net {
class URLRequest;
}  // namespace net

// DownloadThrottlingResourceHandler is used to determine if a download should
// be allowed. When a DownloadThrottlingResourceHandler is created it pauses the
// download and asks the DownloadRequestLimiter if the download should be
// allowed. The DownloadRequestLimiter notifies us asynchronously as to whether
// the download is allowed or not. If the download is allowed the request is
// resumed, all EventHandler methods are delegated to it to the original
// handler. If the download is not allowed the request is canceled.

class DownloadThrottlingResourceHandler
    : public ResourceHandler,
      public DownloadRequestLimiter::Callback {
 public:
  DownloadThrottlingResourceHandler(ResourceHandler* next_handler,
                                    ResourceDispatcherHost* host,
                                    DownloadRequestLimiter* limiter,
                                    net::URLRequest* request,
                                    const GURL& url,
                                    int render_process_host_id,
                                    int render_view_id,
                                    int request_id,
                                    bool in_complete);

  // ResourceHanlder implementation:
  virtual bool OnUploadProgress(int request_id,
                                uint64 position,
                                uint64 size) OVERRIDE;
  virtual bool OnRequestRedirected(int request_id,
                                   const GURL& url,
                                   ResourceResponse* response,
                                   bool* defer) OVERRIDE;
  virtual bool OnResponseStarted(int request_id,
                                 ResourceResponse* response) OVERRIDE;
  virtual bool OnWillStart(int request_id,
                           const GURL& url,
                           bool* defer) OVERRIDE;
  virtual bool OnWillRead(int request_id,
                          net::IOBuffer** buf,
                          int* buf_size,
                          int min_size) OVERRIDE;
  virtual bool OnReadCompleted(int request_id, int* bytes_read) OVERRIDE;
  virtual bool OnResponseCompleted(int request_id,
                                   const net::URLRequestStatus& status,
                                   const std::string& security_info) OVERRIDE;
  virtual void OnRequestClosed() OVERRIDE;

  // DownloadRequestLimiter::Callback implementation:
  virtual void CancelDownload() OVERRIDE;
  virtual void ContinueDownload() OVERRIDE;

 private:
  virtual ~DownloadThrottlingResourceHandler();

  void CopyTmpBufferToDownloadHandler();

  ResourceDispatcherHost* host_;
  net::URLRequest* request_;
  GURL url_;
  int render_process_host_id_;
  int render_view_id_;
  int request_id_;

  // The original handler.
  scoped_refptr<ResourceHandler> next_handler_;

  // Set to true when we know that the request is allowed to start.
  bool request_allowed_;

  // Response supplied to OnResponseStarted. Only non-null if OnResponseStarted
  // is invoked.
  scoped_refptr<ResourceResponse> response_;

  // If we're created by way of BufferedEventHandler we'll get one request for
  // a buffer. This is that buffer.
  scoped_refptr<net::IOBuffer> tmp_buffer_;
  int tmp_buffer_length_;

  // Have we received OnRequestClosed()?  If so, we shouldn't act on
  // CancelDownload()/ContinueDownload().
  bool request_closed_;

  DISALLOW_COPY_AND_ASSIGN(DownloadThrottlingResourceHandler);
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_THROTTLING_RESOURCE_HANDLER_H_
