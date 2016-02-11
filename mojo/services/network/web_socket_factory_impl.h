// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_NETWORK_WEB_SOCKET_FACTORY_IMPL_H_
#define MOJO_SERVICES_NETWORK_WEB_SOCKET_FACTORY_IMPL_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/services/network/public/interfaces/web_socket_factory.mojom.h"
#include "mojo/shell/public/cpp/shell.h"

namespace mojo {
class NetworkContext;

class WebSocketFactoryImpl : public WebSocketFactory {
 public:
  WebSocketFactoryImpl(NetworkContext* context,
                       scoped_ptr<AppRefCount> app_refcount,
                       InterfaceRequest<WebSocketFactory> request);
  ~WebSocketFactoryImpl() override;

  // WebSocketFactory methods:
  void CreateWebSocket(InterfaceRequest<WebSocket> socket) override;

 private:
  NetworkContext* context_;
  scoped_ptr<AppRefCount> app_refcount_;
  StrongBinding<WebSocketFactory> binding_;

  DISALLOW_COPY_AND_ASSIGN(WebSocketFactoryImpl);
};

}  // namespace mojo

#endif  // MOJO_SERVICES_NETWORK_WEB_SOCKET_FACTORY_IMPL_H_
