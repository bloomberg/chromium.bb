// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/network/web_socket_factory_impl.h"

#include "mojo/services/network/web_socket_impl.h"

namespace mojo {

WebSocketFactoryImpl::WebSocketFactoryImpl(
    NetworkContext* context,
    scoped_ptr<AppRefCount> app_refcount,
    InterfaceRequest<WebSocketFactory> request)
    : context_(context),
      app_refcount_(app_refcount.Pass()),
      binding_(this, request.Pass()) {
}

WebSocketFactoryImpl::~WebSocketFactoryImpl() {
}

void WebSocketFactoryImpl::CreateWebSocket(InterfaceRequest<WebSocket> socket) {
  new WebSocketImpl(context_, app_refcount_->Clone(), socket.Pass());
}

}  // namespace mojo
