// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/layered_resource_handler.h"

#include "base/logging.h"

namespace content {

LayeredResourceHandler::LayeredResourceHandler(
    scoped_ptr<ResourceHandler> next_handler)
    : next_handler_(next_handler.Pass()) {
}

LayeredResourceHandler::~LayeredResourceHandler() {
}

void LayeredResourceHandler::SetController(ResourceController* controller) {
  ResourceHandler::SetController(controller);

  // Pass the controller down to the next handler.  This method is intended to
  // be overriden by subclasses of LayeredResourceHandler that need to insert a
  // different ResourceController.

  DCHECK(next_handler_.get());
  next_handler_->SetController(controller);
}

bool LayeredResourceHandler::OnUploadProgress(int request_id, uint64 position,
                                              uint64 size) {
  DCHECK(next_handler_.get());
  return next_handler_->OnUploadProgress(request_id, position, size);
}

bool LayeredResourceHandler::OnRequestRedirected(int request_id,
                                                 const GURL& url,
                                                 ResourceResponse* response,
                                                 bool* defer) {
  DCHECK(next_handler_.get());
  return next_handler_->OnRequestRedirected(request_id, url, response, defer);
}

bool LayeredResourceHandler::OnResponseStarted(int request_id,
                                               ResourceResponse* response,
                                               bool* defer) {
  DCHECK(next_handler_.get());
  return next_handler_->OnResponseStarted(request_id, response, defer);
}

bool LayeredResourceHandler::OnWillStart(int request_id, const GURL& url,
                                         bool* defer) {
  DCHECK(next_handler_.get());
  return next_handler_->OnWillStart(request_id, url, defer);
}

bool LayeredResourceHandler::OnWillRead(int request_id, net::IOBuffer** buf,
                                        int* buf_size, int min_size) {
  DCHECK(next_handler_.get());
  return next_handler_->OnWillRead(request_id, buf, buf_size, min_size);
}

bool LayeredResourceHandler::OnReadCompleted(int request_id, int bytes_read,
                                             bool* defer) {
  DCHECK(next_handler_.get());
  return next_handler_->OnReadCompleted(request_id, bytes_read, defer);
}

bool LayeredResourceHandler::OnResponseCompleted(
    int request_id,
    const net::URLRequestStatus& status,
    const std::string& security_info) {
  DCHECK(next_handler_.get());
  return next_handler_->OnResponseCompleted(request_id, status, security_info);
}

void LayeredResourceHandler::OnDataDownloaded(int request_id,
                                              int bytes_downloaded) {
  DCHECK(next_handler_.get());
  next_handler_->OnDataDownloaded(request_id, bytes_downloaded);
}

}  // namespace content
