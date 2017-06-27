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

SerializedHandleVector::SerializedHandleVector() = default;

SerializedHandleVector::~SerializedHandleVector() = default;

void SerializedHandleVector::AddHandle(mojo::ScopedHandle handle) {
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

void SerializedHandleVector::AddInterfaceInfo(
    mojo::ScopedMessagePipeHandle handle,
    uint32_t version) {
  AddHandle(ScopedHandle::From(std::move(handle)));
  serialized_interface_versions_.emplace_back(version);
}

void SerializedHandleVector::ConsumeNextSerializedHandle(
    Handle_Data* out_data) {
  DCHECK_LT(next_serialized_handle_index_, serialized_handles_.size());
  *out_data = serialized_handles_[next_serialized_handle_index_++];
}

void SerializedHandleVector::ConsumeNextSerializedInterfaceInfo(
    Interface_Data* out_data) {
  ConsumeNextSerializedHandle(&out_data->handle);
  DCHECK_LT(next_serialized_version_index_,
            serialized_interface_versions_.size());
  out_data->version =
      serialized_interface_versions_[next_serialized_version_index_++];
}

mojo::ScopedHandle SerializedHandleVector::TakeHandle(
    const Handle_Data& encoded_handle) {
  if (!encoded_handle.is_valid())
    return mojo::ScopedHandle();
  DCHECK_LT(encoded_handle.value, handles_.size());
  return std::move(handles_[encoded_handle.value]);
}

void SerializedHandleVector::Swap(std::vector<mojo::ScopedHandle>* other) {
  handles_.swap(*other);
}

SerializationContext::SerializationContext() {}

SerializationContext::~SerializationContext() {
  DCHECK(!custom_contexts || custom_contexts->empty());
}

void SerializationContext::AttachHandlesToMessage(Message* message) {
  associated_endpoint_handles.swap(
      *message->mutable_associated_endpoint_handles());
}

}  // namespace internal
}  // namespace mojo
