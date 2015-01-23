// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/web_data_consumer_handle_impl.h"

#include <limits>
#include "base/bind.h"
#include "base/logging.h"
#include "third_party/mojo/src/mojo/public/c/system/types.h"

namespace content {

typedef blink::WebDataConsumerHandle::Result Result;

WebDataConsumerHandleImpl::WebDataConsumerHandleImpl(Handle handle)
    : handle_(handle.Pass()), client_(nullptr) {}

WebDataConsumerHandleImpl::~WebDataConsumerHandleImpl() {}

Result WebDataConsumerHandleImpl::read(
    void* data,
    size_t size,
    Flags flags,
    size_t* read_size) {
  // We need this variable definition to avoid a link error.
  const Flags kNone = FlagNone;
  DCHECK_EQ(flags, kNone);
  DCHECK_LE(size, std::numeric_limits<uint32_t>::max());

  *read_size = 0;

  uint32_t size_to_pass = size;
  MojoReadDataFlags flags_to_pass = MOJO_READ_DATA_FLAG_NONE;
  MojoResult rv =
      mojo::ReadDataRaw(handle_.get(), data, &size_to_pass, flags_to_pass);
  if (rv == MOJO_RESULT_OK)
    *read_size = size_to_pass;

  return HandleReadResult(rv);
}

Result WebDataConsumerHandleImpl::beginRead(
    const void** buffer, Flags flags, size_t* available) {
  // We need this variable definition to avoid a link error.
  const Flags kNone = FlagNone;
  DCHECK_EQ(flags, kNone);

  *buffer = nullptr;
  *available = 0;

  uint32_t size_to_pass = 0;
  MojoReadDataFlags flags_to_pass = MOJO_READ_DATA_FLAG_NONE;

  MojoResult rv = mojo::BeginReadDataRaw(handle_.get(), buffer,
                                         &size_to_pass, flags_to_pass);
  if (rv == MOJO_RESULT_OK)
    *available = size_to_pass;
  return HandleReadResult(rv);
}

Result WebDataConsumerHandleImpl::endRead(size_t read_size) {
  MojoResult rv = mojo::EndReadDataRaw(handle_.get(), read_size);
  return
      rv == MOJO_RESULT_OK ? Ok : UnexpectedError;
}

void WebDataConsumerHandleImpl::registerClient(Client* client) {
  DCHECK(!client_);
  DCHECK(client);
  client_ = client;

  handle_watcher_.Start(
      handle_.get(),
      MOJO_HANDLE_SIGNAL_READABLE,
      MOJO_DEADLINE_INDEFINITE,
      base::Bind(&WebDataConsumerHandleImpl::OnHandleGotReadable,
                 base::Unretained(this)));
}

void WebDataConsumerHandleImpl::unregisterClient() {
  client_ = nullptr;
  handle_watcher_.Stop();
}

Result WebDataConsumerHandleImpl::HandleReadResult(MojoResult mojo_result) {
  switch (mojo_result) {
    case MOJO_RESULT_OK:
      return Ok;
    case MOJO_RESULT_FAILED_PRECONDITION:
      return Done;
    case MOJO_RESULT_BUSY:
      return Busy;
    case MOJO_RESULT_SHOULD_WAIT:
      if (client_) {
        handle_watcher_.Start(
            handle_.get(),
            MOJO_HANDLE_SIGNAL_READABLE,
            MOJO_DEADLINE_INDEFINITE,
            base::Bind(&WebDataConsumerHandleImpl::OnHandleGotReadable,
                       base::Unretained(this)));
      }
      return ShouldWait;
    case MOJO_RESULT_RESOURCE_EXHAUSTED:
      return ResourceExhausted;
    default:
      return UnexpectedError;
  }
}

void WebDataConsumerHandleImpl::OnHandleGotReadable(MojoResult) {
  DCHECK(client_);
  client_->didGetReadable();
}

}  // namespace content
