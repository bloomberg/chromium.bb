// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/network/network_service_impl.h"

#include <utility>

#include "mojo/services/network/http_server_impl.h"
#include "mojo/services/network/net_adapters.h"
#include "mojo/services/network/tcp_bound_socket_impl.h"
#include "mojo/services/network/udp_socket_impl.h"
#include "mojo/services/network/url_loader_impl.h"
#include "net/base/mime_util.h"

namespace mojo {

NetworkServiceImpl::NetworkServiceImpl(
    scoped_ptr<mojo::AppRefCount> app_refcount,
    InterfaceRequest<NetworkService> request)
    : app_refcount_(std::move(app_refcount)),
      binding_(this, std::move(request)) {}

NetworkServiceImpl::~NetworkServiceImpl() {
}

void NetworkServiceImpl::CreateTCPBoundSocket(
    NetAddressPtr local_address,
    InterfaceRequest<TCPBoundSocket> bound_socket,
    const CreateTCPBoundSocketCallback& callback) {
  scoped_ptr<TCPBoundSocketImpl> bound(
      new TCPBoundSocketImpl(app_refcount_->Clone(), std::move(bound_socket)));
  int net_error = bound->Bind(std::move(local_address));
  if (net_error != net::OK) {
    callback.Run(MakeNetworkError(net_error), NetAddressPtr());
    return;
  }
  ignore_result(bound.release());  // Strongly owned by the message pipe.
  NetAddressPtr resulting_local_address(bound->GetLocalAddress());
  callback.Run(MakeNetworkError(net::OK), std::move(resulting_local_address));
}

void NetworkServiceImpl::CreateTCPConnectedSocket(
    NetAddressPtr remote_address,
    ScopedDataPipeConsumerHandle send_stream,
    ScopedDataPipeProducerHandle receive_stream,
    InterfaceRequest<TCPConnectedSocket> client_socket,
    const CreateTCPConnectedSocketCallback& callback) {
  // TODO(brettw) implement this. We need to know what type of socket to use
  // so we can create the right one (i.e. to pass to TCPSocket::Open) before
  // doing the connect.
  callback.Run(MakeNetworkError(net::ERR_NOT_IMPLEMENTED), NetAddressPtr());
}

void NetworkServiceImpl::CreateUDPSocket(InterfaceRequest<UDPSocket> request) {
  // The lifetime of this UDPSocketImpl is bound to that of the underlying pipe.
  new UDPSocketImpl(std::move(request), app_refcount_->Clone());
}

void NetworkServiceImpl::CreateHttpServer(
    NetAddressPtr local_address,
    HttpServerDelegatePtr delegate,
    const CreateHttpServerCallback& callback) {
  HttpServerImpl::Create(std::move(local_address), std::move(delegate),
                         app_refcount_->Clone(), callback);
}

void NetworkServiceImpl::GetMimeTypeFromFile(
    const mojo::String& file_path,
    const GetMimeTypeFromFileCallback& callback) {
  std::string mime;
  net::GetMimeTypeFromFile(
      base::FilePath::FromUTF8Unsafe(file_path.To<std::string>()), &mime);
  callback.Run(mime);
}

}  // namespace mojo
