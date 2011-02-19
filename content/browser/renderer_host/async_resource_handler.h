// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_ASYNC_RESOURCE_HANDLER_H_
#define CONTENT_BROWSER_RENDERER_HOST_ASYNC_RESOURCE_HANDLER_H_
#pragma once

#include <string>

#include "chrome/browser/renderer_host/resource_handler.h"

class ResourceDispatcherHost;
class ResourceMessageFilter;
class SharedIOBuffer;

// Used to complete an asynchronous resource request in response to resource
// load events from the resource dispatcher host.
class AsyncResourceHandler : public ResourceHandler {
 public:
  AsyncResourceHandler(ResourceMessageFilter* filter,
                       int routing_id,
                       const GURL& url,
                       ResourceDispatcherHost* resource_dispatcher_host);

  // ResourceHandler implementation:
  virtual bool OnUploadProgress(int request_id, uint64 position, uint64 size);
  virtual bool OnRequestRedirected(int request_id, const GURL& new_url,
                                   ResourceResponse* response, bool* defer);
  virtual bool OnResponseStarted(int request_id, ResourceResponse* response);
  virtual bool OnWillStart(int request_id, const GURL& url, bool* defer);
  virtual bool OnWillRead(int request_id, net::IOBuffer** buf, int* buf_size,
                          int min_size);
  virtual bool OnReadCompleted(int request_id, int* bytes_read);
  virtual bool OnResponseCompleted(int request_id,
                                   const net::URLRequestStatus& status,
                                   const std::string& security_info);
  virtual void OnRequestClosed();
  virtual void OnDataDownloaded(int request_id, int bytes_downloaded);

  static void GlobalCleanup();

 private:
  virtual ~AsyncResourceHandler();

  scoped_refptr<SharedIOBuffer> read_buffer_;
  ResourceMessageFilter* filter_;
  int routing_id_;
  ResourceDispatcherHost* rdh_;

  // |next_buffer_size_| is the size of the buffer to be allocated on the next
  // OnWillRead() call.  We exponentially grow the size of the buffer allocated
  // when our owner fills our buffers. On the first OnWillRead() call, we
  // allocate a buffer of 32k and double it in OnReadCompleted() if the buffer
  // was filled, up to a maximum size of 512k.
  int next_buffer_size_;

  DISALLOW_COPY_AND_ASSIGN(AsyncResourceHandler);
};

#endif  // CONTENT_BROWSER_RENDERER_HOST_ASYNC_RESOURCE_HANDLER_H_
