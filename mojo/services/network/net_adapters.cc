// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/network/net_adapters.h"

#include <stdint.h>

#include <utility>

#include "net/base/net_errors.h"

namespace mojo {

namespace {

const uint32_t kMaxBufSize = 64 * 1024;

}  // namespace

NetToMojoPendingBuffer::NetToMojoPendingBuffer(
    ScopedDataPipeProducerHandle handle,
    void* buffer)
    : handle_(std::move(handle)), buffer_(buffer) {}

NetToMojoPendingBuffer::~NetToMojoPendingBuffer() {
  if (handle_.is_valid())
    EndWriteDataRaw(handle_.get(), 0);
}

// static
MojoResult NetToMojoPendingBuffer::BeginWrite(
    ScopedDataPipeProducerHandle* handle,
    scoped_refptr<NetToMojoPendingBuffer>* pending,
    uint32_t* num_bytes) {
  void* buf;
  *num_bytes = 0;

  MojoResult result = BeginWriteDataRaw(handle->get(), &buf, num_bytes,
                                        MOJO_WRITE_DATA_FLAG_NONE);
  if (result == MOJO_RESULT_OK) {
    if (*num_bytes > kMaxBufSize)
      *num_bytes = kMaxBufSize;
    *pending = new NetToMojoPendingBuffer(std::move(*handle), buf);
  }
  return result;
}

ScopedDataPipeProducerHandle NetToMojoPendingBuffer::Complete(
    uint32_t num_bytes) {
  EndWriteDataRaw(handle_.get(), num_bytes);
  buffer_ = NULL;
  return std::move(handle_);
}

// -----------------------------------------------------------------------------

NetToMojoIOBuffer::NetToMojoIOBuffer(
    NetToMojoPendingBuffer* pending_buffer)
    : net::WrappedIOBuffer(pending_buffer->buffer()),
      pending_buffer_(pending_buffer) {
}

NetToMojoIOBuffer::~NetToMojoIOBuffer() {
}

// -----------------------------------------------------------------------------

MojoToNetPendingBuffer::MojoToNetPendingBuffer(
    ScopedDataPipeConsumerHandle handle,
    const void* buffer)
    : handle_(std::move(handle)), buffer_(buffer) {}

MojoToNetPendingBuffer::~MojoToNetPendingBuffer() {
}

// static
MojoResult MojoToNetPendingBuffer::BeginRead(
    ScopedDataPipeConsumerHandle* handle,
    scoped_refptr<MojoToNetPendingBuffer>* pending,
    uint32_t* num_bytes) {
  const void* buffer = NULL;
  *num_bytes = 0;
  MojoResult result = BeginReadDataRaw(handle->get(), &buffer, num_bytes,
                                       MOJO_READ_DATA_FLAG_NONE);
  if (result == MOJO_RESULT_OK)
    *pending = new MojoToNetPendingBuffer(std::move(*handle), buffer);
  return result;
}

ScopedDataPipeConsumerHandle MojoToNetPendingBuffer::Complete(
    uint32_t num_bytes) {
  EndReadDataRaw(handle_.get(), num_bytes);
  buffer_ = NULL;
  return std::move(handle_);
}

// -----------------------------------------------------------------------------

MojoToNetIOBuffer::MojoToNetIOBuffer(MojoToNetPendingBuffer* pending_buffer)
    : net::WrappedIOBuffer(pending_buffer->buffer()),
      pending_buffer_(pending_buffer) {
}

MojoToNetIOBuffer::~MojoToNetIOBuffer() {
}

// -----------------------------------------------------------------------------

NetworkErrorPtr MakeNetworkError(int error_code) {
  NetworkErrorPtr error = NetworkError::New();
  error->code = error_code;
  if (error_code <= 0)
    error->description = net::ErrorToString(error_code);
  return error;
}

}  // namespace mojo
