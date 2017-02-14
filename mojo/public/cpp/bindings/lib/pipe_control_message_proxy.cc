// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/pipe_control_message_proxy.h"

#include <stddef.h>
#include <utility>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "mojo/public/cpp/bindings/lib/message_builder.h"
#include "mojo/public/cpp/bindings/lib/serialization.h"
#include "mojo/public/interfaces/bindings/pipe_control_messages.mojom.h"

namespace mojo {
namespace {

Message ConstructRunOrClosePipeMessage(
    pipe_control::RunOrClosePipeInputPtr input_ptr) {
  internal::SerializationContext context;

  auto params_ptr = pipe_control::RunOrClosePipeMessageParams::New();
  params_ptr->input = std::move(input_ptr);

  size_t size = internal::PrepareToSerialize<
      pipe_control::RunOrClosePipeMessageParamsDataView>(params_ptr, &context);
  internal::MessageBuilder builder(pipe_control::kRunOrClosePipeMessageId, 0,
                                   size, 0);

  pipe_control::internal::RunOrClosePipeMessageParams_Data* params = nullptr;
  internal::Serialize<pipe_control::RunOrClosePipeMessageParamsDataView>(
      params_ptr, builder.buffer(), &params, &context);
  builder.message()->set_interface_id(kInvalidInterfaceId);
  return std::move(*builder.message());
}

}  // namespace

PipeControlMessageProxy::PipeControlMessageProxy(MessageReceiver* receiver)
    : receiver_(receiver) {}

void PipeControlMessageProxy::NotifyPeerEndpointClosed(
    InterfaceId id,
    const base::Optional<DisconnectReason>& reason) {
  Message message(ConstructPeerEndpointClosedMessage(id, reason));
  bool ok = receiver_->Accept(&message);
  ALLOW_UNUSED_LOCAL(ok);
}

// static
Message PipeControlMessageProxy::ConstructPeerEndpointClosedMessage(
    InterfaceId id,
    const base::Optional<DisconnectReason>& reason) {
  auto event = pipe_control::PeerAssociatedEndpointClosedEvent::New();
  event->id = id;
  if (reason) {
    event->disconnect_reason = pipe_control::DisconnectReason::New();
    event->disconnect_reason->custom_reason = reason->custom_reason;
    event->disconnect_reason->description = reason->description;
  }

  auto input = pipe_control::RunOrClosePipeInput::New();
  input->set_peer_associated_endpoint_closed_event(std::move(event));

  return ConstructRunOrClosePipeMessage(std::move(input));
}

}  // namespace mojo
