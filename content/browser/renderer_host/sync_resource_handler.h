// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_SYNC_RESOURCE_HANDLER_H_
#define CONTENT_BROWSER_RENDERER_HOST_SYNC_RESOURCE_HANDLER_H_
#pragma once

#include <string>

#include "content/browser/renderer_host/resource_handler.h"
#include "content/public/common/resource_response.h"

namespace IPC {
class Message;
}

namespace net {
class IOBuffer;
}

namespace content {
class ResourceDispatcherHostImpl;
class ResourceMessageFilter;

// Used to complete a synchronous resource request in response to resource load
// events from the resource dispatcher host.
class SyncResourceHandler : public ResourceHandler {
 public:
  SyncResourceHandler(ResourceMessageFilter* filter,
                      const GURL& url,
                      IPC::Message* result_message,
                      ResourceDispatcherHostImpl* resource_dispatcher_host);
  virtual ~SyncResourceHandler();

  virtual bool OnUploadProgress(int request_id,
                                uint64 position,
                                uint64 size) OVERRIDE;
  virtual bool OnRequestRedirected(int request_id,
                                   const GURL& new_url,
                                   ResourceResponse* response,
                                   bool* defer) OVERRIDE;
  virtual bool OnResponseStarted(int request_id,
                                 ResourceResponse* response,
                                 bool* defer) OVERRIDE;
  virtual bool OnWillStart(int request_id,
                           const GURL& url,
                           bool* defer) OVERRIDE;
  virtual bool OnWillRead(int request_id,
                          net::IOBuffer** buf,
                          int* buf_size,
                          int min_size) OVERRIDE;
  virtual bool OnReadCompleted(int request_id,
                               int* bytes_read,
                               bool* defer) OVERRIDE;
  virtual bool OnResponseCompleted(int request_id,
                                   const net::URLRequestStatus& status,
                                   const std::string& security_info) OVERRIDE;

 private:
  enum { kReadBufSize = 3840 };

  scoped_refptr<net::IOBuffer> read_buffer_;

  SyncLoadResult result_;
  scoped_refptr<ResourceMessageFilter> filter_;
  IPC::Message* result_message_;
  ResourceDispatcherHostImpl* rdh_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_SYNC_RESOURCE_HANDLER_H_
