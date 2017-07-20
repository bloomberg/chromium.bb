// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_NETWORK_URL_LOADER_IMPL_H_
#define CONTENT_NETWORK_URL_LOADER_IMPL_H_

#include <stdint.h>

#include <memory>

#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/public/common/url_loader.mojom.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_request.h"

namespace content {

class NetworkContext;
class NetToMojoPendingBuffer;
struct ResourceResponse;

class CONTENT_EXPORT URLLoaderImpl : public mojom::URLLoader,
                                     public net::URLRequest::Delegate {
 public:
  URLLoaderImpl(NetworkContext* context,
                mojom::URLLoaderAssociatedRequest url_loader_request,
                int32_t options,
                const ResourceRequest& request,
                mojom::URLLoaderClientPtr url_loader_client,
                const net::NetworkTrafficAnnotationTag& traffic_annotation);
  ~URLLoaderImpl() override;

  // Called when the associated NetworkContext is going away.
  void Cleanup();

  // mojom::URLLoader implementation:
  void FollowRedirect() override;
  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override;

  // net::URLRequest::Delegate methods:
  void OnReceivedRedirect(net::URLRequest* url_request,
                          const net::RedirectInfo& redirect_info,
                          bool* defer_redirect) override;
  void OnResponseStarted(net::URLRequest* url_request, int net_error) override;
  void OnReadCompleted(net::URLRequest* url_request, int bytes_read) override;

  // Returns a WeakPtr so tests can validate that the object was destroyed.
  base::WeakPtr<URLLoaderImpl> GetWeakPtrForTests();

 private:
  void ReadMore();
  void DidRead(uint32_t num_bytes, bool completed_synchronously);
  void NotifyCompleted(int error_code);
  void OnConnectionError();
  void OnResponseBodyStreamClosed(MojoResult result);
  void OnResponseBodyStreamReady(MojoResult result);
  void DeleteIfNeeded();
  void SendResponseToClient();

  NetworkContext* context_;
  int32_t options_;
  bool connected_;
  std::unique_ptr<net::URLRequest> url_request_;
  mojo::AssociatedBinding<mojom::URLLoader> binding_;
  mojom::URLLoaderClientPtr url_loader_client_;

  mojo::ScopedDataPipeProducerHandle response_body_stream_;
  scoped_refptr<NetToMojoPendingBuffer> pending_write_;
  mojo::SimpleWatcher writable_handle_watcher_;
  mojo::SimpleWatcher peer_closed_handle_watcher_;

  // Used when deferring sending the data to the client until mime sniffing is
  // finished.
  scoped_refptr<ResourceResponse> response_;
  mojo::ScopedDataPipeConsumerHandle consumer_handle_;

  base::WeakPtrFactory<URLLoaderImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(URLLoaderImpl);
};

}  // namespace content

#endif  // CONTENT_NETWORK_URL_LOADER_IMPL_H_
