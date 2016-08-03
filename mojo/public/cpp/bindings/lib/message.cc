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

namespace mojo {

namespace {

base::LazyInstance<base::ThreadLocalPointer<internal::MessageDispatchContext>>
    g_tls_message_dispatch_context = LAZY_INSTANCE_INITIALIZER;

base::LazyInstance<base::ThreadLocalPointer<SyncMessageResponseContext>>
    g_tls_sync_response_context = LAZY_INSTANCE_INITIALIZER;

}  // namespace

Message::Message() {
}

Message::~Message() {
  CloseHandles();
}

void Message::Initialize(size_t capacity, bool zero_initialized) {
  DCHECK(!buffer_);
  buffer_.reset(new internal::MessageBuffer(capacity, zero_initialized));
}

void Message::InitializeFromMojoMessage(ScopedMessageHandle message,
                                        uint32_t num_bytes,
                                        std::vector<Handle>* handles) {
  DCHECK(!buffer_);
  buffer_.reset(new internal::MessageBuffer(std::move(message), num_bytes));
  handles_.swap(*handles);
}

void Message::MoveTo(Message* destination) {
  DCHECK(this != destination);

  // No copy needed.
  std::swap(destination->buffer_, buffer_);
  std::swap(destination->handles_, handles_);

  CloseHandles();
  handles_.clear();
  buffer_.reset();
}

ScopedMessageHandle Message::TakeMojoMessage() {
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

void Message::CloseHandles() {
  for (std::vector<Handle>::iterator it = handles_.begin();
       it != handles_.end(); ++it) {
    if (it->is_valid())
      CloseRaw(*it);
  }
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
    std::unique_ptr<Message> new_message(new Message);
    response_.MoveTo(new_message.get());
    bad_message_callback_ = base::Bind(&Message::NotifyBadMessage,
                                       base::Owned(new_message.release()));
  }
  return bad_message_callback_;
}

MojoResult ReadMessage(MessagePipeHandle handle, Message* message) {
  MojoResult rv;

  std::vector<Handle> handles;
  ScopedMessageHandle mojo_message;
  uint32_t num_bytes = 0, num_handles = 0;
  rv = ReadMessageNew(handle,
                      &mojo_message,
                      &num_bytes,
                      nullptr,
                      &num_handles,
                      MOJO_READ_MESSAGE_FLAG_NONE);
  if (rv == MOJO_RESULT_RESOURCE_EXHAUSTED) {
    DCHECK_GT(num_handles, 0u);
    handles.resize(num_handles);
    rv = ReadMessageNew(handle,
                        &mojo_message,
                        &num_bytes,
                        reinterpret_cast<MojoHandle*>(handles.data()),
                        &num_handles,
                        MOJO_READ_MESSAGE_FLAG_NONE);
  }

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
    std::unique_ptr<Message> new_message(new Message);
    message_->MoveTo(new_message.get());
    bad_message_callback_ = base::Bind(&Message::NotifyBadMessage,
                                       base::Owned(new_message.release()));
  }
  return bad_message_callback_;
}

// static
void SyncMessageResponseSetup::SetCurrentSyncResponseMessage(Message* message) {
  SyncMessageResponseContext* context = SyncMessageResponseContext::current();
  if (context)
    message->MoveTo(&context->response_);
}

}  // namespace internal

}  // namespace mojo
