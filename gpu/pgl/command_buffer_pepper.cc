// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/common/constants.h"
#include "gpu/pgl/command_buffer_pepper.h"

#ifdef __native_client__
#include <assert.h>
#define NOTREACHED() assert(0)
#else
#include "base/logging.h"
#endif  // __native_client__

using base::SharedMemory;
using gpu::Buffer;
using gpu::CommandBuffer;

CommandBufferPepper::CommandBufferPepper(NPP npp,
                                         NPDevice* device,
                                         NPDeviceContext3D* context)
    : npp_(npp),
      device_(device),
      context_(context) {
}

CommandBufferPepper::~CommandBufferPepper() {
}

// Not implemented in CommandBufferPepper.
bool CommandBufferPepper::Initialize(int32 size) {
  NOTREACHED();
  return false;
}

Buffer CommandBufferPepper::GetRingBuffer() {
  Buffer buffer;
  buffer.ptr = context_->commandBuffer;
  buffer.size = context_->commandBufferEntries * sizeof(int32);
  return buffer;
}

CommandBuffer::State CommandBufferPepper::GetState() {
  if (NPERR_NO_ERROR != device_->flushContext(npp_, context_, NULL, NULL))
    context_->error = gpu::parse_error::kParseGenericError;

  return ConvertState();
}

CommandBuffer::State CommandBufferPepper::Flush(int32 put_offset) {
  context_->putOffset = put_offset;

  if (NPERR_NO_ERROR != device_->flushContext(npp_, context_, NULL, NULL))
    context_->error = gpu::parse_error::kParseGenericError;

  return ConvertState();
}

void CommandBufferPepper::SetGetOffset(int32 get_offset) {
  // Not implemented by proxy.
  NOTREACHED();
}

int32 CommandBufferPepper::CreateTransferBuffer(size_t size) {
  int32 id;
  if (NPERR_NO_ERROR != device_->createBuffer(npp_, context_, size, &id))
    return -1;

  return id;
}

void CommandBufferPepper::DestroyTransferBuffer(int32 id) {
  device_->destroyBuffer(npp_, context_, id);
}

Buffer CommandBufferPepper::GetTransferBuffer(int32 id) {
  NPDeviceBuffer np_buffer;
  if (NPERR_NO_ERROR != device_->mapBuffer(npp_, context_, id, &np_buffer))
    return Buffer();

  Buffer buffer;
  buffer.ptr = np_buffer.ptr;
  buffer.size = np_buffer.size;
  return buffer;
}

void CommandBufferPepper::SetToken(int32 token) {
  // Not implemented by proxy.
  NOTREACHED();
}

void CommandBufferPepper::SetParseError(
    gpu::parse_error::ParseError parse_error) {
  // Not implemented by proxy.
  NOTREACHED();
}

CommandBuffer::State CommandBufferPepper::ConvertState() {
  CommandBuffer::State state;
  state.size = context_->commandBufferEntries;
  state.get_offset = context_->getOffset;
  state.put_offset = context_->putOffset;
  state.token = context_->token;
  state.error = static_cast<gpu::parse_error::ParseError>(
      context_->error);
  return state;
}
