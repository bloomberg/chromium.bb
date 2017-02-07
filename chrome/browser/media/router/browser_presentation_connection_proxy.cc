// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/browser_presentation_connection_proxy.h"

#include "chrome/browser/media/router/media_router.h"
#include "content/public/common/presentation_constants.h"

namespace {

// TODO(crbug.com/632623): remove this function when we finish typemaps for
// presentation.mojom.
std::unique_ptr<content::PresentationConnectionMessage>
PresentationConnectionMessageFromMojo(
    blink::mojom::ConnectionMessagePtr input) {
  std::unique_ptr<content::PresentationConnectionMessage> output;
  if (input.is_null())
    return output;

  switch (input->type) {
    case blink::mojom::PresentationMessageType::TEXT: {
      // Return nullptr content::PresentationSessionMessage if invalid (unset
      // |message|, set |data|, or size too large).
      if (input->data || !input->message ||
          input->message->size() >
              content::kMaxPresentationConnectionMessageSize)
        return output;

      output.reset(new content::PresentationConnectionMessage(
          content::PresentationMessageType::TEXT));
      output->message = std::move(input->message.value());
      return output;
    }
    case blink::mojom::PresentationMessageType::BINARY: {
      // Return nullptr content::PresentationSessionMessage if invalid (unset
      // |data|, set |message|, or size too large).
      if (!input->data || input->message ||
          input->data->size() > content::kMaxPresentationConnectionMessageSize)
        return output;

      output.reset(new content::PresentationConnectionMessage(
          content::PresentationMessageType::BINARY));
      output->data.reset(
          new std::vector<uint8_t>(std::move(input->data.value())));
      return output;
    }
  }

  NOTREACHED() << "Invalid presentation message type " << input->type;
  return output;
}

}  // namespace

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
    blink::mojom::ConnectionMessagePtr connection_message,
    const OnMessageCallback& on_message_callback) {
  DVLOG(2) << "BrowserPresentationConnectionProxy::OnMessage";
  DCHECK(!connection_message.is_null());

  auto message =
      PresentationConnectionMessageFromMojo(std::move(connection_message));

  if (message->is_binary()) {
    router_->SendRouteBinaryMessage(route_id_, std::move(message->data),
                                    on_message_callback);
  } else {
    router_->SendRouteMessage(route_id_, message->message, on_message_callback);
  }
}

}  // namespace media_router
