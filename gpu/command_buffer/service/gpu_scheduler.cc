// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gpu_scheduler.h"

#include "base/callback.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/debug/trace_event.h"
#include "base/message_loop.h"
#include "base/time.h"
#include "ui/gfx/gl/gl_context.h"
#include "ui/gfx/gl/gl_bindings.h"
#include "ui/gfx/gl/gl_surface.h"
#include "ui/gfx/gl/gl_switches.h"

using ::base::SharedMemory;

namespace gpu {

GpuScheduler::GpuScheduler(CommandBuffer* command_buffer,
                           gles2::GLES2Decoder* decoder,
                           CommandParser* parser)
    : command_buffer_(command_buffer),
      decoder_(decoder),
      parser_(parser),
      unscheduled_count_(0) {
  // Map the ring buffer and create the parser.
  if (!parser) {
    Buffer ring_buffer = command_buffer_->GetRingBuffer();
    if (ring_buffer.ptr) {
      parser_.reset(new CommandParser(ring_buffer.ptr,
                                      ring_buffer.size,
                                      0,
                                      ring_buffer.size,
                                      0,
                                      decoder_));
    } else {
      parser_.reset(new CommandParser(NULL, 0, 0, 0, 0,
                                      decoder_));
    }
  }
}

GpuScheduler::~GpuScheduler() {
}

void GpuScheduler::PutChanged() {
  TRACE_EVENT1("gpu", "GpuScheduler:PutChanged", "this", this);

  DCHECK(IsScheduled());

  CommandBuffer::State state = command_buffer_->GetState();
  parser_->set_put(state.put_offset);
  if (state.error != error::kNoError)
    return;

  error::Error error = error::kNoError;
  while (!parser_->IsEmpty()) {
    error = parser_->ProcessCommand();

    // TODO(piman): various classes duplicate various pieces of state, leading
    // to needlessly complex update logic. It should be possible to simply
    // share the state across all of them.
    command_buffer_->SetGetOffset(static_cast<int32>(parser_->get()));

    if (error::IsError(error)) {
      command_buffer_->SetContextLostReason(decoder_->GetContextLostReason());
      command_buffer_->SetParseError(error);
      return;
    }

    if (command_processed_callback_.get())
      command_processed_callback_->Run();

    if (unscheduled_count_ > 0)
      return;
  }
}

void GpuScheduler::SetScheduled(bool scheduled) {
  TRACE_EVENT2("gpu", "GpuScheduler:SetScheduled", "this", this,
               "new unscheduled_count_",
               unscheduled_count_ + (scheduled? -1 : 1));
  if (scheduled) {
    --unscheduled_count_;
    DCHECK_GE(unscheduled_count_, 0);

    if (unscheduled_count_ == 0 && scheduled_callback_.get())
      scheduled_callback_->Run();
  } else {
    ++unscheduled_count_;
  }
}

bool GpuScheduler::IsScheduled() {
  return unscheduled_count_ == 0;
}

void GpuScheduler::SetScheduledCallback(Callback0::Type* scheduled_callback) {
  scheduled_callback_.reset(scheduled_callback);
}

Buffer GpuScheduler::GetSharedMemoryBuffer(int32 shm_id) {
  return command_buffer_->GetTransferBuffer(shm_id);
}

void GpuScheduler::set_token(int32 token) {
  command_buffer_->SetToken(token);
}

bool GpuScheduler::SetGetOffset(int32 offset) {
  if (parser_->set_get(offset)) {
    command_buffer_->SetGetOffset(static_cast<int32>(parser_->get()));
    return true;
  }
  return false;
}

int32 GpuScheduler::GetGetOffset() {
  return parser_->get();
}

void GpuScheduler::SetCommandProcessedCallback(
    Callback0::Type* callback) {
  command_processed_callback_.reset(callback);
}

}  // namespace gpu
