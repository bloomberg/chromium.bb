// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/pipe_control_message_proxy.h"

#include <stddef.h>
#include <utility>

#include "base/macros.h"
#include "mojo/public/cpp/bindings/lib/serialization.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/interfaces/bindings/pipe_control_messages.mojom.h"

namespace mojo {
namespace {

const char kMessageTag[] = "PipeControlMessageProxy";

Message ConstructRunOrClosePipeMessage(
    pipe_control::RunOrClosePipeInputPtr input_ptr) {
  auto params_ptr = pipe_control::RunOrClosePipeMessageParams::New();
  params_ptr->input = std::move(input_ptr);

  Message message(pipe_control::kRunOrClosePipeMessageId, 0, 0, 0, nullptr);
  internal::SerializationContext context;
  pipe_control::internal::RunOrClosePipeMessageParams_Data::BufferWriter writer;
  internal::Serialize<pipe_control::RunOrClosePipeMessageParamsDataView>(
      params_ptr, message.payload_buffer(), &writer, &context);
  message.set_interface_id(kInvalidInterfaceId);
  message.set_heap_profiler_tag(kMessageTag);
  message.AttachHandlesFromSerializationContext(&context);
  return message;
}

}  // namespace

PipeControlMessageProxy::PipeControlMessageProxy(MessageReceiver* receiver)
    : receiver_(receiver) {}

void PipeControlMessageProxy::NotifyPeerEndpointClosed(
    InterfaceId id,
    const base::Optional<DisconnectReason>& reason) {
  Message message(ConstructPeerEndpointClosedMessage(id, reason));
  message.set_heap_profiler_tag(kMessageTag);
  ignore_result(receiver_->Accept(&message));
}

void PipeControlMessageProxy::PausePeerUntilFlushCompletes(PendingFlush flush) {
  auto input = pipe_control::RunOrClosePipeInput::New();
  input->set_pause_until_flush_completes(
      pipe_control::PauseUntilFlushCompletes::New(flush.PassPipe()));
  Message message(ConstructRunOrClosePipeMessage(std::move(input)));
  message.set_heap_profiler_tag(kMessageTag);
  ignore_result(receiver_->Accept(&message));
}

void PipeControlMessageProxy::FlushAsync(AsyncFlusher flusher) {
  auto input = pipe_control::RunOrClosePipeInput::New();
  input->set_flush_async(pipe_control::FlushAsync::New(flusher.PassPipe()));
  Message message(ConstructRunOrClosePipeMessage(std::move(input)));
  message.set_heap_profiler_tag(kMessageTag);
  ignore_result(receiver_->Accept(&message));
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
