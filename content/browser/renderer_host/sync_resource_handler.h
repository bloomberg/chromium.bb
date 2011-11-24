// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_SYNC_RESOURCE_HANDLER_H_
#define CONTENT_BROWSER_RENDERER_HOST_SYNC_RESOURCE_HANDLER_H_
#pragma once

#include <string>

#include "content/browser/renderer_host/resource_handler.h"
#include "content/public/common/resource_response.h"

class ResourceDispatcherHost;
class ResourceMessageFilter;

namespace IPC {
class Message;
}

namespace net {
class IOBuffer;
}

// Used to complete a synchronous resource request in response to resource load
// events from the resource dispatcher host.
class SyncResourceHandler : public ResourceHandler {
 public:
  SyncResourceHandler(ResourceMessageFilter* filter,
                      const GURL& url,
                      IPC::Message* result_message,
                      ResourceDispatcherHost* resource_dispatcher_host);

  virtual bool OnUploadProgress(int request_id,
                                uint64 position,
                                uint64 size) OVERRIDE;
  virtual bool OnRequestRedirected(int request_id,
                                   const GURL& new_url,
                                   content::ResourceResponse* response,
                                   bool* defer) OVERRIDE;
  virtual bool OnResponseStarted(int request_id,
                                 content::ResourceResponse* response) OVERRIDE;
  virtual bool OnWillStart(int request_id,
                           const GURL& url,
                           bool* defer) OVERRIDE;
  virtual bool OnWillRead(int request_id,
                          net::IOBuffer** buf,
                          int* buf_size,
                          int min_size) OVERRIDE;
  virtual bool OnReadCompleted(int request_id,
                               int* bytes_read) OVERRIDE;
  virtual bool OnResponseCompleted(int request_id,
                                   const net::URLRequestStatus& status,
                                   const std::string& security_info) OVERRIDE;
  virtual void OnRequestClosed() OVERRIDE;

 private:
  enum { kReadBufSize = 3840 };

  virtual ~SyncResourceHandler();

  scoped_refptr<net::IOBuffer> read_buffer_;

  content::SyncLoadResult result_;
  ResourceMessageFilter* filter_;
  IPC::Message* result_message_;
  ResourceDispatcherHost* rdh_;
};

#endif  // CONTENT_BROWSER_RENDERER_HOST_SYNC_RESOURCE_HANDLER_H_
