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

GpuScheduler* GpuScheduler::Create(CommandBuffer* command_buffer,
                                   SurfaceManager* surface_manager,
                                   gles2::ContextGroup* group) {
  DCHECK(command_buffer);

  gles2::GLES2Decoder* decoder =
      gles2::GLES2Decoder::Create(surface_manager, group);

  GpuScheduler* scheduler = new GpuScheduler(command_buffer,
                                             decoder,
                                             NULL);

  decoder->set_engine(scheduler);

  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableGPUServiceLogging)) {
    decoder->set_debug(true);
  }

  return scheduler;
}

GpuScheduler* GpuScheduler::CreateForTests(CommandBuffer* command_buffer,
                                           gles2::GLES2Decoder* decoder,
                                           CommandParser* parser) {
  DCHECK(command_buffer);
  GpuScheduler* scheduler = new GpuScheduler(command_buffer,
                                             decoder,
                                             parser);

  return scheduler;
}

GpuScheduler::~GpuScheduler() {
  Destroy();
}

bool GpuScheduler::InitializeCommon(
    const scoped_refptr<gfx::GLSurface>& surface,
    const scoped_refptr<gfx::GLContext>& context,
    const gfx::Size& size,
    const gles2::DisallowedExtensions& disallowed_extensions,
    const char* allowed_extensions,
    const std::vector<int32>& attribs) {
  DCHECK(context);

  if (!context->MakeCurrent(surface))
    return false;

#if !defined(OS_MACOSX) && !defined(TOUCH_UI)
  // Set up swap interval for onscreen contexts.
  if (!surface->IsOffscreen()) {
    if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kDisableGpuVsync))
      context->SetSwapInterval(0);
    else
      context->SetSwapInterval(1);
  }
#endif

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
  // TODO(apatrick): The GpuScheduler should know nothing about the surface the
  // decoder is rendering to. Get rid of the surface parameter.
  if (!decoder_->Initialize(surface,
                            context,
                            size,
                            disallowed_extensions,
                            allowed_extensions,
                            attribs)) {
    LOG(ERROR) << "GpuScheduler::InitializeCommon failed because decoder "
               << "failed to initialize.";
    Destroy();
    return false;
  }

  return true;
}

void GpuScheduler::DestroyCommon() {
  if (decoder_.get()) {
    decoder_->MakeCurrent();
    decoder_->Destroy();
    decoder_.reset();
  }

  parser_.reset();
}

bool GpuScheduler::SetParent(GpuScheduler* parent_scheduler,
                             uint32 parent_texture_id) {
  if (parent_scheduler)
    return decoder_->SetParent(parent_scheduler->decoder_.get(),
                               parent_texture_id);
  else
    return decoder_->SetParent(NULL, 0);
}

#if defined(OS_MACOSX)
namespace {
const unsigned int kMaxOutstandingSwapBuffersCallsPerOnscreenContext = 1;
}
#endif

void GpuScheduler::PutChanged() {
  TRACE_EVENT1("gpu", "GpuScheduler:PutChanged", "this", this);

  DCHECK(IsScheduled());

  CommandBuffer::State state = command_buffer_->GetState();
  parser_->set_put(state.put_offset);
  if (state.error != error::kNoError)
    return;

  if (decoder_.get()) {
    if (!decoder_->MakeCurrent()) {
      LOG(ERROR) << "Context lost because MakeCurrent failed.";
      command_buffer_->SetContextLostReason(decoder_->GetContextLostReason());
      command_buffer_->SetParseError(error::kLostContext);
      return;
    }
  }

#if defined(OS_MACOSX)
  bool do_rate_limiting = surface_.get() != NULL;

  // Don't swamp the browser process with SwapBuffers calls it can't handle.
  DCHECK(!do_rate_limiting ||
         swap_buffers_count_ - acknowledged_swap_buffers_count_ == 0);
#endif

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

GpuScheduler::GpuScheduler(CommandBuffer* command_buffer,
                           gles2::GLES2Decoder* decoder,
                           CommandParser* parser)
    : command_buffer_(command_buffer),
      decoder_(decoder),
      parser_(parser),
      unscheduled_count_(0),
#if defined(OS_MACOSX)
      swap_buffers_count_(0),
      acknowledged_swap_buffers_count_(0),
#endif
      method_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
}

}  // namespace gpu
