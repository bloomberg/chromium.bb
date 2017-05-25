// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/command_buffer_service.h"

#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <memory>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/trace_event/trace_event.h"
#include "gpu/command_buffer/common/cmd_buffer_common.h"
#include "gpu/command_buffer/common/command_buffer_shared.h"
#include "gpu/command_buffer/service/transfer_buffer_manager.h"

using ::base::SharedMemory;

namespace gpu {

namespace {

class MemoryBufferBacking : public BufferBacking {
 public:
  explicit MemoryBufferBacking(size_t size)
      : memory_(new char[size]), size_(size) {}
  ~MemoryBufferBacking() override {}
  void* GetMemory() const override { return memory_.get(); }
  size_t GetSize() const override { return size_; }

 private:
  std::unique_ptr<char[]> memory_;
  size_t size_;
  DISALLOW_COPY_AND_ASSIGN(MemoryBufferBacking);
};

}  // anonymous namespace

CommandBufferService::CommandBufferService(
    TransferBufferManager* transfer_buffer_manager,
    AsyncAPIInterface* handler)
    : transfer_buffer_manager_(transfer_buffer_manager), handler_(handler) {
  state_.token = 0;
}

CommandBufferService::~CommandBufferService() {}

void CommandBufferService::UpdateState() {
  ++state_.generation;
  if (shared_state_)
    shared_state_->Write(state_);
}

void CommandBufferService::Flush(int32_t put_offset) {
  if (put_offset < 0 || put_offset >= num_entries_) {
    SetParseError(gpu::error::kOutOfBounds);
    return;
  }

  put_offset_ = put_offset;

  DCHECK(buffer_);

  if (state_.error != error::kNoError)
    return;

  DCHECK(scheduled());

  error::Error error = error::kNoError;
  handler_->BeginDecoding();
  while (put_offset_ != state_.get_offset) {
    if (PauseExecution())
      break;

    error = ProcessCommands(kParseCommandsSlice);

    if (error::IsError(error)) {
      SetParseError(error);
      break;
    }

    if (!command_processed_callback_.is_null())
      command_processed_callback_.Run();

    if (!scheduled())
      break;
  }

  handler_->EndDecoding();
}

void CommandBufferService::SetGetBuffer(int32_t transfer_buffer_id) {
  DCHECK_EQ(put_offset_, state_.get_offset);  // Only if it's empty.
  put_offset_ = 0;
  state_.get_offset = 0;
  ++state_.set_get_buffer_count;

  // If the buffer is invalid we handle it gracefully.
  // This means ring_buffer_ can be NULL.
  ring_buffer_ = GetTransferBuffer(transfer_buffer_id);
  ring_buffer_id_ = transfer_buffer_id;
  if (ring_buffer_) {
    int32_t size = ring_buffer_->size();
    volatile void* memory = ring_buffer_->memory();
    // check proper alignments.
    DCHECK_EQ(
        0u, (reinterpret_cast<intptr_t>(memory)) % alignof(CommandBufferEntry));
    DCHECK_EQ(0u, size % sizeof(CommandBufferEntry));

    num_entries_ = size / sizeof(CommandBufferEntry);
    buffer_ = reinterpret_cast<volatile CommandBufferEntry*>(memory);
  } else {
    num_entries_ = 0;
    buffer_ = nullptr;
  }

  UpdateState();
}

void CommandBufferService::SetSharedStateBuffer(
    std::unique_ptr<BufferBacking> shared_state_buffer) {
  shared_state_buffer_ = std::move(shared_state_buffer);
  DCHECK(shared_state_buffer_->GetSize() >= sizeof(*shared_state_));

  shared_state_ =
      static_cast<CommandBufferSharedState*>(shared_state_buffer_->GetMemory());

  UpdateState();
}

CommandBuffer::State CommandBufferService::GetState() {
  return state_;
}

void CommandBufferService::SetReleaseCount(uint64_t release_count) {
  DCHECK(release_count >= state_.release_count);
  state_.release_count = release_count;
  UpdateState();
}

scoped_refptr<Buffer> CommandBufferService::CreateTransferBuffer(size_t size,
                                                                 int32_t* id) {
  static int32_t next_id = 1;
  *id = next_id++;
  auto result = CreateTransferBufferWithId(size, *id);
  if (!result)
    *id = -1;
  return result;
}

void CommandBufferService::DestroyTransferBuffer(int32_t id) {
  transfer_buffer_manager_->DestroyTransferBuffer(id);
  if (id == ring_buffer_id_) {
    ring_buffer_id_ = -1;
    ring_buffer_ = nullptr;
    buffer_ = nullptr;
    num_entries_ = 0;
    state_.get_offset = 0;
    put_offset_ = 0;
  }
}

scoped_refptr<Buffer> CommandBufferService::GetTransferBuffer(int32_t id) {
  return transfer_buffer_manager_->GetTransferBuffer(id);
}

bool CommandBufferService::RegisterTransferBuffer(
    int32_t id,
    std::unique_ptr<BufferBacking> buffer) {
  return transfer_buffer_manager_->RegisterTransferBuffer(id,
                                                          std::move(buffer));
}

scoped_refptr<Buffer> CommandBufferService::CreateTransferBufferWithId(
    size_t size,
    int32_t id) {
  if (!RegisterTransferBuffer(id,
                              base::MakeUnique<MemoryBufferBacking>(size))) {
    SetParseError(gpu::error::kOutOfBounds);
    return nullptr;
  }

  return GetTransferBuffer(id);
}

void CommandBufferService::SetToken(int32_t token) {
  state_.token = token;
  UpdateState();
}

void CommandBufferService::SetParseError(error::Error error) {
  if (state_.error == error::kNoError) {
    state_.error = error;
    if (!parse_error_callback_.is_null())
      parse_error_callback_.Run();
  }
}

void CommandBufferService::SetContextLostReason(
    error::ContextLostReason reason) {
  state_.context_lost_reason = reason;
}

void CommandBufferService::SetPauseExecutionCallback(
    const PauseExecutionCallback& callback) {
  pause_execution_callback_ = callback;
}

void CommandBufferService::SetCommandProcessedCallback(
    const base::Closure& callback) {
  command_processed_callback_ = callback;
}

void CommandBufferService::SetParseErrorCallback(
    const base::Closure& callback) {
  parse_error_callback_ = callback;
}

void CommandBufferService::SetScheduled(bool scheduled) {
  TRACE_EVENT2("gpu", "CommandBufferService:SetScheduled", "this", this,
               "scheduled", scheduled);
  scheduled_ = scheduled;
}

bool CommandBufferService::PauseExecution() {
  if (pause_execution_callback_.is_null())
    return false;

  bool pause = pause_execution_callback_.Run();
  if (paused_ != pause) {
    TRACE_COUNTER_ID1("gpu", "CommandBufferService::Paused", this, pause);
    paused_ = pause;
  }
  return pause;
}

error::Error CommandBufferService::ProcessCommands(int num_commands) {
  int num_entries = put_offset_ < state_.get_offset
                        ? num_entries_ - state_.get_offset
                        : put_offset_ - state_.get_offset;

  int entries_processed = 0;
  error::Error result =
      handler_->DoCommands(num_commands, buffer_ + state_.get_offset,
                           num_entries, &entries_processed);

  state_.get_offset += entries_processed;
  if (state_.get_offset == num_entries_)
    state_.get_offset = 0;

  return result;
}

}  // namespace gpu
