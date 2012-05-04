// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_RESOURCE_HANDLER_H_
#define CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_RESOURCE_HANDLER_H_
#pragma once

#include <string>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/timer.h"
#include "content/browser/renderer_host/resource_handler.h"
#include "content/public/browser/download_id.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/download_save_info.h"
#include "content/public/browser/download_url_parameters.h"
#include "content/public/browser/global_request_id.h"
#include "net/base/net_errors.h"

class DownloadFileManager;
class DownloadRequestHandle;
struct DownloadCreateInfo;

namespace content {
class DownloadBuffer;
}

namespace net {
class URLRequest;
}  // namespace net

// Forwards data to the download thread.
class DownloadResourceHandler : public ResourceHandler {
 public:
  typedef content::DownloadUrlParameters::OnStartedCallback OnStartedCallback;

  static const size_t kLoadsToWrite = 100;  // number of data buffers queued

  // started_cb will be called exactly once on the UI thread.
  DownloadResourceHandler(int render_process_host_id,
                          int render_view_id,
                          int request_id,
                          const GURL& url,
                          DownloadFileManager* download_file_manager,
                          net::URLRequest* request,
                          const OnStartedCallback& started_cb,
                          const content::DownloadSaveInfo& save_info);

  virtual bool OnUploadProgress(int request_id,
                                uint64 position,
                                uint64 size) OVERRIDE;

  // Not needed, as this event handler ought to be the final resource.
  virtual bool OnRequestRedirected(int request_id,
                                   const GURL& url,
                                   content::ResourceResponse* response,
                                   bool* defer) OVERRIDE;

  // Send the download creation information to the download thread.
  virtual bool OnResponseStarted(int request_id,
                                 content::ResourceResponse* response) OVERRIDE;

  // Pass-through implementation.
  virtual bool OnWillStart(int request_id,
                           const GURL& url,
                           bool* defer) OVERRIDE;

  // Create a new buffer, which will be handed to the download thread for file
  // writing and deletion.
  virtual bool OnWillRead(int request_id,
                          net::IOBuffer** buf,
                          int* buf_size,
                          int min_size) OVERRIDE;

  virtual bool OnReadCompleted(int request_id, int* bytes_read) OVERRIDE;

  virtual bool OnResponseCompleted(int request_id,
                                   const net::URLRequestStatus& status,
                                   const std::string& security_info) OVERRIDE;
  virtual void OnRequestClosed() OVERRIDE;

  // If the content-length header is not present (or contains something other
  // than numbers), the incoming content_length is -1 (unknown size).
  // Set the content length to 0 to indicate unknown size to DownloadManager.
  void set_content_length(const int64& content_length);

  void set_content_disposition(const std::string& content_disposition);

  void CheckWriteProgress();

  std::string DebugString() const;

 private:
  virtual ~DownloadResourceHandler();

  void OnResponseCompletedInternal(int request_id,
                                   const net::URLRequestStatus& status,
                                   const std::string& security_info,
                                   int response_code);

  void StartPauseTimer();
  void CallStartedCB(content::DownloadId id, net::Error error);

  // Generates a DownloadId and calls DownloadFileManager.
  void StartOnUIThread(scoped_ptr<DownloadCreateInfo> info,
                       const DownloadRequestHandle& handle);
  void set_download_id(content::DownloadId id);

  content::DownloadId download_id_;
  content::GlobalRequestID global_id_;
  int render_view_id_;
  scoped_refptr<net::IOBuffer> read_buffer_;
  std::string content_disposition_;
  int64 content_length_;
  DownloadFileManager* download_file_manager_;
  net::URLRequest* request_;
  // This is used only on the UI thread.
  OnStartedCallback started_cb_;
  content::DownloadSaveInfo save_info_;
  scoped_refptr<content::DownloadBuffer> buffer_;
  bool is_paused_;
  base::OneShotTimer<DownloadResourceHandler> pause_timer_;

  // The following are used to collect stats.
  base::TimeTicks download_start_time_;
  base::TimeTicks last_read_time_;
  size_t last_buffer_size_;
  int64 bytes_read_;
  std::string accept_ranges_;

  static const int kReadBufSize = 32768;  // bytes
  static const int kThrottleTimeMs = 200;  // milliseconds

  DISALLOW_COPY_AND_ASSIGN(DownloadResourceHandler);
};

#endif  // CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_RESOURCE_HANDLER_H_
