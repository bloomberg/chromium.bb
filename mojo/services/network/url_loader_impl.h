// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_NETWORK_URL_LOADER_IMPL_H_
#define MOJO_SERVICES_NETWORK_URL_LOADER_IMPL_H_

#include <stdint.h>

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "mojo/message_pump/handle_watcher.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/services/network/public/interfaces/url_loader.mojom.h"
#include "mojo/shell/public/cpp/app_lifetime_helper.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request.h"

namespace mojo {

class NetworkContext;
class NetToMojoPendingBuffer;

class URLLoaderImpl : public URLLoader,
                      public net::URLRequest::Delegate {
 public:
  URLLoaderImpl(NetworkContext* context,
                InterfaceRequest<URLLoader> request,
                scoped_ptr<mojo::AppRefCount> app_refcount);
  ~URLLoaderImpl() override;

  // Called when the associated NetworkContext is going away.
  void Cleanup();

 private:
  // URLLoader methods:
  void Start(URLRequestPtr request,
             const Callback<void(URLResponsePtr)>& callback) override;
  void FollowRedirect(const Callback<void(URLResponsePtr)>& callback) override;
  void QueryStatus(const Callback<void(URLLoaderStatusPtr)>& callback) override;

  void OnConnectionError();

  // net::URLRequest::Delegate methods:
  void OnReceivedRedirect(net::URLRequest* url_request,
                          const net::RedirectInfo& redirect_info,
                          bool* defer_redirect) override;
  void OnResponseStarted(net::URLRequest* url_request) override;
  void OnReadCompleted(net::URLRequest* url_request, int bytes_read) override;

  void SendError(
      int error,
      const Callback<void(URLResponsePtr)>& callback);
  void SendResponse(URLResponsePtr response);
  void OnResponseBodyStreamReady(MojoResult result);
  void OnResponseBodyStreamClosed(MojoResult result);
  void ReadMore();
  void DidRead(uint32_t num_bytes, bool completed_synchronously);
  void ListenForPeerClosed();
  void DeleteIfNeeded();

  NetworkContext* context_;
  scoped_ptr<net::URLRequest> url_request_;
  Callback<void(URLResponsePtr)> callback_;
  ScopedDataPipeProducerHandle response_body_stream_;
  scoped_refptr<NetToMojoPendingBuffer> pending_write_;
  common::HandleWatcher handle_watcher_;
  uint32_t response_body_buffer_size_;
  uint32_t response_body_bytes_read_;
  bool auto_follow_redirects_;
  bool connected_;
  Binding<URLLoader> binding_;
  scoped_ptr<mojo::AppRefCount> app_refcount_;

  base::WeakPtrFactory<URLLoaderImpl> weak_ptr_factory_;
};

}  // namespace mojo

#endif  // MOJO_SERVICES_NETWORK_URL_LOADER_IMPL_H_
