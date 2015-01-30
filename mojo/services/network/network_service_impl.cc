// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/network/network_service_impl.h"

#include "mojo/services/network/cookie_store_impl.h"
#include "mojo/services/network/net_adapters.h"
#include "mojo/services/network/tcp_bound_socket_impl.h"
#include "mojo/services/network/udp_socket_impl.h"
#include "mojo/services/network/url_loader_impl.h"
#include "mojo/services/network/web_socket_impl.h"
#include "third_party/mojo/src/mojo/public/cpp/application/application_connection.h"

namespace mojo {

NetworkServiceImpl::NetworkServiceImpl(ApplicationConnection* connection,
                                       NetworkContext* context)
    : context_(context),
      origin_(GURL(connection->GetRemoteApplicationURL()).GetOrigin()) {
}

NetworkServiceImpl::~NetworkServiceImpl() {
}

void NetworkServiceImpl::CreateURLLoader(InterfaceRequest<URLLoader> loader) {
  // TODO(darin): Plumb origin_. Use for CORS.
  // The loader will delete itself when the pipe is closed, unless a request is
  // in progress. In which case, the loader will delete itself when the request
  // is finished.
  new URLLoaderImpl(context_, loader.Pass());
}

void NetworkServiceImpl::GetCookieStore(InterfaceRequest<CookieStore> store) {
  BindToRequest(new CookieStoreImpl(context_, origin_), &store);
}

void NetworkServiceImpl::CreateWebSocket(InterfaceRequest<WebSocket> socket) {
  BindToRequest(new WebSocketImpl(context_), &socket);
}

void NetworkServiceImpl::CreateTCPBoundSocket(
    NetAddressPtr local_address,
    InterfaceRequest<TCPBoundSocket> bound_socket,
    const Callback<void(NetworkErrorPtr, NetAddressPtr)>& callback) {
  scoped_ptr<TCPBoundSocketImpl> bound(new TCPBoundSocketImpl);
  int net_error = bound->Bind(local_address.Pass());
  if (net_error != net::OK) {
    callback.Run(MakeNetworkError(net_error), NetAddressPtr());
    return;
  }
  NetAddressPtr resulting_local_address(bound->GetLocalAddress());
  BindToRequest(bound.release(), &bound_socket);
  callback.Run(MakeNetworkError(net::OK), resulting_local_address.Pass());
}

void NetworkServiceImpl::CreateTCPConnectedSocket(
    NetAddressPtr remote_address,
    ScopedDataPipeConsumerHandle send_stream,
    ScopedDataPipeProducerHandle receive_stream,
    InterfaceRequest<TCPConnectedSocket> client_socket,
    const Callback<void(NetworkErrorPtr, NetAddressPtr)>& callback) {
  // TODO(brettw) implement this. We need to know what type of socket to use
  // so we can create the right one (i.e. to pass to TCPSocket::Open) before
  // doing the connect.
  callback.Run(MakeNetworkError(net::ERR_NOT_IMPLEMENTED), NetAddressPtr());
}

void NetworkServiceImpl::CreateUDPSocket(InterfaceRequest<UDPSocket> request) {
  // The lifetime of this UDPSocketImpl is bound to that of the underlying pipe.
  new UDPSocketImpl(request.Pass());
}

}  // namespace mojo
