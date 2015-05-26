// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVTOOLS_SERVICE_DEVTOOLS_COORDINATOR_IMPL_H_
#define COMPONENTS_DEVTOOLS_SERVICE_DEVTOOLS_COORDINATOR_IMPL_H_

#include <set>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "components/devtools_service/public/interfaces/devtools_service.mojom.h"
#include "mojo/common/weak_binding_set.h"
#include "mojo/services/network/public/interfaces/http_connection.mojom.h"
#include "mojo/services/network/public/interfaces/http_server.mojom.h"

namespace mojo {
class ApplicationImpl;
}

namespace devtools_service {

// DevToolsCoordinatorImpl is the central control of the DevTools service. It
// manages the communication with DevTools agents (e.g., Web page renderers). It
// also starts an HTTP server to speak the Chrome remote debugging protocol.
class DevToolsCoordinatorImpl : public DevToolsCoordinator,
                                public mojo::HttpServerDelegate {
 public:
  // Doesn't take ownership of |application|, which must outlive this object.
  explicit DevToolsCoordinatorImpl(mojo::ApplicationImpl* application);
  ~DevToolsCoordinatorImpl() override;

  void CreateAgentClient(mojo::InterfaceRequest<DevToolsAgentClient> request);
  void BindToCoordinatorRequest(
      mojo::InterfaceRequest<DevToolsCoordinator> request);

 private:
  class HttpConnectionDelegateImpl;

  // DevToolsCoordinator implementation.
  void Initialize(uint16_t remote_debugging_port) override;

  // mojo::HttpServerDelegate implementation.
  void OnConnected(
      mojo::HttpConnectionPtr connection,
      mojo::InterfaceRequest<mojo::HttpConnectionDelegate> delegate) override;

  bool IsInitialized() const { return !!http_server_delegate_binding_; }

  // The following methods are called by HttpConnectionDelegateImpl.
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

  // Not owned by this object.
  mojo::ApplicationImpl* const application_;

  mojo::WeakBindingSet<DevToolsCoordinator> coordinator_bindings_;

  scoped_ptr<mojo::Binding<mojo::HttpServerDelegate>>
      http_server_delegate_binding_;

  // Owns the elements.
  std::set<HttpConnectionDelegateImpl*> connections_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsCoordinatorImpl);
};

}  // namespace devtools_service

#endif  // COMPONENTS_DEVTOOLS_SERVICE_DEVTOOLS_COORDINATOR_IMPL_H_
