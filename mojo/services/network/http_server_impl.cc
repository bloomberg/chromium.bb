// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/network/http_server_impl.h"

#include <utility>

#include "base/logging.h"
#include "mojo/services/network/http_connection_impl.h"
#include "mojo/services/network/net_adapters.h"
#include "mojo/services/network/net_address_type_converters.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/log/net_log.h"
#include "net/socket/tcp_server_socket.h"

namespace mojo {

namespace {

const int kBackLog = 10;

}  // namespace

// static
void HttpServerImpl::Create(
    NetAddressPtr local_address,
    HttpServerDelegatePtr delegate,
    scoped_ptr<mojo::AppRefCount> app_refcount,
    const Callback<void(NetworkErrorPtr, NetAddressPtr)>& callback) {
  HttpServerImpl* http_server =
      new HttpServerImpl(std::move(delegate), std::move(app_refcount));

  int net_error = http_server->Start(std::move(local_address));
  if (net_error != net::OK) {
    callback.Run(MakeNetworkError(net_error), nullptr);
    delete http_server;
    return;
  }
  callback.Run(MakeNetworkError(net::OK), http_server->GetLocalAddress());
}

HttpServerImpl::HttpServerImpl(HttpServerDelegatePtr delegate,
                               scoped_ptr<mojo::AppRefCount> app_refcount)
    : delegate_(std::move(delegate)), app_refcount_(std::move(app_refcount)) {
  DCHECK(delegate_);
  delegate_.set_connection_error_handler([this]() { delete this; });
}

HttpServerImpl::~HttpServerImpl() {}

int HttpServerImpl::Start(NetAddressPtr local_address) {
  DCHECK(local_address);

  scoped_ptr<net::ServerSocket> socket(
      new net::TCPServerSocket(nullptr, net::NetLog::Source()));
  int net_result = socket->Listen(local_address.To<net::IPEndPoint>(),
                                  kBackLog);
  if (net_result != net::OK)
    return net_result;

  server_.reset(new net::HttpServer(std::move(socket), this));

  return net::OK;
}

NetAddressPtr HttpServerImpl::GetLocalAddress() const {
  if (!server_)
    return nullptr;

  net::IPEndPoint address;
  int net_result = server_->GetLocalAddress(&address);
  if (net_result != net::OK)
    return nullptr;

  return NetAddress::From(address);
}

void HttpServerImpl::OnConnect(int connection_id) {
  DCHECK(connections_.find(connection_id) == connections_.end());

  HttpConnectionPtr connection;
  HttpConnectionDelegatePtr connection_delegate;
  InterfaceRequest<HttpConnectionDelegate> delegate_request =
      GetProxy(&connection_delegate);
  linked_ptr<HttpConnectionImpl> connection_impl(new HttpConnectionImpl(
      connection_id, this, std::move(connection_delegate), &connection));

  connections_[connection_id] = connection_impl;

  delegate_->OnConnected(std::move(connection), std::move(delegate_request));
}

void HttpServerImpl::OnHttpRequest(int connection_id,
                                   const net::HttpServerRequestInfo& info) {
  DCHECK(connections_.find(connection_id) != connections_.end());
  connections_[connection_id]->OnReceivedHttpRequest(info);
}

void HttpServerImpl::OnWebSocketRequest(
    int connection_id,
    const net::HttpServerRequestInfo& info) {
  DCHECK(connections_.find(connection_id) != connections_.end());
  connections_[connection_id]->OnReceivedWebSocketRequest(info);
}

void HttpServerImpl::OnWebSocketMessage(int connection_id,
                                        const std::string& data) {
  DCHECK(connections_.find(connection_id) != connections_.end());
  connections_[connection_id]->OnReceivedWebSocketMessage(data);
}

void HttpServerImpl::OnClose(int connection_id) {
  DCHECK(connections_.find(connection_id) != connections_.end());
  connections_.erase(connection_id);
}

}  // namespace mojo
