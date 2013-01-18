// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the implementation of the command buffer helper class.

#include "../client/cmd_buffer_helper.h"
#include "../common/command_buffer.h"
#include "../common/trace_event.h"

namespace gpu {

namespace {
const int kCommandsPerFlushCheck = 100;
const double kFlushDelay = 1.0 / (5.0 * 60.0);
}

CommandBufferHelper::CommandBufferHelper(CommandBuffer* command_buffer)
    : command_buffer_(command_buffer),
      ring_buffer_id_(-1),
      ring_buffer_size_(0),
      entries_(NULL),
      total_entry_count_(0),
      token_(0),
      put_(0),
      last_put_sent_(0),
      commands_issued_(0),
      usable_(true),
      context_lost_(false),
      flush_automatically_(true),
      last_flush_time_(0) {
}

void CommandBufferHelper::SetAutomaticFlushes(bool enabled) {
  flush_automatically_ = enabled;
}

bool CommandBufferHelper::IsContextLost() {
  if (!context_lost_) {
    context_lost_ = error::IsError(command_buffer()->GetLastError());
  }
  return context_lost_;
}

bool CommandBufferHelper::AllocateRingBuffer() {
  if (!usable()) {
    return false;
  }

  if (HaveRingBuffer()) {
    return true;
  }

  int32 id = -1;
  Buffer buffer = command_buffer_->CreateTransferBuffer(ring_buffer_size_, &id);
  if (id < 0) {
    ClearUsable();
    return false;
  }

  ring_buffer_ = buffer;
  ring_buffer_id_ = id;
  command_buffer_->SetGetBuffer(id);

  // TODO(gman): Do we really need to call GetState here? We know get & put = 0
  // Also do we need to check state.num_entries?
  CommandBuffer::State state = command_buffer_->GetState();
  entries_ = static_cast<CommandBufferEntry*>(ring_buffer_.ptr);
  int32 num_ring_buffer_entries =
      ring_buffer_size_ / sizeof(CommandBufferEntry);
  if (num_ring_buffer_entries > state.num_entries) {
    ClearUsable();
    return false;
  }

  total_entry_count_ = num_ring_buffer_entries;
  put_ = state.put_offset;
  return true;
}

void CommandBufferHelper::FreeResources() {
  if (HaveRingBuffer()) {
    command_buffer_->DestroyTransferBuffer(ring_buffer_id_);
    ring_buffer_id_ = -1;
  }
}

void CommandBufferHelper::FreeRingBuffer() {
  GPU_CHECK((put_ == get_offset()) ||
      error::IsError(command_buffer_->GetLastState().error));
  FreeResources();
}

bool CommandBufferHelper::Initialize(int32 ring_buffer_size) {
  ring_buffer_size_ = ring_buffer_size;
  return AllocateRingBuffer();
}

CommandBufferHelper::~CommandBufferHelper() {
  FreeResources();
}

bool CommandBufferHelper::FlushSync() {
  if (!usable()) {
    return false;
  }
  last_flush_time_ = clock();
  last_put_sent_ = put_;
  CommandBuffer::State state = command_buffer_->FlushSync(put_, get_offset());
  return state.error == error::kNoError;
}

void CommandBufferHelper::Flush() {
  if (usable()) {
    last_flush_time_ = clock();
    last_put_sent_ = put_;
    command_buffer_->Flush(put_);
  }
}

// Calls Flush() and then waits until the buffer is empty. Break early if the
// error is set.
bool CommandBufferHelper::Finish() {
  TRACE_EVENT0("gpu", "CommandBufferHelper::Finish");
  if (!usable()) {
    return false;
  }
  // If there is no work just exit.
  if (put_ == get_offset()) {
    return true;
  }
  GPU_DCHECK(HaveRingBuffer());
  do {
    // Do not loop forever if the flush fails, meaning the command buffer reader
    // has shutdown.
    if (!FlushSync())
      return false;
  } while (put_ != get_offset());

  return true;
}

// Inserts a new token into the command stream. It uses an increasing value
// scheme so that we don't lose tokens (a token has passed if the current token
// value is higher than that token). Calls Finish() if the token value wraps,
// which will be rare.
int32 CommandBufferHelper::InsertToken() {
  AllocateRingBuffer();
  if (!usable()) {
    return token_;
  }
  GPU_DCHECK(HaveRingBuffer());
  // Increment token as 31-bit integer. Negative values are used to signal an
  // error.
  token_ = (token_ + 1) & 0x7FFFFFFF;
  cmd::SetToken* cmd = GetCmdSpace<cmd::SetToken>();
  if (cmd) {
    cmd->Init(token_);
    if (token_ == 0) {
      TRACE_EVENT0("gpu", "CommandBufferHelper::InsertToken(wrapped)");
      // we wrapped
      Finish();
      GPU_DCHECK_EQ(token_, last_token_read());
    }
  }
  return token_;
}

// Waits until the current token value is greater or equal to the value passed
// in argument.
void CommandBufferHelper::WaitForToken(int32 token) {
  if (!usable() || !HaveRingBuffer()) {
    return;
  }
  // Return immediately if corresponding InsertToken failed.
  if (token < 0)
    return;
  if (token > token_) return;  // we wrapped
  while (last_token_read() < token) {
    if (get_offset() == put_) {
      GPU_LOG(FATAL) << "Empty command buffer while waiting on a token.";
      return;
    }
    // Do not loop forever if the flush fails, meaning the command buffer reader
    // has shutdown.
    if (!FlushSync())
      return;
  }
}

// Waits for available entries, basically waiting until get >= put + count + 1.
// It actually waits for contiguous entries, so it may need to wrap the buffer
// around, adding a noops. Thus this function may change the value of put_. The
// function will return early if an error occurs, in which case the available
// space may not be available.
void CommandBufferHelper::WaitForAvailableEntries(int32 count) {
  AllocateRingBuffer();
  if (!usable()) {
    return;
  }
  GPU_DCHECK(HaveRingBuffer());
  GPU_DCHECK(count < total_entry_count_);
  if (put_ + count > total_entry_count_) {
    // There's not enough room between the current put and the end of the
    // buffer, so we need to wrap. We will add noops all the way to the end,
    // but we need to make sure get wraps first, actually that get is 1 or
    // more (since put will wrap to 0 after we add the noops).
    GPU_DCHECK_LE(1, put_);
    if (get_offset() > put_ || get_offset() == 0) {
      TRACE_EVENT0("gpu", "CommandBufferHelper::WaitForAvailableEntries");
      while (get_offset() > put_ || get_offset() == 0) {
        // Do not loop forever if the flush fails, meaning the command buffer
        // reader has shutdown.
        if (!FlushSync())
          return;
      }
    }
    // Insert Noops to fill out the buffer.
    int32 num_entries = total_entry_count_ - put_;
    while (num_entries > 0) {
      int32 num_to_skip = std::min(CommandHeader::kMaxSize, num_entries);
      cmd::Noop::Set(&entries_[put_], num_to_skip);
      put_ += num_to_skip;
      num_entries -= num_to_skip;
    }
    put_ = 0;
  }
  if (AvailableEntries() < count) {
    TRACE_EVENT0("gpu", "CommandBufferHelper::WaitForAvailableEntries1");
    while (AvailableEntries() < count) {
      // Do not loop forever if the flush fails, meaning the command buffer
      // reader has shutdown.
      if (!FlushSync())
        return;
    }
  }
  // Force a flush if the buffer is getting half full, or even earlier if the
  // reader is known to be idle.
  int32 pending =
      (put_ + total_entry_count_ - last_put_sent_) % total_entry_count_;
  int32 limit = total_entry_count_ /
      ((get_offset() == last_put_sent_) ? 16 : 2);
  if (pending > limit) {
    Flush();
  } else if (flush_automatically_ &&
             (commands_issued_ % kCommandsPerFlushCheck == 0)) {
#if !defined(OS_ANDROID)
    // Allow this command buffer to be pre-empted by another if a "reasonable"
    // amount of work has been done. On highend machines, this reduces the
    // latency of GPU commands. However, on Android, this can cause the
    // kernel to thrash between generating GPU commands and executing them.
    clock_t current_time = clock();
    if (current_time - last_flush_time_ > kFlushDelay * CLOCKS_PER_SEC)
      Flush();
#endif
  }
}

CommandBufferEntry* CommandBufferHelper::GetSpace(uint32 entries) {
  AllocateRingBuffer();
  if (!usable()) {
    return NULL;
  }
  GPU_DCHECK(HaveRingBuffer());
  ++commands_issued_;
  WaitForAvailableEntries(entries);
  CommandBufferEntry* space = &entries_[put_];
  put_ += entries;
  GPU_DCHECK_LE(put_, total_entry_count_);
  if (put_ == total_entry_count_) {
    put_ = 0;
  }
  return space;
}

error::Error CommandBufferHelper::GetError() {
  CommandBuffer::State state = command_buffer_->GetState();
  return static_cast<error::Error>(state.error);
}

}  // namespace gpu
