// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/serialization_context.h"

#include <limits>

#include "base/logging.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/system/core.h"

namespace mojo {
namespace internal {

SerializationContext::SerializationContext() = default;

SerializationContext::~SerializationContext() {
  DCHECK(custom_contexts_.empty() ||
         custom_context_index_ == custom_contexts_.size());
}

void SerializationContext::PushNextNullState(bool is_null) {
  null_states_.container().push_back(is_null);
}

bool SerializationContext::IsNextFieldNull() {
  DCHECK_LT(null_state_index_, null_states_.container().size());
  return null_states_.container()[null_state_index_++];
}

void SerializationContext::PushCustomContext(void* custom_context) {
  custom_contexts_.emplace_back(custom_context);
}

void* SerializationContext::ConsumeNextCustomContext() {
  DCHECK_LT(custom_context_index_, custom_contexts_.size());
  return custom_contexts_[custom_context_index_++];
}

void SerializationContext::AddHandle(mojo::ScopedHandle handle) {
  Handle_Data data;
  if (!handle.is_valid()) {
    data.value = kEncodedInvalidHandleValue;
  } else {
    DCHECK_LT(handles_.size(), std::numeric_limits<uint32_t>::max());
    data.value = static_cast<uint32_t>(handles_.size());
    handles_.emplace_back(std::move(handle));
  }
  serialized_handles_.emplace_back(data);
}

void SerializationContext::ConsumeNextSerializedHandle(Handle_Data* out_data) {
  DCHECK_LT(next_serialized_handle_index_, serialized_handles_.size());
  *out_data = serialized_handles_[next_serialized_handle_index_++];
}

void SerializationContext::AddInterfaceInfo(
    mojo::ScopedMessagePipeHandle handle,
    uint32_t version) {
  AddHandle(ScopedHandle::From(std::move(handle)));
  serialized_interface_versions_.emplace_back(version);
}

void SerializationContext::ConsumeNextSerializedInterfaceInfo(
    Interface_Data* out_data) {
  ConsumeNextSerializedHandle(&out_data->handle);
  DCHECK_LT(next_serialized_version_index_,
            serialized_interface_versions_.size());
  out_data->version =
      serialized_interface_versions_[next_serialized_version_index_++];
}

void SerializationContext::AddAssociatedEndpoint(
    ScopedInterfaceEndpointHandle handle) {
  AssociatedEndpointHandle_Data data;
  if (!handle.is_valid()) {
    data.value = kEncodedInvalidHandleValue;
  } else {
    DCHECK_LT(associated_endpoint_handles_.size(),
              std::numeric_limits<uint32_t>::max());
    data.value = static_cast<uint32_t>(associated_endpoint_handles_.size());
    associated_endpoint_handles_.emplace_back(std::move(handle));
  }
  serialized_associated_endpoint_handles_.emplace_back(data);
}

void SerializationContext::ConsumeNextSerializedAssociatedEndpoint(
    AssociatedEndpointHandle_Data* out_data) {
  DCHECK_LT(next_serialized_associated_endpoint_handle_index_,
            serialized_associated_endpoint_handles_.size());
  *out_data = serialized_associated_endpoint_handles_
      [next_serialized_associated_endpoint_handle_index_++];
}

void SerializationContext::AddAssociatedInterfaceInfo(
    ScopedInterfaceEndpointHandle handle,
    uint32_t version) {
  AddAssociatedEndpoint(std::move(handle));
  serialized_interface_versions_.emplace_back(version);
}

void SerializationContext::ConsumeNextSerializedAssociatedInterfaceInfo(
    AssociatedInterface_Data* out_data) {
  ConsumeNextSerializedAssociatedEndpoint(&out_data->handle);
  DCHECK_LT(next_serialized_version_index_,
            serialized_interface_versions_.size());
  out_data->version =
      serialized_interface_versions_[next_serialized_version_index_++];
}

void SerializationContext::PrepareMessage(uint32_t message_name,
                                          uint32_t flags,
                                          size_t payload_size,
                                          Message* message) {
  *message = Message(message_name, flags, payload_size,
                     associated_endpoint_handles_.size(), &handles_);
  associated_endpoint_handles_.swap(
      *message->mutable_associated_endpoint_handles());
}

void SerializationContext::TakeHandlesFromMessage(Message* message) {
  handles_.swap(*message->mutable_handles());
  associated_endpoint_handles_.swap(
      *message->mutable_associated_endpoint_handles());
}

mojo::ScopedHandle SerializationContext::TakeHandle(
    const Handle_Data& encoded_handle) {
  if (!encoded_handle.is_valid())
    return mojo::ScopedHandle();
  DCHECK_LT(encoded_handle.value, handles_.size());
  return std::move(handles_[encoded_handle.value]);
}

mojo::ScopedInterfaceEndpointHandle
SerializationContext::TakeAssociatedEndpointHandle(
    const AssociatedEndpointHandle_Data& encoded_handle) {
  if (!encoded_handle.is_valid())
    return mojo::ScopedInterfaceEndpointHandle();
  DCHECK_LT(encoded_handle.value, associated_endpoint_handles_.size());
  return std::move(associated_endpoint_handles_[encoded_handle.value]);
}

}  // namespace internal
}  // namespace mojo
