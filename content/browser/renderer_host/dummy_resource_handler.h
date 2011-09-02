// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_DUMMY_RESOURCE_HANDLER_H_
#define CONTENT_BROWSER_RENDERER_HOST_DUMMY_RESOURCE_HANDLER_H_
#pragma once

#include <string>

#include "base/compiler_specific.h"
#include "content/browser/renderer_host/resource_handler.h"

namespace content {

class DummyResourceHandler : public ResourceHandler {
 public:
  DummyResourceHandler();

  virtual bool OnUploadProgress(int request_id, uint64 position, uint64 size)
      OVERRIDE;
  virtual bool OnRequestRedirected(int request_id,
                                   const GURL& url,
                                   ResourceResponse* response,
                                   bool* defer) OVERRIDE;
  virtual bool OnResponseStarted(int request_id, ResourceResponse* response)
      OVERRIDE;
  virtual bool OnWillStart(int request_id, const GURL& url, bool* defer)
      OVERRIDE;
  virtual bool OnWillRead(int request_id,
                          net::IOBuffer** buf,
                          int* buf_size,
                          int min_size) OVERRIDE;
  virtual bool OnReadCompleted(int request_id, int* bytes_read) OVERRIDE;
  virtual bool OnResponseCompleted(int request_id,
                                   const net::URLRequestStatus& status,
                                   const std::string& info) OVERRIDE;
  virtual void OnRequestClosed() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(DummyResourceHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_DUMMY_RESOURCE_HANDLER_H_
