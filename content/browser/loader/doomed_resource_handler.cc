// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/doomed_resource_handler.h"

#include "base/logging.h"
#include "net/url_request/url_request_status.h"

namespace content {

DoomedResourceHandler::DoomedResourceHandler(
    scoped_ptr<ResourceHandler> old_handler)
    : old_handler_(old_handler.Pass()) {
}

DoomedResourceHandler::~DoomedResourceHandler() {
}

bool DoomedResourceHandler::OnUploadProgress(int request_id,
                                             uint64 position,
                                             uint64 size) {
  NOTREACHED();
  return true;
}

bool DoomedResourceHandler::OnRequestRedirected(int request_id,
                                                const GURL& new_url,
                                                ResourceResponse* response,
                                                bool* defer) {
  NOTREACHED();
  return true;
}

bool DoomedResourceHandler::OnResponseStarted(int request_id,
                                              ResourceResponse* response,
                                              bool* defer) {
  NOTREACHED();
  return true;
}

bool DoomedResourceHandler::OnWillStart(int request_id,
                                        const GURL& url,
                                        bool* defer) {
  NOTREACHED();
  return true;
}

bool DoomedResourceHandler::OnWillRead(int request_id,
                                       net::IOBuffer** buf,
                                       int* buf_size,
                                       int min_size) {
  NOTREACHED();
  return true;
}

bool DoomedResourceHandler::OnReadCompleted(int request_id,
                                            int bytes_read,
                                            bool* defer) {
  NOTREACHED();
  return true;
}

bool DoomedResourceHandler::OnResponseCompleted(
    int request_id,
    const net::URLRequestStatus& status,
    const std::string& security_info) {
  DCHECK(status.status() == net::URLRequestStatus::CANCELED ||
         status.status() == net::URLRequestStatus::FAILED);
  return true;
}

void DoomedResourceHandler::OnDataDownloaded(int request_id,
                                             int bytes_downloaded) {
  NOTREACHED();
}

}  // namespace content
