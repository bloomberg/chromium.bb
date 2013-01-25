// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/command_buffer_service.h"

#include <limits>

#include "base/process_util.h"
#include "base/debug/trace_event.h"
#include "gpu/command_buffer/common/cmd_buffer_common.h"
#include "gpu/command_buffer/common/command_buffer_shared.h"
#include "gpu/command_buffer/service/transfer_buffer_manager.h"

using ::base::SharedMemory;

namespace gpu {

CommandBufferService::CommandBufferService(
    TransferBufferManagerInterface* transfer_buffer_manager)
    : ring_buffer_id_(-1),
      shared_state_(NULL),
      num_entries_(0),
      get_offset_(0),
      put_offset_(0),
      transfer_buffer_manager_(transfer_buffer_manager),
      token_(0),
      generation_(0),
      error_(error::kNoError),
      context_lost_reason_(error::kUnknown) {
}

CommandBufferService::~CommandBufferService() {
}

bool CommandBufferService::Initialize() {
  return true;
}

CommandBufferService::State CommandBufferService::GetState() {
  State state;
  state.num_entries = num_entries_;
  state.get_offset = get_offset_;
  state.put_offset = put_offset_;
  state.token = token_;
  state.error = error_;
  state.context_lost_reason = context_lost_reason_;
  state.generation = ++generation_;

  return state;
}

CommandBufferService::State CommandBufferService::GetLastState() {
  return GetState();
}

int32 CommandBufferService::GetLastToken() {
  return GetState().token;
}

void CommandBufferService::UpdateState() {
  if (shared_state_) {
    CommandBufferService::State state = GetState();
    shared_state_->Write(state);
  }
}

CommandBufferService::State CommandBufferService::FlushSync(
    int32 put_offset, int32 last_known_get) {
  if (put_offset < 0 || put_offset > num_entries_) {
    error_ = gpu::error::kOutOfBounds;
    return GetState();
  }

  put_offset_ = put_offset;

  if (!put_offset_change_callback_.is_null())
    put_offset_change_callback_.Run();

  return GetState();
}

void CommandBufferService::Flush(int32 put_offset) {
  if (put_offset < 0 || put_offset > num_entries_) {
    error_ = gpu::error::kOutOfBounds;
    return;
  }

  put_offset_ = put_offset;

  if (!put_offset_change_callback_.is_null())
    put_offset_change_callback_.Run();
}

void CommandBufferService::SetGetBuffer(int32 transfer_buffer_id) {
  DCHECK_EQ(-1, ring_buffer_id_);
  DCHECK_EQ(put_offset_, get_offset_);  // Only if it's empty.
  ring_buffer_ = GetTransferBuffer(transfer_buffer_id);
  DCHECK(ring_buffer_.ptr);
  ring_buffer_id_ = transfer_buffer_id;
  num_entries_ = ring_buffer_.size / sizeof(CommandBufferEntry);
  put_offset_ = 0;
  SetGetOffset(0);
  if (!get_buffer_change_callback_.is_null()) {
    get_buffer_change_callback_.Run(ring_buffer_id_);
  }

  UpdateState();
}

bool CommandBufferService::SetSharedStateBuffer(
    scoped_ptr<base::SharedMemory> shared_state_shm) {
  shared_state_shm_.reset(shared_state_shm.release());
  if (!shared_state_shm_->Map(sizeof(*shared_state_)))
    return false;

  shared_state_ =
      static_cast<CommandBufferSharedState*>(shared_state_shm_->memory());

  UpdateState();
  return true;
}

void CommandBufferService::SetGetOffset(int32 get_offset) {
  DCHECK(get_offset >= 0 && get_offset < num_entries_);
  get_offset_ = get_offset;
}

Buffer CommandBufferService::CreateTransferBuffer(size_t size,
                                                  int32* id) {
  *id = -1;

  SharedMemory buffer;
  if (!buffer.CreateAnonymous(size))
    return Buffer();

  static int32 next_id = 1;
  *id = next_id++;

  if (!RegisterTransferBuffer(*id, &buffer, size))
    return Buffer();

  return GetTransferBuffer(*id);
}

void CommandBufferService::DestroyTransferBuffer(int32 id) {
  transfer_buffer_manager_->DestroyTransferBuffer(id);
  if (id == ring_buffer_id_) {
    ring_buffer_id_ = -1;
    ring_buffer_ = Buffer();
    num_entries_ = 0;
    get_offset_ = 0;
    put_offset_ = 0;
  }
}

Buffer CommandBufferService::GetTransferBuffer(int32 id) {
  return transfer_buffer_manager_->GetTransferBuffer(id);
}

bool CommandBufferService::RegisterTransferBuffer(
    int32 id,
    base::SharedMemory* shared_memory,
    size_t size) {
  return transfer_buffer_manager_->RegisterTransferBuffer(id,
                                                          shared_memory,
                                                          size);
}

void CommandBufferService::SetToken(int32 token) {
  token_ = token;
  UpdateState();
}

void CommandBufferService::SetParseError(error::Error error) {
  if (error_ == error::kNoError) {
    error_ = error;
    if (!parse_error_callback_.is_null())
      parse_error_callback_.Run();
  }
}

void CommandBufferService::SetContextLostReason(
    error::ContextLostReason reason) {
  context_lost_reason_ = reason;
}

void CommandBufferService::SetPutOffsetChangeCallback(
    const base::Closure& callback) {
  put_offset_change_callback_ = callback;
}

void CommandBufferService::SetGetBufferChangeCallback(
    const GetBufferChangedCallback& callback) {
  get_buffer_change_callback_ = callback;
}

void CommandBufferService::SetParseErrorCallback(
    const base::Closure& callback) {
  parse_error_callback_ = callback;
}

}  // namespace gpu
