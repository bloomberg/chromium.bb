// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/message.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_local.h"
#include "mojo/public/cpp/bindings/associated_group_controller.h"
#include "mojo/public/cpp/bindings/lib/array_internal.h"

namespace mojo {

namespace {

base::LazyInstance<base::ThreadLocalPointer<internal::MessageDispatchContext>>::
    DestructorAtExit g_tls_message_dispatch_context = LAZY_INSTANCE_INITIALIZER;

base::LazyInstance<base::ThreadLocalPointer<SyncMessageResponseContext>>::
    DestructorAtExit g_tls_sync_response_context = LAZY_INSTANCE_INITIALIZER;

void DoNotifyBadMessage(Message message, const std::string& error) {
  message.NotifyBadMessage(error);
}

template <typename HeaderType>
void AllocateHeaderFromBuffer(internal::Buffer* buffer, HeaderType** header) {
  *header = static_cast<HeaderType*>(buffer->Allocate(sizeof(HeaderType)));
  (*header)->num_bytes = sizeof(HeaderType);
}

// An internal serialization context used to initialize new serialized messages.
struct MessageInfo {
  MessageInfo(size_t total_size, std::vector<ScopedHandle>* handles)
      : total_size(total_size), handles(handles) {}
  const size_t total_size;
  std::vector<ScopedHandle>* handles;
  internal::Buffer payload_buffer;
};

void GetSerializedSizeFromMessageInfo(uintptr_t context,
                                      size_t* num_bytes,
                                      size_t* num_handles) {
  auto* info = reinterpret_cast<MessageInfo*>(context);
  *num_bytes = info->total_size;
  *num_handles = info->handles ? info->handles->size() : 0;
}

void SerializeHandlesFromMessageInfo(uintptr_t context, MojoHandle* handles) {
  auto* info = reinterpret_cast<MessageInfo*>(context);
  DCHECK(info->handles);
  for (size_t i = 0; i < info->handles->size(); ++i)
    handles[i] = info->handles->at(i).release().value();
}

void SerializePayloadFromMessageInfo(uintptr_t context, void* storage) {
  auto* info = reinterpret_cast<MessageInfo*>(context);
  info->payload_buffer = internal::Buffer(storage, info->total_size);
}

void DestroyMessageInfo(uintptr_t context) {
  // MessageInfo is always stack-allocated, so there's nothing to do here.
}

const MojoMessageOperationThunks kMessageInfoThunks{
    sizeof(MojoMessageOperationThunks),
    &GetSerializedSizeFromMessageInfo,
    &SerializeHandlesFromMessageInfo,
    &SerializePayloadFromMessageInfo,
    &DestroyMessageInfo,
};

size_t ComputeHeaderSize(uint32_t flags, size_t payload_interface_id_count) {
  if (payload_interface_id_count > 0) {
    // Version 2
    return sizeof(internal::MessageHeaderV2);
  } else if (flags &
             (Message::kFlagExpectsResponse | Message::kFlagIsResponse)) {
    // Version 1
    return sizeof(internal::MessageHeaderV1);
  } else {
    // Version 0
    return sizeof(internal::MessageHeader);
  }
}

size_t ComputeTotalSize(uint32_t flags,
                        size_t payload_size,
                        size_t payload_interface_id_count) {
  const size_t header_size =
      ComputeHeaderSize(flags, payload_interface_id_count);
  if (payload_interface_id_count > 0) {
    return internal::Align(
        header_size + internal::Align(payload_size) +
        internal::ArrayDataTraits<uint32_t>::GetStorageSize(
            static_cast<uint32_t>(payload_interface_id_count)));
  }
  return internal::Align(header_size + payload_size);
}

void WriteMessageHeader(uint32_t name,
                        uint32_t flags,
                        size_t payload_interface_id_count,
                        internal::Buffer* payload_buffer) {
  if (payload_interface_id_count > 0) {
    // Version 2
    internal::MessageHeaderV2* header;
    AllocateHeaderFromBuffer(payload_buffer, &header);
    header->version = 2;
    header->name = name;
    header->flags = flags;
    // The payload immediately follows the header.
    header->payload.Set(header + 1);
  } else if (flags &
             (Message::kFlagExpectsResponse | Message::kFlagIsResponse)) {
    // Version 1
    internal::MessageHeaderV1* header;
    AllocateHeaderFromBuffer(payload_buffer, &header);
    header->version = 1;
    header->name = name;
    header->flags = flags;
  } else {
    internal::MessageHeader* header;
    AllocateHeaderFromBuffer(payload_buffer, &header);
    header->version = 0;
    header->name = name;
    header->flags = flags;
  }
}

void CreateSerializedMessageObject(uint32_t name,
                                   uint32_t flags,
                                   size_t payload_size,
                                   size_t payload_interface_id_count,
                                   std::vector<ScopedHandle>* handles,
                                   ScopedMessageHandle* out_handle,
                                   internal::Buffer* out_buffer) {
  internal::Buffer buffer;
  MessageInfo info(
      ComputeTotalSize(flags, payload_size, payload_interface_id_count),
      handles);
  ScopedMessageHandle handle;
  MojoResult rv = mojo::CreateMessage(reinterpret_cast<uintptr_t>(&info),
                                      &kMessageInfoThunks, &handle);
  DCHECK_EQ(MOJO_RESULT_OK, rv);
  DCHECK(handle.is_valid());

  // Force the message object to be serialized immediately. MessageInfo thunks
  // are invoked here to serialize the handles and allocate enough space for
  // header and user payload. |info.payload_buffer| is initialized with a Buffer
  // that can be used to write the header and payload data.
  rv = MojoSerializeMessage(handle->value());
  DCHECK_EQ(MOJO_RESULT_OK, rv);
  DCHECK(info.payload_buffer.is_valid());

  // Make sure we zero the memory first!
  memset(info.payload_buffer.data(), 0, info.total_size);
  WriteMessageHeader(name, flags, payload_interface_id_count,
                     &info.payload_buffer);

  *out_handle = std::move(handle);
  *out_buffer = std::move(info.payload_buffer);
}

}  // namespace

Message::Message() = default;

Message::Message(Message&& other) = default;

Message::Message(uint32_t name,
                 uint32_t flags,
                 size_t payload_size,
                 size_t payload_interface_id_count) {
  CreateSerializedMessageObject(name, flags, payload_size,
                                payload_interface_id_count, nullptr, &handle_,
                                &payload_buffer_);
  data_ = payload_buffer_.data();
  data_size_ = payload_buffer_.size();
  transferable_ = true;
}

Message::Message(ScopedMessageHandle handle) {
  DCHECK(handle.is_valid());

  // Extract any serialized handles if possible.
  uint32_t num_bytes;
  void* buffer;
  uint32_t num_handles = 0;
  MojoResult rv = MojoGetSerializedMessageContents(
      handle->value(), &buffer, &num_bytes, nullptr, &num_handles,
      MOJO_GET_SERIALIZED_MESSAGE_CONTENTS_FLAG_NONE);
  if (rv == MOJO_RESULT_RESOURCE_EXHAUSTED) {
    handles_.resize(num_handles);
    rv = MojoGetSerializedMessageContents(
        handle->value(), &buffer, &num_bytes,
        reinterpret_cast<MojoHandle*>(handles_.data()), &num_handles,
        MOJO_GET_SERIALIZED_MESSAGE_CONTENTS_FLAG_NONE);
  } else {
    // No handles, so it's safe to retransmit this message if the caller really
    // wants to.
    transferable_ = true;
  }

  if (rv != MOJO_RESULT_OK) {
    // Failed to deserialize handles. Leave the Message uninitialized.
    return;
  }

  handle_ = std::move(handle);
  data_ = buffer;
  data_size_ = num_bytes;
}

Message::~Message() = default;

Message& Message::operator=(Message&& other) = default;

void Message::Reset() {
  handle_.reset();
  handles_.clear();
  associated_endpoint_handles_.clear();
  data_ = nullptr;
  data_size_ = 0;
  transferable_ = false;
}

const uint8_t* Message::payload() const {
  if (version() < 2)
    return data() + header()->num_bytes;

  DCHECK(!header_v2()->payload.is_null());
  return static_cast<const uint8_t*>(header_v2()->payload.Get());
}

uint32_t Message::payload_num_bytes() const {
  DCHECK_GE(data_num_bytes(), header()->num_bytes);
  size_t num_bytes;
  if (version() < 2) {
    num_bytes = data_num_bytes() - header()->num_bytes;
  } else {
    auto payload_begin =
        reinterpret_cast<uintptr_t>(header_v2()->payload.Get());
    auto payload_end =
        reinterpret_cast<uintptr_t>(header_v2()->payload_interface_ids.Get());
    if (!payload_end)
      payload_end = reinterpret_cast<uintptr_t>(data() + data_num_bytes());
    DCHECK_GE(payload_end, payload_begin);
    num_bytes = payload_end - payload_begin;
  }
  DCHECK_LE(num_bytes, std::numeric_limits<uint32_t>::max());
  return static_cast<uint32_t>(num_bytes);
}

uint32_t Message::payload_num_interface_ids() const {
  auto* array_pointer =
      version() < 2 ? nullptr : header_v2()->payload_interface_ids.Get();
  return array_pointer ? static_cast<uint32_t>(array_pointer->size()) : 0;
}

const uint32_t* Message::payload_interface_ids() const {
  auto* array_pointer =
      version() < 2 ? nullptr : header_v2()->payload_interface_ids.Get();
  return array_pointer ? array_pointer->storage() : nullptr;
}

void Message::AttachHandles(std::vector<ScopedHandle>* handles) {
  DCHECK(handles);
  if (handles->empty())
    return;

  // Sanity-check the current serialized message state. We must have a valid
  // message handle and it must have no serialized handles.
  DCHECK(handle_.is_valid());
  DCHECK(payload_buffer_.is_valid());
  void* buffer;
  uint32_t num_bytes;
  uint32_t num_handles = 0;
  MojoResult rv = MojoGetSerializedMessageContents(
      handle_->value(), &buffer, &num_bytes, nullptr, &num_handles,
      MOJO_GET_SERIALIZED_MESSAGE_CONTENTS_FLAG_NONE);
  DCHECK_EQ(MOJO_RESULT_OK, rv);

  MessageInfo new_info(data_size_, handles);
  ScopedMessageHandle new_handle;
  rv = mojo::CreateMessage(reinterpret_cast<uintptr_t>(&new_info),
                           &kMessageInfoThunks, &new_handle);
  DCHECK_EQ(MOJO_RESULT_OK, rv);
  DCHECK(new_handle.is_valid());

  rv = MojoSerializeMessage(new_handle->value());
  DCHECK_EQ(MOJO_RESULT_OK, rv);
  DCHECK(new_info.payload_buffer.is_valid());

  // Copy the existing payload into the new message.
  void* storage = new_info.payload_buffer.Allocate(payload_buffer_.cursor());
  memcpy(storage, data_, data_size_);

  handle_ = std::move(new_handle);
  payload_buffer_ = std::move(new_info.payload_buffer);
  data_ = payload_buffer_.data();
  data_size_ = payload_buffer_.size();
  transferable_ = true;
}

ScopedMessageHandle Message::TakeMojoMessage() {
  // If there are associated endpoints transferred,
  // SerializeAssociatedEndpointHandles() must be called before this method.
  DCHECK(associated_endpoint_handles_.empty());
  DCHECK(transferable_);
  auto handle = std::move(handle_);
  Reset();
  return handle;
}

void Message::NotifyBadMessage(const std::string& error) {
  DCHECK(handle_.is_valid());
  mojo::NotifyBadMessage(handle_.get(), error);
}

void Message::SerializeAssociatedEndpointHandles(
    AssociatedGroupController* group_controller) {
  if (associated_endpoint_handles_.empty())
    return;

  DCHECK_GE(version(), 2u);
  DCHECK(header_v2()->payload_interface_ids.is_null());
  DCHECK(payload_buffer_.is_valid());

  size_t size = associated_endpoint_handles_.size();
  auto* data = internal::Array_Data<uint32_t>::New(size, &payload_buffer_);
  header_v2()->payload_interface_ids.Set(data);

  for (size_t i = 0; i < size; ++i) {
    ScopedInterfaceEndpointHandle& handle = associated_endpoint_handles_[i];

    DCHECK(handle.pending_association());
    data->storage()[i] =
        group_controller->AssociateInterface(std::move(handle));
  }
  associated_endpoint_handles_.clear();
}

bool Message::DeserializeAssociatedEndpointHandles(
    AssociatedGroupController* group_controller) {
  associated_endpoint_handles_.clear();

  uint32_t num_ids = payload_num_interface_ids();
  if (num_ids == 0)
    return true;

  associated_endpoint_handles_.reserve(num_ids);
  uint32_t* ids = header_v2()->payload_interface_ids.Get()->storage();
  bool result = true;
  for (uint32_t i = 0; i < num_ids; ++i) {
    auto handle = group_controller->CreateLocalEndpointHandle(ids[i]);
    if (IsValidInterfaceId(ids[i]) && !handle.is_valid()) {
      // |ids[i]| itself is valid but handle creation failed. In that case, mark
      // deserialization as failed but continue to deserialize the rest of
      // handles.
      result = false;
    }

    associated_endpoint_handles_.push_back(std::move(handle));
    ids[i] = kInvalidInterfaceId;
  }
  return result;
}

PassThroughFilter::PassThroughFilter() {}

PassThroughFilter::~PassThroughFilter() {}

bool PassThroughFilter::Accept(Message* message) {
  return true;
}

SyncMessageResponseContext::SyncMessageResponseContext()
    : outer_context_(current()) {
  g_tls_sync_response_context.Get().Set(this);
}

SyncMessageResponseContext::~SyncMessageResponseContext() {
  DCHECK_EQ(current(), this);
  g_tls_sync_response_context.Get().Set(outer_context_);
}

// static
SyncMessageResponseContext* SyncMessageResponseContext::current() {
  return g_tls_sync_response_context.Get().Get();
}

void SyncMessageResponseContext::ReportBadMessage(const std::string& error) {
  GetBadMessageCallback().Run(error);
}

const ReportBadMessageCallback&
SyncMessageResponseContext::GetBadMessageCallback() {
  if (bad_message_callback_.is_null()) {
    bad_message_callback_ =
        base::Bind(&DoNotifyBadMessage, base::Passed(&response_));
  }
  return bad_message_callback_;
}

MojoResult ReadMessage(MessagePipeHandle handle, Message* message) {
  ScopedMessageHandle message_handle;
  MojoResult rv =
      ReadMessageNew(handle, &message_handle, MOJO_READ_MESSAGE_FLAG_NONE);
  if (rv != MOJO_RESULT_OK)
    return rv;

  *message = Message(std::move(message_handle));
  return MOJO_RESULT_OK;
}

void ReportBadMessage(const std::string& error) {
  internal::MessageDispatchContext* context =
      internal::MessageDispatchContext::current();
  DCHECK(context);
  context->GetBadMessageCallback().Run(error);
}

ReportBadMessageCallback GetBadMessageCallback() {
  internal::MessageDispatchContext* context =
      internal::MessageDispatchContext::current();
  DCHECK(context);
  return context->GetBadMessageCallback();
}

namespace internal {

MessageHeaderV2::MessageHeaderV2() = default;

MessageDispatchContext::MessageDispatchContext(Message* message)
    : outer_context_(current()), message_(message) {
  g_tls_message_dispatch_context.Get().Set(this);
}

MessageDispatchContext::~MessageDispatchContext() {
  DCHECK_EQ(current(), this);
  g_tls_message_dispatch_context.Get().Set(outer_context_);
}

// static
MessageDispatchContext* MessageDispatchContext::current() {
  return g_tls_message_dispatch_context.Get().Get();
}

const ReportBadMessageCallback&
MessageDispatchContext::GetBadMessageCallback() {
  if (bad_message_callback_.is_null()) {
    bad_message_callback_ =
        base::Bind(&DoNotifyBadMessage, base::Passed(message_));
  }
  return bad_message_callback_;
}

// static
void SyncMessageResponseSetup::SetCurrentSyncResponseMessage(Message* message) {
  SyncMessageResponseContext* context = SyncMessageResponseContext::current();
  if (context)
    context->response_ = std::move(*message);
}

}  // namespace internal

}  // namespace mojo
