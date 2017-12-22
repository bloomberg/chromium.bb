// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_SYNC_RESOURCE_HANDLER_H_
#define CONTENT_BROWSER_LOADER_SYNC_RESOURCE_HANDLER_H_

#include <stdint.h>

#include <string>

#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/loader/resource_handler.h"
#include "content/public/common/resource_response.h"

namespace net {
class IOBuffer;
class URLRequest;
}

namespace content {
class ResourceController;

// Used to complete a synchronous resource request in response to resource load
// events from the resource dispatcher host.
class SyncResourceHandler : public ResourceHandler {
 public:
  using SyncLoadResultCallback =
      ResourceDispatcherHostImpl::SyncLoadResultCallback;

  SyncResourceHandler(net::URLRequest* request,
                      const SyncLoadResultCallback& sync_result_handler,
                      ResourceDispatcherHostImpl* resource_dispatcher_host);
  ~SyncResourceHandler() override;

  void OnRequestRedirected(
      const net::RedirectInfo& redirect_info,
      ResourceResponse* response,
      std::unique_ptr<ResourceController> controller) override;
  void OnResponseStarted(
      ResourceResponse* response,
      std::unique_ptr<ResourceController> controller) override;
  void OnWillStart(const GURL& url,
                   std::unique_ptr<ResourceController> controller) override;
  void OnWillRead(scoped_refptr<net::IOBuffer>* buf,
                  int* buf_size,
                  std::unique_ptr<ResourceController> controller) override;
  void OnReadCompleted(int bytes_read,
                       std::unique_ptr<ResourceController> controller) override;
  void OnResponseCompleted(
      const net::URLRequestStatus& status,
      std::unique_ptr<ResourceController> controller) override;
  void OnDataDownloaded(int bytes_downloaded) override;

 private:
  enum { kReadBufSize = 3840 };

  scoped_refptr<net::IOBuffer> read_buffer_;

  SyncLoadResult result_;
  SyncLoadResultCallback result_handler_;
  ResourceDispatcherHostImpl* rdh_;
  int64_t total_transfer_size_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_SYNC_RESOURCE_HANDLER_H_
