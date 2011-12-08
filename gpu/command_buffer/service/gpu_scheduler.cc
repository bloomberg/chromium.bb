// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gpu_scheduler.h"

#include "base/bind.h"
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
namespace {
const uint64 kPollFencePeriod = 1;
}

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

  // Check that the GPU has passed all fences.
  if (!unschedule_fences_.empty()) {
    if (gfx::g_GL_NV_fence) {
      while (!unschedule_fences_.empty()) {
        if (glTestFenceNV(unschedule_fences_.front().fence)) {
          glDeleteFencesNV(1, &unschedule_fences_.front().fence);
          unschedule_fences_.front().task.Run();
          unschedule_fences_.pop();
        } else {
          SetScheduled(false);
          MessageLoop::current()->PostDelayedTask(
              FROM_HERE,
              base::Bind(&GpuScheduler::SetScheduled, AsWeakPtr(), true),
              kPollFencePeriod);
          return;
        }
      }
    } else {
      // Hopefully no recent drivers don't support GL_NV_fence and this will
      // not happen in practice.
      glFinish();

      while (!unschedule_fences_.empty()) {
        unschedule_fences_.front().task.Run();
        unschedule_fences_.pop();
      }
    }
  }

  // One of the unschedule fence tasks might have unscheduled us.
  if (!IsScheduled())
    return;

  error::Error error = error::kNoError;
  while (!parser_->IsEmpty()) {
    DCHECK(IsScheduled());
    DCHECK(unschedule_fences_.empty());

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

    if (!command_processed_callback_.is_null())
      command_processed_callback_.Run();

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

    if (unscheduled_count_ == 0 && !scheduled_callback_.is_null())
      scheduled_callback_.Run();
  } else {
    ++unscheduled_count_;
  }
}

bool GpuScheduler::IsScheduled() {
  return unscheduled_count_ == 0;
}

void GpuScheduler::SetScheduledCallback(
    const base::Closure& scheduled_callback) {
  scheduled_callback_ = scheduled_callback;
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
    const base::Closure& callback) {
  command_processed_callback_ = callback;
}

void GpuScheduler::DeferToFence(base::Closure task) {
  UnscheduleFence fence;

  // What if either of these GL calls fails? TestFenceNV will return true and
  // PutChanged will treat the fence as having been crossed and thereby not
  // poll indefinately. See spec:
  // http://www.opengl.org/registry/specs/NV/fence.txt
  //
  // What should happen if TestFenceNV is called for a name before SetFenceNV
  // is called?
  //     We generate an INVALID_OPERATION error, and return TRUE.
  //     This follows the semantics for texture object names before
  //     they are bound, in that they acquire their state upon binding.
  //     We will arbitrarily return TRUE for consistency.
  if (gfx::g_GL_NV_fence) {
    glGenFencesNV(1, &fence.fence);
    glSetFenceNV(fence.fence, GL_ALL_COMPLETED_NV);
  }

  glFlush();

  fence.task = task;

  unschedule_fences_.push(fence);
}

GpuScheduler::UnscheduleFence::UnscheduleFence() : fence(0) {
}

GpuScheduler::UnscheduleFence::~UnscheduleFence() {
}

}  // namespace gpu
