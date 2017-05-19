// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/command_executor.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/message_loop/message_loop.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "gpu/command_buffer/service/logger.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_fence.h"
#include "ui/gl/gl_switches.h"

using ::base::SharedMemory;

namespace gpu {

CommandExecutor::CommandExecutor(CommandBufferServiceBase* command_buffer,
                                 AsyncAPIInterface* handler,
                                 gles2::GLES2Decoder* decoder)
    : command_buffer_(command_buffer), handler_(handler), decoder_(decoder) {}

CommandExecutor::~CommandExecutor() {}

void CommandExecutor::PutChanged() {
  TRACE_EVENT1("gpu", "CommandExecutor:PutChanged", "decoder",
               decoder_ ? decoder_->GetLogger()->GetLogPrefix() : "None");

  CommandBuffer::State state = command_buffer_->GetLastState();

  put_ = command_buffer_->GetPutOffset();

  // If there is no buffer, exit.
  if (!buffer_) {
    DCHECK_EQ(state.get_offset, put_);
    return;
  }

  if (state.error != error::kNoError)
    return;

  error::Error error = error::kNoError;
  if (decoder_)
    decoder_->BeginDecoding();
  while (put_ != get_) {
    if (PauseExecution())
      break;

    DCHECK(scheduled());

    error = ProcessCommands(kParseCommandsSlice);

    if (error == error::kDeferCommandUntilLater) {
      DCHECK(!scheduled());
      break;
    }

    // TODO(piman): various classes duplicate various pieces of state, leading
    // to needlessly complex update logic. It should be possible to simply
    // share the state across all of them.
    command_buffer_->SetGetOffset(get_);

    if (error::IsError(error)) {
      if (decoder_)
        command_buffer_->SetContextLostReason(decoder_->GetContextLostReason());
      command_buffer_->SetParseError(error);
      break;
    }

    if (!command_processed_callback_.is_null())
      command_processed_callback_.Run();

    if (!scheduled())
      break;
  }

  if (decoder_) {
    if (!error::IsError(error) && decoder_->WasContextLost()) {
      command_buffer_->SetContextLostReason(decoder_->GetContextLostReason());
      command_buffer_->SetParseError(error::kLostContext);
    }
    decoder_->EndDecoding();
  }
}

void CommandExecutor::SetScheduled(bool scheduled) {
  TRACE_EVENT2("gpu", "CommandExecutor:SetScheduled", "this", this, "scheduled",
               scheduled);
  scheduled_ = scheduled;
}

bool CommandExecutor::HasPendingQueries() const {
  return (decoder_ && decoder_->HasPendingQueries());
}

void CommandExecutor::ProcessPendingQueries() {
  if (!decoder_)
    return;
  decoder_->ProcessPendingQueries(false);
}

scoped_refptr<Buffer> CommandExecutor::GetSharedMemoryBuffer(int32_t shm_id) {
  return command_buffer_->GetTransferBuffer(shm_id);
}

void CommandExecutor::set_token(int32_t token) {
  command_buffer_->SetToken(token);
}

bool CommandExecutor::SetGetBuffer(int32_t transfer_buffer_id) {
  scoped_refptr<Buffer> ring_buffer =
      command_buffer_->GetTransferBuffer(transfer_buffer_id);
  if (!ring_buffer)
    return false;

  volatile void* memory = ring_buffer->memory();
  size_t size = ring_buffer->size();
  // check proper alignments.
  DCHECK_EQ(0, (reinterpret_cast<intptr_t>(memory)) % 4);
  DCHECK_EQ(0u, size % 4);
  get_ = 0;
  put_ = 0;
  buffer_ = reinterpret_cast<volatile CommandBufferEntry*>(memory);
  entry_count_ = size / 4;

  command_buffer_->SetGetOffset(get_);
  return true;
}

void CommandExecutor::SetCommandProcessedCallback(
    const base::Closure& callback) {
  command_processed_callback_ = callback;
}

void CommandExecutor::SetPauseExecutionCallback(
    const PauseExecutionCallback& callback) {
  pause_execution_callback_ = callback;
}

bool CommandExecutor::PauseExecution() {
  if (pause_execution_callback_.is_null())
    return false;

  bool pause = pause_execution_callback_.Run();
  if (paused_ != pause) {
    TRACE_COUNTER_ID1("gpu", "CommandExecutor::Paused", this, pause);
    paused_ = pause;
  }
  return pause;
}

bool CommandExecutor::HasMoreIdleWork() const {
  return (decoder_ && decoder_->HasMoreIdleWork());
}

void CommandExecutor::PerformIdleWork() {
  if (!decoder_)
    return;
  decoder_->PerformIdleWork();
}

bool CommandExecutor::HasPollingWork() const {
  return (decoder_ && decoder_->HasPollingWork());
}

void CommandExecutor::PerformPollingWork() {
  if (!decoder_)
    return;
  decoder_->PerformPollingWork();
}

error::Error CommandExecutor::ProcessCommands(int num_commands) {
  int num_entries = put_ < get_ ? entry_count_ - get_ : put_ - get_;
  int entries_processed = 0;

  error::Error result = handler_->DoCommands(num_commands, buffer_ + get_,
                                             num_entries, &entries_processed);

  get_ += entries_processed;
  if (get_ == entry_count_)
    get_ = 0;

  return result;
}

}  // namespace gpu
