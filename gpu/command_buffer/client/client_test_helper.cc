// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests for GLES2Implementation.

#include "gpu/command_buffer/client/client_test_helper.h"

#include "gpu/command_buffer/common/command_buffer.h"
#include "gpu/command_buffer/client/cmd_buffer_helper.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::_;
using ::testing::Invoke;

namespace gpu {

MockCommandBufferBase::~MockCommandBufferBase() {
}

bool MockCommandBufferBase::Initialize() {
  return true;
}

CommandBuffer::State MockCommandBufferBase::GetState() {
  return state_;
}

CommandBuffer::State MockCommandBufferBase::GetLastState() {
  return state_;
}

int32 MockCommandBufferBase::GetLastToken() {
  return state_.token;
}

void MockCommandBufferBase::SetGetOffset(int32 get_offset) {
  state_.get_offset = get_offset;
}

CommandBuffer::State MockCommandBufferBase::FlushSync(
    int32 put_offset, int32 last_known_get) {
  state_.put_offset = put_offset;
  state_.get_offset = put_offset;
  OnFlush();
  return state_;
}

void MockCommandBufferBase::SetGetBuffer(int transfer_buffer_id) {
  ring_buffer_buffer_ = GetTransferBuffer(transfer_buffer_id);
  ring_buffer_ = static_cast<CommandBufferEntry*>(ring_buffer_buffer_.ptr);
  state_.num_entries = ring_buffer_buffer_.size / sizeof(ring_buffer_[0]);
  state_.token = 10000;  // All token checks in the tests should pass.
}

// Get's the Id of the next transfer buffer that will be returned
// by CreateTransferBuffer. This is useful for testing expected ids.
int32 MockCommandBufferBase::GetNextFreeTransferBufferId() {
  for (size_t ii = 0; ii < arraysize(transfer_buffers_); ++ii) {
    if (!transfer_buffers_[ii].get()) {
      return kTransferBufferBaseId + ii;
    }
  }
  return -1;
}

Buffer MockCommandBufferBase::CreateTransferBuffer(size_t size, int32* id) {
  *id = GetNextFreeTransferBufferId();
  if (*id >= 0) {
    int32 ndx = *id - kTransferBufferBaseId;
    transfer_buffers_[ndx].reset(new int8[size]);
    transfer_buffer_buffers_[ndx].ptr = transfer_buffers_[ndx].get();
    transfer_buffer_buffers_[ndx].size = size;
  }
  return GetTransferBuffer(*id);
}

void MockCommandBufferBase::DestroyTransferBufferHelper(int32 id) {
  GPU_DCHECK_GE(id, kTransferBufferBaseId);
  GPU_DCHECK_LT(id, kTransferBufferBaseId + kMaxTransferBuffers);
  id -= kTransferBufferBaseId;
  transfer_buffers_[id].reset();
  transfer_buffer_buffers_[id] = Buffer();
}

Buffer MockCommandBufferBase::GetTransferBuffer(int32 id) {
  GPU_DCHECK_GE(id, kTransferBufferBaseId);
  GPU_DCHECK_LT(id, kTransferBufferBaseId + kMaxTransferBuffers);
  return transfer_buffer_buffers_[id - kTransferBufferBaseId];
}

void MockCommandBufferBase::FlushHelper(int32 put_offset) {
  state_.put_offset = put_offset;
}

void MockCommandBufferBase::SetToken(int32 token) {
  GPU_NOTREACHED();
  state_.token = token;
}

void MockCommandBufferBase::SetParseError(error::Error error) {
  GPU_NOTREACHED();
  state_.error = error;
}

void MockCommandBufferBase::SetContextLostReason(
    error::ContextLostReason reason) {
  GPU_NOTREACHED();
  state_.context_lost_reason = reason;
}

// GCC requires these declarations, but MSVC requires they not be present
#ifndef _MSC_VER
const int32 MockCommandBufferBase::kTransferBufferBaseId;
const int32 MockCommandBufferBase::kMaxTransferBuffers;
#endif

MockClientCommandBuffer::MockClientCommandBuffer() {
  DelegateToFake();
}

MockClientCommandBuffer::~MockClientCommandBuffer() {
}

void MockClientCommandBuffer::Flush(int32 put_offset) {
  FlushHelper(put_offset);
}

void MockClientCommandBuffer::DelegateToFake() {
  ON_CALL(*this, DestroyTransferBuffer(_))
      .WillByDefault(Invoke(
          this, &MockCommandBufferBase::DestroyTransferBufferHelper));
}

MockClientCommandBufferMockFlush::MockClientCommandBufferMockFlush() {
  DelegateToFake();
}

MockClientCommandBufferMockFlush::~MockClientCommandBufferMockFlush() {
}

void MockClientCommandBufferMockFlush::DelegateToFake() {
  MockClientCommandBuffer::DelegateToFake();
  ON_CALL(*this, Flush(_))
      .WillByDefault(Invoke(
          this, &MockCommandBufferBase::FlushHelper));
}

}  // namespace gpu


