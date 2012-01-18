// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_LAYERED_RESOURCE_HANDLER_H_
#define CONTENT_BROWSER_RENDERER_HOST_LAYERED_RESOURCE_HANDLER_H_
#pragma once

#include "content/browser/renderer_host/resource_handler.h"
#include "content/common/content_export.h"

namespace content {

// A ResourceHandler that simply delegates all calls to a next handler.  This
// class is intended to be subclassed.
class CONTENT_EXPORT LayeredResourceHandler : public ResourceHandler {
 public:
  LayeredResourceHandler(ResourceHandler* next_handler);

  // ResourceHandler implementation:
  virtual bool OnUploadProgress(int request_id, uint64 position,
                                uint64 size) OVERRIDE;
  virtual bool OnRequestRedirected(int request_id, const GURL& url,
                                   ResourceResponse* response,
                                   bool* defer) OVERRIDE;
  virtual bool OnResponseStarted(int request_id,
                                 ResourceResponse* response) OVERRIDE;
  virtual bool OnWillStart(int request_id, const GURL& url,
                           bool* defer) OVERRIDE;
  virtual bool OnWillRead(int request_id, net::IOBuffer** buf, int* buf_size,
                          int min_size) OVERRIDE;
  virtual bool OnReadCompleted(int request_id, int* bytes_read) OVERRIDE;
  virtual bool OnResponseCompleted(int request_id,
                                   const net::URLRequestStatus& status,
                                   const std::string& security_info) OVERRIDE;
  virtual void OnRequestClosed() OVERRIDE;
  virtual void OnDataDownloaded(int request_id, int bytes_downloaded) OVERRIDE;

 protected:
  virtual ~LayeredResourceHandler();

  scoped_refptr<ResourceHandler> next_handler_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_LAYERED_RESOURCE_HANDLER_H_
