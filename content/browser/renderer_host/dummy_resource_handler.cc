// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/dummy_resource_handler.h"

namespace content {

DummyResourceHandler::DummyResourceHandler() {}

bool DummyResourceHandler::OnUploadProgress(int request_id,
                                            uint64 position,
                                            uint64 size) {
  return true;
}

bool DummyResourceHandler::OnRequestRedirected(int request_id,
                                               const GURL& url,
                                               ResourceResponse* response,
                                               bool* defer) {
  return true;
}

bool DummyResourceHandler::OnResponseStarted(int request_id,
                                             ResourceResponse* response) {
  return true;
}

bool DummyResourceHandler::OnWillStart(int request_id,
                                       const GURL& url,
                                       bool* defer) {
  return true;
}

bool DummyResourceHandler::OnWillRead(int request_id,
                                      net::IOBuffer** buf,
                                      int* buf_size,
                                      int min_size) {
  return true;
}

bool DummyResourceHandler::OnReadCompleted(int request_id, int* bytes_read) {
  return true;
}

bool DummyResourceHandler::OnResponseCompleted(
  int request_id,
  const net::URLRequestStatus& status,
  const std::string& info) {
  return true;
}

void DummyResourceHandler::OnRequestClosed() {}

}  // namespace content

