// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_LOADER_SYNC_LOAD_CONTEXT_H_
#define CONTENT_RENDERER_LOADER_SYNC_LOAD_CONTEXT_H_

#include "base/macros.h"
#include "base/optional.h"
#include "base/synchronization/waitable_event_watcher.h"
#include "base/timer/timer.h"
#include "content/public/common/url_loader_factory.mojom.h"
#include "content/public/renderer/request_peer.h"
#include "content/renderer/loader/resource_dispatcher.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace base {
class WaitableEvent;
}

namespace url {
class Origin;
}

namespace content {

struct ResourceRequest;
struct SyncLoadResponse;

// This class owns the context necessary to perform an asynchronous request
// while the main thread is blocked so that it appears to be synchronous.
class SyncLoadContext : public RequestPeer {
 public:
  // Begins a new asynchronous request on whatever sequence this method is
  // called on. |completed_event| will be signalled when the request is complete
  // and |response| will be populated with the response data. |abort_event|
  // will be signalled from the main thread to abort the sync request on a
  // worker thread when the worker thread is being terminated.
  static void StartAsyncWithWaitableEvent(
      std::unique_ptr<ResourceRequest> request,
      int routing_id,
      const url::Origin& frame_origin,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      mojom::URLLoaderFactoryPtrInfo url_loader_factory_pipe,
      std::vector<std::unique_ptr<URLLoaderThrottle>> throttles,
      SyncLoadResponse* response,
      base::WaitableEvent* completed_event,
      base::WaitableEvent* abort_event,
      double timeout);

  ~SyncLoadContext() override;

 private:
  SyncLoadContext(ResourceRequest* request,
                  mojom::URLLoaderFactoryPtrInfo url_loader_factory,
                  SyncLoadResponse* response,
                  base::WaitableEvent* completed_event,
                  base::WaitableEvent* abort_event,
                  double timeout);
  // RequestPeer implementation:
  void OnUploadProgress(uint64_t position, uint64_t size) override;
  bool OnReceivedRedirect(const net::RedirectInfo& redirect_info,
                          const ResourceResponseInfo& info) override;
  void OnReceivedResponse(const ResourceResponseInfo& info) override;
  void OnDownloadedData(int len, int encoded_data_length) override;
  void OnReceivedData(std::unique_ptr<ReceivedData> data) override;
  void OnTransferSizeUpdated(int transfer_size_diff) override;
  void OnCompletedRequest(
      const network::URLLoaderCompletionStatus& status) override;

  void OnAbort(base::WaitableEvent* event);
  void OnTimeout();

  void CompleteRequest(bool remove_pending_request);
  bool Completed() const;

  // This raw pointer will remain valid for the lifetime of this object because
  // it remains on the stack until |event_| is signaled.
  // Set to null after CompleteRequest() is called.
  SyncLoadResponse* response_;

  // This event is signaled when the request is complete.
  // Set to null after CompleteRequest() is called.
  base::WaitableEvent* completed_event_;

  // State necessary to run a request on an independent thread.
  mojom::URLLoaderFactoryPtr url_loader_factory_;
  std::unique_ptr<ResourceDispatcher> resource_dispatcher_;

  int request_id_;

  base::Optional<int64_t> downloaded_file_length_;

  base::WaitableEventWatcher abort_watcher_;
  base::OneShotTimer timeout_timer_;

  DISALLOW_COPY_AND_ASSIGN(SyncLoadContext);
};

}  // namespace content

#endif  // CONTENT_RENDERER_LOADER_SYNC_LOAD_CONTEXT_H_
