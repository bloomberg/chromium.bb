// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gpu_scheduler.h"

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/debug/trace_event.h"
#include "base/message_loop.h"
#include "ui/gfx/gl/gl_context.h"
#include "ui/gfx/gl/gl_bindings.h"

using ::base::SharedMemory;

namespace gpu {

GpuScheduler::GpuScheduler(CommandBuffer* command_buffer,
                           gles2::ContextGroup* group)
    : command_buffer_(command_buffer),
      commands_per_update_(100),
      unscheduled_count_(0),
#if defined(OS_MACOSX)
      swap_buffers_count_(0),
      acknowledged_swap_buffers_count_(0),
#endif
      method_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  DCHECK(command_buffer);
  decoder_.reset(gles2::GLES2Decoder::Create(group));
  decoder_->set_engine(this);
}

GpuScheduler::GpuScheduler(CommandBuffer* command_buffer,
                           gles2::GLES2Decoder* decoder,
                           CommandParser* parser,
                           int commands_per_update)
    : command_buffer_(command_buffer),
      commands_per_update_(commands_per_update),
      unscheduled_count_(0),
#if defined(OS_MACOSX)
      swap_buffers_count_(0),
      acknowledged_swap_buffers_count_(0),
#endif
      method_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  DCHECK(command_buffer);
  decoder_.reset(decoder);
  parser_.reset(parser);
}

GpuScheduler::~GpuScheduler() {
  Destroy();
}

bool GpuScheduler::InitializeCommon(
    gfx::GLContext* context,
    const gfx::Size& size,
    const gles2::DisallowedExtensions& disallowed_extensions,
    const char* allowed_extensions,
    const std::vector<int32>& attribs,
    gles2::GLES2Decoder* parent_decoder,
    uint32 parent_texture_id) {
  DCHECK(context);

  if (!context->MakeCurrent())
    return false;

  // Do not limit to a certain number of commands before scheduling another
  // update when rendering onscreen.
  if (!context->IsOffscreen())
    commands_per_update_ = INT_MAX;

  // Map the ring buffer and create the parser.
  Buffer ring_buffer = command_buffer_->GetRingBuffer();
  if (ring_buffer.ptr) {
    parser_.reset(new CommandParser(ring_buffer.ptr,
                                    ring_buffer.size,
                                    0,
                                    ring_buffer.size,
                                    0,
                                    decoder_.get()));
  } else {
    parser_.reset(new CommandParser(NULL, 0, 0, 0, 0,
                                    decoder_.get()));
  }

  // Initialize the decoder with either the view or pbuffer GLContext.
  if (!decoder_->Initialize(context,
                            size,
                            disallowed_extensions,
                            allowed_extensions,
                            attribs,
                            parent_decoder,
                            parent_texture_id)) {
    LOG(ERROR) << "GpuScheduler::InitializeCommon failed because decoder "
               << "failed to initialize.";
    Destroy();
    return false;
  }

  return true;
}

void GpuScheduler::DestroyCommon() {
  bool have_context = false;
  if (decoder_.get()) {
    have_context = decoder_->MakeCurrent();
    decoder_->Destroy();
    decoder_.reset();
  }

  parser_.reset();
}

#if defined(OS_MACOSX)
namespace {
const unsigned int kMaxOutstandingSwapBuffersCallsPerOnscreenContext = 1;
}
#endif

void GpuScheduler::PutChanged(bool sync) {
  CommandBuffer::State state = command_buffer_->GetState();
  parser_->set_put(state.put_offset);

  if (sync)
    ProcessCommands();
  else
    ScheduleProcessCommands();
}

void GpuScheduler::ProcessCommands() {
  TRACE_EVENT0("gpu", "GpuScheduler:ProcessCommands");
  CommandBuffer::State state = command_buffer_->GetState();
  if (state.error != error::kNoError)
    return;

  if (unscheduled_count_ > 0)
    return;

  if (decoder_.get()) {
    if (!decoder_->MakeCurrent()) {
      LOG(ERROR) << "Context lost because MakeCurrent failed.";
      command_buffer_->SetParseError(error::kLostContext);
      return;
    }
  }

#if defined(OS_MACOSX)
  bool do_rate_limiting = surface_.get() != NULL;
  // Don't swamp the browser process with SwapBuffers calls it can't handle.
  if (do_rate_limiting &&
      swap_buffers_count_ - acknowledged_swap_buffers_count_ >=
      kMaxOutstandingSwapBuffersCallsPerOnscreenContext) {
    // Stop doing work on this command buffer. In the GPU process,
    // receipt of the GpuMsg_AcceleratedSurfaceBuffersSwappedACK
    // message causes ProcessCommands to be scheduled again.
    return;
  }
#endif

  error::Error error = error::kNoError;
  int commands_processed = 0;
  while (commands_processed < commands_per_update_ &&
         !parser_->IsEmpty()) {
    error = parser_->ProcessCommand();

    // TODO(piman): various classes duplicate various pieces of state, leading
    // to needlessly complex update logic. It should be possible to simply share
    // the state across all of them.
    command_buffer_->SetGetOffset(static_cast<int32>(parser_->get()));

    if (error == error::kWaiting || error == error::kYield) {
      break;
    } else if (error::IsError(error)) {
      command_buffer_->SetParseError(error);
      return;
    }

    if (unscheduled_count_ > 0)
      break;

    ++commands_processed;
    if (command_processed_callback_.get()) {
      command_processed_callback_->Run();
    }
  }

  if (unscheduled_count_ == 0 &&
      error != error::kWaiting &&
      !parser_->IsEmpty()) {
    ScheduleProcessCommands();
  }
}

void GpuScheduler::SetScheduled(bool scheduled) {
  if (scheduled) {
    --unscheduled_count_;
    DCHECK_GE(unscheduled_count_, 0);

    if (unscheduled_count_ == 0) {
      if (scheduled_callback_.get())
        scheduled_callback_->Run();

      ScheduleProcessCommands();
    }
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

void GpuScheduler::ResizeOffscreenFrameBuffer(const gfx::Size& size) {
  decoder_->ResizeOffscreenFrameBuffer(size);
}

void GpuScheduler::SetResizeCallback(Callback1<gfx::Size>::Type* callback) {
  decoder_->SetResizeCallback(callback);
}

void GpuScheduler::SetSwapBuffersCallback(
    Callback0::Type* callback) {
  wrapped_swap_buffers_callback_.reset(callback);
  decoder_->SetSwapBuffersCallback(
      NewCallback(this,
                  &GpuScheduler::WillSwapBuffers));
}

void GpuScheduler::SetCommandProcessedCallback(
    Callback0::Type* callback) {
  command_processed_callback_.reset(callback);
}

void GpuScheduler::ScheduleProcessCommands() {
  MessageLoop::current()->PostTask(
      FROM_HERE,
      method_factory_.NewRunnableMethod(&GpuScheduler::ProcessCommands));
}

}  // namespace gpu
