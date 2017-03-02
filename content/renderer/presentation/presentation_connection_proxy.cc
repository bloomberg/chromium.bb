// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/presentation/presentation_connection_proxy.h"

#include "base/logging.h"
#include "content/public/common/presentation_session.h"
#include "content/renderer/presentation/presentation_dispatcher.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/modules/presentation/WebPresentationConnection.h"
#include "third_party/WebKit/public/platform/modules/presentation/WebPresentationController.h"
#include "third_party/WebKit/public/platform/modules/presentation/WebPresentationSessionInfo.h"

namespace content {

PresentationConnectionProxy::PresentationConnectionProxy(
    blink::WebPresentationConnection* source_connection)
    : binding_(this),
      target_connection_ptr_(nullptr),
      source_connection_(source_connection) {
  DCHECK(source_connection_);
}

PresentationConnectionProxy::~PresentationConnectionProxy() = default;

void PresentationConnectionProxy::SendConnectionMessage(
    blink::mojom::ConnectionMessagePtr connection_message,
    const OnMessageCallback& callback) const {
  DCHECK(target_connection_ptr_);
  target_connection_ptr_->OnMessage(std::move(connection_message), callback);
}

void PresentationConnectionProxy::OnMessage(
    blink::mojom::ConnectionMessagePtr message,
    const OnMessageCallback& callback) {
  DCHECK(!callback.is_null());

  switch (message->type) {
    case blink::mojom::PresentationMessageType::TEXT: {
      DCHECK(message->message);
      source_connection_->didReceiveTextMessage(
          blink::WebString::fromUTF8(message->message.value()));
      break;
    }
    case blink::mojom::PresentationMessageType::BINARY: {
      DCHECK(message->data);
      source_connection_->didReceiveBinaryMessage(&(message->data->front()),
                                                  message->data->size());
      break;
    }
    default: {
      callback.Run(false);
      NOTREACHED();
      return;
    }
  }

  callback.Run(true);
}

// TODO(crbug.com/588874): Ensure legal PresentationConnection state transitions
// in a single place.
void PresentationConnectionProxy::DidChangeState(
    content::PresentationConnectionState state) {
  if (state == content::PRESENTATION_CONNECTION_STATE_CONNECTED) {
    source_connection_->didChangeState(
        blink::WebPresentationConnectionState::Connected);
  } else if (state == content::PRESENTATION_CONNECTION_STATE_CLOSED) {
    source_connection_->didChangeState(
        blink::WebPresentationConnectionState::Closed);
  } else {
    NOTREACHED();
  }
}

void PresentationConnectionProxy::OnClose() {
  DCHECK(target_connection_ptr_);
  source_connection_->didChangeState(
      blink::WebPresentationConnectionState::Closed);
  target_connection_ptr_->DidChangeState(
      content::PRESENTATION_CONNECTION_STATE_CLOSED);
}

void PresentationConnectionProxy::close() const {
  DCHECK(target_connection_ptr_);
  target_connection_ptr_->OnClose();
}

ControllerConnectionProxy::ControllerConnectionProxy(
    blink::WebPresentationConnection* controller_connection)
    : PresentationConnectionProxy(controller_connection) {}

ControllerConnectionProxy::~ControllerConnectionProxy() = default;

blink::mojom::PresentationConnectionPtr ControllerConnectionProxy::Bind() {
  return binding_.CreateInterfacePtrAndBind();
}

blink::mojom::PresentationConnectionRequest
ControllerConnectionProxy::MakeRemoteRequest() {
  DCHECK(!target_connection_ptr_)
      << "target_connection_ptr_ should only be bound once.";
  return mojo::MakeRequest(&target_connection_ptr_);
}

ReceiverConnectionProxy::ReceiverConnectionProxy(
    blink::WebPresentationConnection* receiver_connection)
    : PresentationConnectionProxy(receiver_connection) {}

ReceiverConnectionProxy::~ReceiverConnectionProxy() = default;

void ReceiverConnectionProxy::Bind(
    blink::mojom::PresentationConnectionRequest receiver_connection_request) {
  binding_.Bind(std::move(receiver_connection_request));
}

void ReceiverConnectionProxy::BindControllerConnection(
    blink::mojom::PresentationConnectionPtr controller_connection_ptr) {
  DCHECK(!target_connection_ptr_);
  target_connection_ptr_ = std::move(controller_connection_ptr);
  target_connection_ptr_->DidChangeState(
      content::PRESENTATION_CONNECTION_STATE_CONNECTED);

  DidChangeState(content::PRESENTATION_CONNECTION_STATE_CONNECTED);
}

}  // namespace content
