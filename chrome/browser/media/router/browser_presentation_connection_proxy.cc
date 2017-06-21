// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/browser_presentation_connection_proxy.h"

#include <vector>

#include "base/memory/ptr_util.h"

#include "chrome/browser/media/router/media_router.h"
#include "chrome/common/media_router/route_message.h"

namespace media_router {

namespace {

void OnMessageReceivedByRenderer(bool success) {
  DLOG_IF(ERROR, !success)
      << "Renderer PresentationConnection failed to process message!";
}

}  // namespace

BrowserPresentationConnectionProxy::BrowserPresentationConnectionProxy(
    MediaRouter* router,
    const MediaRoute::Id& route_id,
    blink::mojom::PresentationConnectionRequest receiver_connection_request,
    blink::mojom::PresentationConnectionPtr controller_connection_ptr)
    : RouteMessageObserver(router, route_id),
      router_(router),
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
    OnMessageCallback on_message_callback) {
  DVLOG(2) << "BrowserPresentationConnectionProxy::OnMessage";
  if (message.is_binary()) {
    router_->SendRouteBinaryMessage(
        route_id_,
        base::MakeUnique<std::vector<uint8_t>>(std::move(message.data.value())),
        std::move(on_message_callback));
  } else {
    router_->SendRouteMessage(route_id_, message.message.value(),
                              std::move(on_message_callback));
  }
}

void BrowserPresentationConnectionProxy::OnMessagesReceived(
    const std::vector<RouteMessage>& messages) {
  DVLOG(2) << __func__ << ", number of messages : " << messages.size();
  // TODO(mfoltz): Remove RouteMessage and replace with move-only
  // PresentationConnectionMessage.
  std::vector<content::PresentationConnectionMessage> presentation_messages;
  for (const RouteMessage& message : messages) {
    if (message.type == RouteMessage::TEXT && message.text) {
      presentation_messages.emplace_back(message.text.value());
    } else if (message.type == RouteMessage::BINARY && message.binary) {
      presentation_messages.emplace_back(message.binary.value());
    } else {
      NOTREACHED() << "Unknown route message type";
    }
  }

  // TODO(imcheng): It would be slightly more efficient to send messages in
  // a single batch.
  for (auto& message : presentation_messages) {
    target_connection_ptr_->OnMessage(std::move(message),
                                      base::Bind(&OnMessageReceivedByRenderer));
  }
}
}  // namespace media_router
