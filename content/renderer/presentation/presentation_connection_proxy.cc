// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/presentation/presentation_connection_proxy.h"

#include "base/logging.h"
#include "content/public/common/presentation_info.h"
#include "content/renderer/presentation/presentation_dispatcher.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/modules/presentation/WebPresentationConnection.h"
#include "third_party/WebKit/public/platform/modules/presentation/WebPresentationController.h"
#include "third_party/WebKit/public/platform/modules/presentation/WebPresentationInfo.h"
#include "third_party/WebKit/public/platform/modules/presentation/WebPresentationReceiver.h"

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
    PresentationConnectionMessage message,
    const OnMessageCallback& callback) const {
  DCHECK(target_connection_ptr_);
  target_connection_ptr_->OnMessage(std::move(message), callback);
}

void PresentationConnectionProxy::OnMessage(
    PresentationConnectionMessage message,
    const OnMessageCallback& callback) {
  DCHECK(!callback.is_null());

  if (message.is_binary()) {
    source_connection_->DidReceiveBinaryMessage(&(message.data->front()),
                                                message.data->size());
  } else {
    source_connection_->DidReceiveTextMessage(
        blink::WebString::FromUTF8(*(message.message)));
  }

  callback.Run(true);
}

// TODO(crbug.com/588874): Ensure legal PresentationConnection state transitions
// in a single place.
void PresentationConnectionProxy::DidChangeState(
    content::PresentationConnectionState state) {
  if (state == content::PRESENTATION_CONNECTION_STATE_CONNECTED) {
    source_connection_->DidChangeState(
        blink::WebPresentationConnectionState::kConnected);
  } else if (state == content::PRESENTATION_CONNECTION_STATE_CLOSED) {
    source_connection_->DidClose();
  } else if (state == content::PRESENTATION_CONNECTION_STATE_TERMINATED) {
    source_connection_->DidChangeState(
        blink::WebPresentationConnectionState::kTerminated);
  } else {
    NOTREACHED();
  }
}

void PresentationConnectionProxy::OnClose() {
  DCHECK(target_connection_ptr_);
  DidChangeState(content::PRESENTATION_CONNECTION_STATE_CLOSED);
  target_connection_ptr_->DidChangeState(
      content::PRESENTATION_CONNECTION_STATE_CLOSED);
}

void PresentationConnectionProxy::Close() const {
  DCHECK(target_connection_ptr_);
  target_connection_ptr_->OnClose();
}

void PresentationConnectionProxy::NotifyTargetConnection(
    blink::WebPresentationConnectionState state) {
  if (!target_connection_ptr_)
    return;

  if (state == blink::WebPresentationConnectionState::kTerminated) {
    target_connection_ptr_->DidChangeState(
        content::PRESENTATION_CONNECTION_STATE_TERMINATED);
  }
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
    blink::WebPresentationConnection* receiver_connection,
    blink::WebPresentationReceiver* receiver)
    : PresentationConnectionProxy(receiver_connection), receiver_(receiver) {
  DCHECK(receiver_);
}

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

void ReceiverConnectionProxy::DidChangeState(
    content::PresentationConnectionState state) {
  PresentationConnectionProxy::DidChangeState(state);
  if (state == content::PRESENTATION_CONNECTION_STATE_CLOSED)
    receiver_->RemoveConnection(source_connection_);
}

}  // namespace content
