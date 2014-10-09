// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/network/network_service_impl.h"

#include "mojo/public/cpp/application/application_connection.h"
#include "mojo/services/network/cookie_store_impl.h"
#include "mojo/services/network/net_adapters.h"
#include "mojo/services/network/tcp_bound_socket_impl.h"
#include "mojo/services/network/url_loader_impl.h"
#include "mojo/services/network/web_socket_impl.h"

namespace mojo {

namespace {

// Allows a Callback<NetworkErrorPtr, NetAddressPtr> to be called by a
// Callback<NetworkErrorPtr> when the address is already known.
void BoundAddressCallbackAdapter(
    const Callback<void(NetworkErrorPtr, NetAddressPtr)>& callback,
    NetAddressPtr address,
    NetworkErrorPtr err) {
  callback.Run(err.Pass(), address.Pass());
}

}  // namespace

NetworkServiceImpl::NetworkServiceImpl(ApplicationConnection* connection,
                                       NetworkContext* context)
    : context_(context),
      origin_(GURL(connection->GetRemoteApplicationURL()).GetOrigin()) {
}

NetworkServiceImpl::~NetworkServiceImpl() {
}

void NetworkServiceImpl::CreateURLLoader(InterfaceRequest<URLLoader> loader) {
  // TODO(darin): Plumb origin_. Use for CORS.
  BindToRequest(new URLLoaderImpl(context_), &loader);
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
  TCPBoundSocketImpl bound_socket;
  int net_error = bound_socket.Bind(NetAddressPtr());
  if (net_error != net::OK) {
    callback.Run(MakeNetworkError(net_error), NetAddressPtr());
    return;
  }

  bound_socket.Connect(
      remote_address.Pass(), send_stream.Pass(), receive_stream.Pass(),
      client_socket.Pass(),
      base::Bind(&BoundAddressCallbackAdapter,
                 callback,
                 base::Passed(bound_socket.GetLocalAddress().Pass())));
}

}  // namespace mojo
