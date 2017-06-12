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

}  // namespace

Message::Message() {
}

Message::Message(Message&& other)
    : buffer_(std::move(other.buffer_)),
      handles_(std::move(other.handles_)),
      associated_endpoint_handles_(
          std::move(other.associated_endpoint_handles_)) {}

Message::~Message() {}

Message& Message::operator=(Message&& other) {
  Reset();
  std::swap(other.buffer_, buffer_);
  std::swap(other.handles_, handles_);
  std::swap(other.associated_endpoint_handles_, associated_endpoint_handles_);
  return *this;
}

void Message::Reset() {
  handles_.clear();
  associated_endpoint_handles_.clear();
  buffer_.reset();
}

void Message::Initialize(size_t capacity, bool zero_initialized) {
  DCHECK(!buffer_);
  buffer_.reset(new internal::MessageBuffer(capacity, zero_initialized));
}

void Message::InitializeFromMojoMessage(ScopedMessageHandle message,
                                        uint32_t num_bytes,
                                        std::vector<ScopedHandle>* handles) {
  DCHECK(!buffer_);
  buffer_.reset(new internal::MessageBuffer(std::move(message), num_bytes));
  handles_.swap(*handles);
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

ScopedMessageHandle Message::TakeMojoMessage() {
  // If there are associated endpoints transferred,
  // SerializeAssociatedEndpointHandles() must be called before this method.
  DCHECK(associated_endpoint_handles_.empty());

  if (handles_.empty())  // Fast path for the common case: No handles.
    return buffer_->TakeMessage();

  // Allocate a new message with space for the handles, then copy the buffer
  // contents into it.
  //
  // TODO(rockot): We could avoid this copy by extending GetSerializedSize()
  // behavior to collect handles. It's unoptimized for now because it's much
  // more common to have messages with no handles.
  ScopedMessageHandle new_message;
  MojoResult rv = AllocMessage(
      data_num_bytes(),
      handles_.empty() ? nullptr
                       : reinterpret_cast<const MojoHandle*>(handles_.data()),
      handles_.size(),
      MOJO_ALLOC_MESSAGE_FLAG_NONE,
      &new_message);
  CHECK_EQ(rv, MOJO_RESULT_OK);

  // The handles are now owned by the message object.
  for (auto& handle : handles_)
    ignore_result(handle.release());
  handles_.clear();

  void* new_buffer = nullptr;
  rv = GetMessageBuffer(new_message.get(), &new_buffer);
  CHECK_EQ(rv, MOJO_RESULT_OK);

  memcpy(new_buffer, data(), data_num_bytes());
  buffer_.reset();

  return new_message;
}

void Message::NotifyBadMessage(const std::string& error) {
  DCHECK(buffer_);
  buffer_->NotifyBadMessage(error);
}

void Message::SerializeAssociatedEndpointHandles(
    AssociatedGroupController* group_controller) {
  if (associated_endpoint_handles_.empty())
    return;

  DCHECK_GE(version(), 2u);
  DCHECK(header_v2()->payload_interface_ids.is_null());

  size_t size = associated_endpoint_handles_.size();
  auto* data = internal::Array_Data<uint32_t>::New(size, buffer());
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

bool PassThroughFilter::Accept(Message* message) { return true; }

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
  ScopedMessageHandle mojo_message;
  MojoResult rv =
      ReadMessageNew(handle, &mojo_message, MOJO_READ_MESSAGE_FLAG_NONE);
  if (rv != MOJO_RESULT_OK)
    return rv;

  uint32_t num_bytes = 0;
  void* buffer;
  std::vector<ScopedHandle> handles;
  rv = GetSerializedMessageContents(
      mojo_message.get(), &buffer, &num_bytes, &handles,
      MOJO_GET_SERIALIZED_MESSAGE_CONTENTS_FLAG_NONE);
  if (rv != MOJO_RESULT_OK)
    return rv;

  message->InitializeFromMojoMessage(
      std::move(mojo_message), num_bytes, &handles);
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
