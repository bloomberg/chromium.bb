// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_NETWORK_HTTP_CONNECTION_IMPL_H_
#define MOJO_SERVICES_NETWORK_HTTP_CONNECTION_IMPL_H_

#include <string>

#include "base/macros.h"
#include "mojo/services/network/public/interfaces/http_connection.mojom.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/binding.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/error_handler.h"

namespace net {
class HttpServerRequestInfo;
}

namespace mojo {

class HttpServerImpl;

class HttpConnectionImpl : public HttpConnection,
                           public ErrorHandler {
 public:
  // |owner| must outlive this object.
  HttpConnectionImpl(int connection_id,
                     HttpServerImpl* owner,
                     HttpConnectionDelegatePtr delegate,
                     HttpConnectionPtr* connection);

  ~HttpConnectionImpl() override;

  void OnReceivedHttpRequest(const net::HttpServerRequestInfo& info);
  void OnReceivedWebSocketRequest(const net::HttpServerRequestInfo& info);
  void OnReceivedWebSocketMessage(const std::string& data);

 private:
  // HttpConnection implementation.
  void SetSendBufferSize(uint32_t size,
                         const SetSendBufferSizeCallback& callback) override;
  void SetReceiveBufferSize(
      uint32_t size,
      const SetReceiveBufferSizeCallback& callback) override;

  // ErrorHandler implementation.
  void OnConnectionError() override;

  const int connection_id_;
  HttpServerImpl* const owner_;
  HttpConnectionDelegatePtr delegate_;
  Binding<HttpConnection> binding_;

  DISALLOW_COPY_AND_ASSIGN(HttpConnectionImpl);
};

}  // namespace mojo

#endif  // MOJO_SERVICES_NETWORK_HTTP_CONNECTION_IMPL_H_
