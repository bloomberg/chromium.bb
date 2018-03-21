// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_LOADER_SYNC_LOAD_CONTEXT_H_
#define CONTENT_RENDERER_LOADER_SYNC_LOAD_CONTEXT_H_

#include "base/macros.h"
#include "content/public/renderer/request_peer.h"
#include "content/renderer/loader/resource_dispatcher.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace base {
class WaitableEvent;
}

namespace network {
struct ResourceRequest;
}

namespace content {

struct SyncLoadResponse;

// This class owns the context necessary to perform an asynchronous request
// while the main thread is blocked so that it appears to be synchronous.
class SyncLoadContext : public RequestPeer {
 public:
  // Begins a new asynchronous request on whatever sequence this method is
  // called on. |event| will be signalled when the request is complete and
  // |response| will be populated with the response data.
  static void StartAsyncWithWaitableEvent(
      std::unique_ptr<network::ResourceRequest> request,
      int routing_id,
      scoped_refptr<base::SingleThreadTaskRunner> loading_task_runner,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      std::unique_ptr<network::SharedURLLoaderFactoryInfo>
          url_loader_factory_info,
      std::vector<std::unique_ptr<URLLoaderThrottle>> throttles,
      SyncLoadResponse* response,
      base::WaitableEvent* event);

  SyncLoadContext(
      network::ResourceRequest* request,
      std::unique_ptr<network::SharedURLLoaderFactoryInfo> url_loader_factory,
      SyncLoadResponse* response,
      base::WaitableEvent* event,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  ~SyncLoadContext() override;

 private:
  // RequestPeer implementation:
  void OnUploadProgress(uint64_t position, uint64_t size) override;
  bool OnReceivedRedirect(const net::RedirectInfo& redirect_info,
                          const network::ResourceResponseInfo& info) override;
  void OnReceivedResponse(const network::ResourceResponseInfo& info) override;
  void OnDownloadedData(int len, int encoded_data_length) override;
  void OnReceivedData(std::unique_ptr<ReceivedData> data) override;
  void OnTransferSizeUpdated(int transfer_size_diff) override;
  void OnCompletedRequest(
      const network::URLLoaderCompletionStatus& status) override;

  // This raw pointer will remain valid for the lifetime of this object because
  // it remains on the stack until |event_| is signaled.
  SyncLoadResponse* response_;

  // This event is signaled when the request is complete.
  base::WaitableEvent* event_;

  // State necessary to run a request on an independent thread.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<ResourceDispatcher> resource_dispatcher_;

  int request_id_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(SyncLoadContext);
};

}  // namespace content

#endif  // CONTENT_RENDERER_LOADER_SYNC_LOAD_CONTEXT_H_
