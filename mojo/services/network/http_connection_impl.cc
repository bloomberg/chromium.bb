// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/network/http_connection_impl.h"

#include <limits>

#include "mojo/services/network/http_server_impl.h"
#include "mojo/services/network/net_adapters.h"
#include "net/base/net_errors.h"
#include "net/server/http_server.h"
#include "net/server/http_server_request_info.h"

namespace mojo {

HttpConnectionImpl::HttpConnectionImpl(int connection_id,
                                       HttpServerImpl* owner,
                                       HttpConnectionDelegatePtr delegate,
                                       HttpConnectionPtr* connection)
    : connection_id_(connection_id),
      owner_(owner),
      delegate_(delegate.Pass()),
      binding_(this, connection) {
  binding_.set_error_handler(this);
  delegate_.set_error_handler(this);
}

HttpConnectionImpl::~HttpConnectionImpl() {}

void HttpConnectionImpl::OnReceivedHttpRequest(
    const net::HttpServerRequestInfo& info) {
  // TODO(yzshen): implement it.
}

void HttpConnectionImpl::OnReceivedWebSocketRequest(
    const net::HttpServerRequestInfo& info) {
  // TODO(yzshen): implement it.
}

void HttpConnectionImpl::OnReceivedWebSocketMessage(const std::string& data) {
  // TODO(yzshen): implement it.
}

void HttpConnectionImpl::SetSendBufferSize(
    uint32_t size,
    const SetSendBufferSizeCallback& callback) {
  if (size > static_cast<uint32_t>(std::numeric_limits<int32_t>::max()))
    size = std::numeric_limits<int32_t>::max();

  owner_->server()->SetSendBufferSize(
      connection_id_, static_cast<int32_t>(size));
  callback.Run(MakeNetworkError(net::OK));
}

void HttpConnectionImpl::SetReceiveBufferSize(
    uint32_t size,
    const SetReceiveBufferSizeCallback& callback) {
  if (size > static_cast<uint32_t>(std::numeric_limits<int32_t>::max()))
    size = std::numeric_limits<int32_t>::max();

  owner_->server()->SetReceiveBufferSize(
      connection_id_, static_cast<int32_t>(size));
  callback.Run(MakeNetworkError(net::OK));
}

void HttpConnectionImpl::OnConnectionError() {
  // The proxy side of |binding_| or the impl side of |delegate_| has closed the
  // pipe. The connection is not needed anymore.
  owner_->server()->Close(connection_id_);
}

}  // namespace mojo
