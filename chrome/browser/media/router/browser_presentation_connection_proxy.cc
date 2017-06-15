// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/browser_presentation_connection_proxy.h"

#include <vector>

#include "base/memory/ptr_util.h"

#include "chrome/browser/media/router/media_router.h"

namespace media_router {

BrowserPresentationConnectionProxy::BrowserPresentationConnectionProxy(
    MediaRouter* router,
    const MediaRoute::Id& route_id,
    blink::mojom::PresentationConnectionRequest receiver_connection_request,
    blink::mojom::PresentationConnectionPtr controller_connection_ptr)
    : router_(router),
      route_id_(route_id),
      binding_(this),
      target_connection_ptr_(std::move(controller_connection_ptr)) {
  DCHECK(router);
  DCHECK(target_connection_ptr_);

  binding_.Bind(std::move(receiver_connection_request));
  target_connection_ptr_->DidChangeState(
      content::PRESENTATION_CONNECTION_STATE_CONNECTED);
}

BrowserPresentationConnectionProxy::~BrowserPresentationConnectionProxy() {}

void BrowserPresentationConnectionProxy::OnMessage(
    content::PresentationConnectionMessage message,
    const OnMessageCallback& on_message_callback) {
  DVLOG(2) << "BrowserPresentationConnectionProxy::OnMessage";
  if (message.is_binary()) {
    router_->SendRouteBinaryMessage(
        route_id_,
        base::MakeUnique<std::vector<uint8_t>>(std::move(message.data.value())),
        on_message_callback);
  } else {
    router_->SendRouteMessage(route_id_, message.message.value(),
                              on_message_callback);
  }
}

}  // namespace media_router
