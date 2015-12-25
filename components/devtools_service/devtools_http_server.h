// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVTOOLS_SERVICE_DEVTOOLS_HTTP_SERVER_H_
#define COMPONENTS_DEVTOOLS_SERVICE_DEVTOOLS_HTTP_SERVER_H_

#include <stdint.h>

#include <set>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/services/network/public/interfaces/http_connection.mojom.h"
#include "mojo/services/network/public/interfaces/http_message.mojom.h"
#include "mojo/services/network/public/interfaces/http_server.mojom.h"

namespace devtools_service {

class DevToolsService;

class DevToolsHttpServer : public mojo::HttpServerDelegate {
 public:
  // |service| must outlive this object.
  DevToolsHttpServer(DevToolsService* service, uint16_t remote_debugging_port);
  ~DevToolsHttpServer() override;

 private:
  class HttpConnectionDelegateImpl;

  // mojo::HttpServerDelegate implementation.
  void OnConnected(
      mojo::HttpConnectionPtr connection,
      mojo::InterfaceRequest<mojo::HttpConnectionDelegate> delegate) override;

  // The following three methods are called by HttpConnectionDelegateImpl.
  using OnReceivedRequestCallback =
      mojo::HttpConnectionDelegate::OnReceivedRequestCallback;
  void OnReceivedRequest(HttpConnectionDelegateImpl* connection,
                         mojo::HttpRequestPtr request,
                         const OnReceivedRequestCallback& callback);
  using OnReceivedWebSocketRequestCallback =
      mojo::HttpConnectionDelegate::OnReceivedWebSocketRequestCallback;
  void OnReceivedWebSocketRequest(
      HttpConnectionDelegateImpl* connection,
      mojo::HttpRequestPtr request,
      const OnReceivedWebSocketRequestCallback& callback);
  void OnConnectionClosed(HttpConnectionDelegateImpl* connection);

  mojo::HttpResponsePtr ProcessJsonRequest(mojo::HttpRequestPtr request);

  // Not owned by this object.
  DevToolsService* const service_;

  const uint16_t remote_debugging_port_;

  scoped_ptr<mojo::Binding<mojo::HttpServerDelegate>>
      http_server_delegate_binding_;

  // Owns the elements.
  std::set<HttpConnectionDelegateImpl*> connections_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsHttpServer);
};

}  // namespace devtools_service

#endif  // COMPONENTS_DEVTOOLS_SERVICE_DEVTOOLS_HTTP_SERVER_H_
