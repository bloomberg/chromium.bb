// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/presentation/browser_presentation_connection_proxy.h"

#include <memory>
#include <vector>

#include "chrome/browser/media/router/media_router.h"
#include "chrome/browser/media/router/route_message_util.h"

namespace media_router {

namespace {

void LogMojoPipeError() {
  DVLOG(1) << "BrowserPresentationConnectionProxy mojo pipe error!";
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
      blink::mojom::PresentationConnectionState::CONNECTED);
  // TODO(btolsch): These pipes may need proper mojo error handlers.  They
  // probably need to be plumbed up to PSDImpl so the PresentationFrame knows
  // about the error.
  binding_.set_connection_error_handler(base::BindOnce(LogMojoPipeError));
  target_connection_ptr_.set_connection_error_handler(
      base::BindOnce(LogMojoPipeError));
}

BrowserPresentationConnectionProxy::~BrowserPresentationConnectionProxy() {}

void BrowserPresentationConnectionProxy::OnMessage(
    blink::mojom::PresentationConnectionMessagePtr message) {
  DVLOG(2) << "BrowserPresentationConnectionProxy::OnMessage";
  if (message->is_data()) {
    router_->SendRouteBinaryMessage(
        route_id_,
        std::make_unique<std::vector<uint8_t>>(std::move(message->get_data())));
  } else {
    router_->SendRouteMessage(route_id_, message->get_message());
  }
}

void BrowserPresentationConnectionProxy::OnMessagesReceived(
    std::vector<mojom::RouteMessagePtr> messages) {
  DVLOG(2) << __func__ << ", number of messages : " << messages.size();
  // TODO(imcheng): It would be slightly more efficient to send messages in
  // a single batch.
  for (auto& message : messages) {
    target_connection_ptr_->OnMessage(
        message_util::PresentationConnectionFromRouteMessage(
            std::move(message)));
  }
}
}  // namespace media_router
