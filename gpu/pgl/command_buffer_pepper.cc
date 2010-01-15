// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/pgl/command_buffer_pepper.h"
#include "base/logging.h"

using base::SharedMemory;
using gpu::Buffer;

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

int32 CommandBufferPepper::GetSize() {
  return context_->commandBufferEntries;
}

int32 CommandBufferPepper::SyncOffsets(int32 put_offset) {
  context_->putOffset = put_offset;
  if (NPERR_NO_ERROR != device_->flushContext(npp_, context_, NULL, NULL))
    return -1;

  return context_->getOffset;
}

int32 CommandBufferPepper::GetGetOffset() {
  int32 value;
  if (NPERR_NO_ERROR != device_->getStateContext(
      npp_,
      context_,
      NPDeviceContext3DState_GetOffset,
      &value)) {
    return -1;
  }

  return value;
}

void CommandBufferPepper::SetGetOffset(int32 get_offset) {
  // Not implemented by proxy.
  NOTREACHED();
}

int32 CommandBufferPepper::GetPutOffset() {
  int32 value;
  if (NPERR_NO_ERROR != device_->getStateContext(
      npp_,
      context_,
      NPDeviceContext3DState_PutOffset,
      &value)) {
    return -1;
  }

  return value;
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

int32 CommandBufferPepper::GetToken() {
  int32 value;
  if (NPERR_NO_ERROR != device_->getStateContext(
      npp_,
      context_,
      NPDeviceContext3DState_Token,
      &value)) {
    return -1;
  }

  return value;
}

void CommandBufferPepper::SetToken(int32 token) {
  // Not implemented by proxy.
  NOTREACHED();
}

int32 CommandBufferPepper::ResetParseError() {
  int32 value;
  if (NPERR_NO_ERROR != device_->getStateContext(
      npp_,
      context_,
      NPDeviceContext3DState_ParseError,
      &value)) {
    return -1;
  }

  return value;
}

void CommandBufferPepper::SetParseError(int32 parse_error) {
  // Not implemented by proxy.
  NOTREACHED();
}

bool CommandBufferPepper::GetErrorStatus() {
  int32 value;
  if (NPERR_NO_ERROR != device_->getStateContext(
      npp_,
      context_,
      NPDeviceContext3DState_ErrorStatus,
      &value)) {
    return value != 0;
  }

  return true;
}

void CommandBufferPepper::RaiseErrorStatus() {
  // Not implemented by proxy.
  NOTREACHED();
}
