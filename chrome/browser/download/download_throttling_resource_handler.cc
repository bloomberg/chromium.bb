// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_throttling_resource_handler.h"

#include "base/logging.h"
#include "content/browser/download/download_id.h"
#include "content/browser/download/download_resource_handler.h"
#include "content/browser/download/download_stats.h"
#include "content/browser/renderer_host/resource_dispatcher_host.h"
#include "content/browser/renderer_host/resource_dispatcher_host_request_info.h"
#include "content/browser/resource_context.h"
#include "content/public/common/resource_response.h"
#include "net/base/io_buffer.h"
#include "net/base/mime_sniffer.h"

DownloadThrottlingResourceHandler::DownloadThrottlingResourceHandler(
    ResourceHandler* next_handler,
    ResourceDispatcherHost* host,
    DownloadRequestLimiter* limiter,
    net::URLRequest* request,
    const GURL& url,
    int render_process_host_id,
    int render_view_id,
    int request_id,
    bool in_complete)
    : host_(host),
      request_(request),
      url_(url),
      render_process_host_id_(render_process_host_id),
      render_view_id_(render_view_id),
      request_id_(request_id),
      next_handler_(next_handler),
      request_allowed_(false),
      tmp_buffer_length_(0),
      request_closed_(false) {
  download_stats::RecordDownloadCount(
      download_stats::INITIATED_BY_NAVIGATION_COUNT);

  // Pause the request.
  host_->PauseRequest(render_process_host_id_, request_id_, true);

  // Add a reference to ourselves to keep this object alive until we
  // receive a callback from DownloadRequestLimiter. The reference is
  // released in ContinueDownload() and CancelDownload().
  AddRef();

  limiter->CanDownloadOnIOThread(
      render_process_host_id_, render_view_id, request_id, this);
}

DownloadThrottlingResourceHandler::~DownloadThrottlingResourceHandler() {
}

bool DownloadThrottlingResourceHandler::OnUploadProgress(int request_id,
                                                         uint64 position,
                                                         uint64 size) {
  DCHECK(!request_closed_);
  if (request_allowed_)
    return next_handler_->OnUploadProgress(request_id, position, size);
  return true;
}

bool DownloadThrottlingResourceHandler::OnRequestRedirected(
    int request_id,
    const GURL& url,
    content::ResourceResponse* response,
    bool* defer) {
  DCHECK(!request_closed_);
  if (request_allowed_) {
    return next_handler_->OnRequestRedirected(request_id, url, response, defer);
  }
  url_ = url;
  return true;
}

bool DownloadThrottlingResourceHandler::OnResponseStarted(
    int request_id,
    content::ResourceResponse* response) {
  DCHECK(!request_closed_);
  if (request_allowed_)
    return next_handler_->OnResponseStarted(request_id, response);
  response_ = response;
  return true;
}

bool DownloadThrottlingResourceHandler::OnWillStart(int request_id,
                                                    const GURL& url,
                                                    bool* defer) {
  DCHECK(!request_closed_);
  if (request_allowed_)
    return next_handler_->OnWillStart(request_id, url, defer);
  return true;
}

bool DownloadThrottlingResourceHandler::OnWillRead(int request_id,
                                                   net::IOBuffer** buf,
                                                   int* buf_size,
                                                   int min_size) {
  DCHECK(!request_closed_);
  if (request_allowed_)
    return next_handler_->OnWillRead(request_id, buf, buf_size, min_size);

  // We should only have this invoked once, as such we only deal with one
  // tmp buffer.
  DCHECK(!tmp_buffer_.get());
  // If the caller passed a negative |min_size| then chose an appropriate
  // default. The BufferedResourceHandler requires this to be at least 2 times
  // the size required for mime detection.
  if (min_size < 0)
    min_size = 2 * net::kMaxBytesToSniff;
  tmp_buffer_ = new net::IOBuffer(min_size);
  *buf = tmp_buffer_.get();
  *buf_size = min_size;
  return true;
}

bool DownloadThrottlingResourceHandler::OnReadCompleted(int request_id,
                                                        int* bytes_read) {
  DCHECK(!request_closed_);
  if (!*bytes_read)
    return true;

  if (tmp_buffer_.get()) {
    DCHECK(!tmp_buffer_length_);
    tmp_buffer_length_ = *bytes_read;
    if (request_allowed_)
      CopyTmpBufferToDownloadHandler();
    return true;
  }
  if (request_allowed_)
    return next_handler_->OnReadCompleted(request_id, bytes_read);
  return true;
}

bool DownloadThrottlingResourceHandler::OnResponseCompleted(
    int request_id,
    const net::URLRequestStatus& status,
    const std::string& security_info) {
  DCHECK(!request_closed_);
  if (request_allowed_)
    return next_handler_->OnResponseCompleted(request_id, status,
                                              security_info);

  // For a download, if ResourceDispatcher::Read() fails,
  // ResourceDispatcher::OnresponseStarted() will call
  // OnResponseCompleted(), and we will end up here with an error
  // status.
  if (!status.is_success())
    return false;
  NOTREACHED();
  return true;
}

void DownloadThrottlingResourceHandler::OnRequestClosed() {
  DCHECK(!request_closed_);
  if (request_allowed_)
    next_handler_->OnRequestClosed();
  request_closed_ = true;
}

void DownloadThrottlingResourceHandler::CancelDownload() {
  if (!request_closed_)
    host_->CancelRequest(render_process_host_id_, request_id_, false);
  Release();  // Release the additional reference from constructor.
}

void DownloadThrottlingResourceHandler::ContinueDownload() {
  if (!request_closed_) {
    request_allowed_ = true;
    if (response_.get())
      next_handler_->OnResponseStarted(request_id_, response_.get());

    if (tmp_buffer_length_)
      CopyTmpBufferToDownloadHandler();

    // And let the request continue.
    host_->PauseRequest(render_process_host_id_, request_id_, false);
  }
  Release();  // Release the addtional reference from constructor.
}

void DownloadThrottlingResourceHandler::CopyTmpBufferToDownloadHandler() {
  // Copy over the tmp buffer.
  net::IOBuffer* buffer;
  int buf_size;
  if (next_handler_->OnWillRead(request_id_, &buffer, &buf_size,
                                tmp_buffer_length_)) {
    CHECK(buf_size >= tmp_buffer_length_);
    memcpy(buffer->data(), tmp_buffer_->data(), tmp_buffer_length_);
    next_handler_->OnReadCompleted(request_id_, &tmp_buffer_length_);
  }
  tmp_buffer_length_ = 0;
  tmp_buffer_ = NULL;
}
