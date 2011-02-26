// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_DOWNLOAD_RESOURCE_HANDLER_H_
#define CHROME_BROWSER_RENDERER_HOST_DOWNLOAD_RESOURCE_HANDLER_H_
#pragma once

#include <string>

#include "base/timer.h"
#include "chrome/browser/download/download_file.h"
#include "chrome/browser/renderer_host/global_request_id.h"
#include "chrome/browser/renderer_host/resource_handler.h"

class DownloadFileManager;
class ResourceDispatcherHost;
struct DownloadBuffer;

namespace net {
class URLRequest;
}  // namespace net

// Forwards data to the download thread.
class DownloadResourceHandler : public ResourceHandler {
 public:
  DownloadResourceHandler(ResourceDispatcherHost* rdh,
                          int render_process_host_id,
                          int render_view_id,
                          int request_id,
                          const GURL& url,
                          DownloadFileManager* download_file_manager,
                          net::URLRequest* request,
                          bool save_as,
                          const DownloadSaveInfo& save_info);

  virtual bool OnUploadProgress(int request_id, uint64 position, uint64 size);

  // Not needed, as this event handler ought to be the final resource.
  virtual bool OnRequestRedirected(int request_id, const GURL& url,
                                   ResourceResponse* response, bool* defer);

  // Send the download creation information to the download thread.
  virtual bool OnResponseStarted(int request_id, ResourceResponse* response);

  // Pass-through implementation.
  virtual bool OnWillStart(int request_id, const GURL& url, bool* defer);

  // Create a new buffer, which will be handed to the download thread for file
  // writing and deletion.
  virtual bool OnWillRead(int request_id, net::IOBuffer** buf, int* buf_size,
                          int min_size);

  virtual bool OnReadCompleted(int request_id, int* bytes_read);

  virtual bool OnResponseCompleted(int request_id,
                                   const net::URLRequestStatus& status,
                                   const std::string& security_info);
  virtual void OnRequestClosed();

  // If the content-length header is not present (or contains something other
  // than numbers), the incoming content_length is -1 (unknown size).
  // Set the content length to 0 to indicate unknown size to DownloadManager.
  void set_content_length(const int64& content_length);

  void set_content_disposition(const std::string& content_disposition);

  void CheckWriteProgress();

  std::string DebugString() const;

 private:
  ~DownloadResourceHandler();

  void StartPauseTimer();

  int download_id_;
  GlobalRequestID global_id_;
  int render_view_id_;
  scoped_refptr<net::IOBuffer> read_buffer_;
  std::string content_disposition_;
  GURL url_; // final URL from which we're downloading.
  GURL original_url_; // original URL before any redirection by the server.
  int64 content_length_;
  DownloadFileManager* download_file_manager_;
  net::URLRequest* request_;
  bool save_as_;  // Request was initiated via "Save As" by the user.
  DownloadSaveInfo save_info_;
  DownloadBuffer* buffer_;
  ResourceDispatcherHost* rdh_;
  bool is_paused_;
  base::OneShotTimer<DownloadResourceHandler> pause_timer_;
  base::TimeTicks download_start_time_;  // used to collect stats.
  static const int kReadBufSize = 32768;  // bytes
  static const size_t kLoadsToWrite = 100;  // number of data buffers queued
  static const int kThrottleTimeMs = 200;  // milliseconds

  DISALLOW_COPY_AND_ASSIGN(DownloadResourceHandler);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_DOWNLOAD_RESOURCE_HANDLER_H_
