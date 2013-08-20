// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/output_surface.h"

#include <set>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/managed_memory_policy.h"
#include "cc/output/output_surface_client.h"
#include "cc/scheduler/delay_based_time_source.h"
#include "third_party/WebKit/public/platform/WebGraphicsContext3D.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

using std::set;
using std::string;
using std::vector;

namespace cc {

OutputSurface::OutputSurface(
    scoped_refptr<ContextProvider> context_provider)
    : context_provider_(context_provider),
      has_gl_discard_backbuffer_(false),
      has_swap_buffers_complete_callback_(false),
      device_scale_factor_(-1),
      weak_ptr_factory_(this),
      max_frames_pending_(0),
      pending_swap_buffers_(0),
      needs_begin_frame_(false),
      begin_frame_pending_(false),
      client_(NULL),
      check_for_retroactive_begin_frame_pending_(false) {
}

OutputSurface::OutputSurface(
    scoped_ptr<cc::SoftwareOutputDevice> software_device)
    : software_device_(software_device.Pass()),
      has_gl_discard_backbuffer_(false),
      has_swap_buffers_complete_callback_(false),
      device_scale_factor_(-1),
      weak_ptr_factory_(this),
      max_frames_pending_(0),
      pending_swap_buffers_(0),
      needs_begin_frame_(false),
      begin_frame_pending_(false),
      client_(NULL),
      check_for_retroactive_begin_frame_pending_(false) {
}

OutputSurface::OutputSurface(
    scoped_refptr<ContextProvider> context_provider,
    scoped_ptr<cc::SoftwareOutputDevice> software_device)
    : context_provider_(context_provider),
      software_device_(software_device.Pass()),
      has_gl_discard_backbuffer_(false),
      has_swap_buffers_complete_callback_(false),
      device_scale_factor_(-1),
      weak_ptr_factory_(this),
      max_frames_pending_(0),
      pending_swap_buffers_(0),
      needs_begin_frame_(false),
      begin_frame_pending_(false),
      client_(NULL),
      check_for_retroactive_begin_frame_pending_(false) {
}

void OutputSurface::InitializeBeginFrameEmulation(
    base::SingleThreadTaskRunner* task_runner,
    bool throttle_frame_production,
    base::TimeDelta interval) {
  if (throttle_frame_production) {
    frame_rate_controller_.reset(
        new FrameRateController(
            DelayBasedTimeSource::Create(interval, task_runner)));
  } else {
    frame_rate_controller_.reset(new FrameRateController(task_runner));
  }

  frame_rate_controller_->SetClient(this);
  frame_rate_controller_->SetMaxSwapsPending(max_frames_pending_);
  frame_rate_controller_->SetDeadlineAdjustment(
      capabilities_.adjust_deadline_for_parent ?
          BeginFrameArgs::DefaultDeadlineAdjustment() : base::TimeDelta());

  // The new frame rate controller will consume the swap acks of the old
  // frame rate controller, so we set that expectation up here.
  for (int i = 0; i < pending_swap_buffers_; i++)
    frame_rate_controller_->DidSwapBuffers();
}

void OutputSurface::SetMaxFramesPending(int max_frames_pending) {
  if (frame_rate_controller_)
    frame_rate_controller_->SetMaxSwapsPending(max_frames_pending);
  max_frames_pending_ = max_frames_pending;
}

void OutputSurface::OnVSyncParametersChanged(base::TimeTicks timebase,
                                             base::TimeDelta interval) {
  TRACE_EVENT2("cc", "OutputSurface::OnVSyncParametersChanged",
               "timebase", (timebase - base::TimeTicks()).InSecondsF(),
               "interval", interval.InSecondsF());
  if (frame_rate_controller_)
    frame_rate_controller_->SetTimebaseAndInterval(timebase, interval);
}

void OutputSurface::FrameRateControllerTick(bool throttled,
                                            const BeginFrameArgs& args) {
  DCHECK(frame_rate_controller_);
  if (throttled)
    skipped_begin_frame_args_ = args;
  else
    BeginFrame(args);
}

// Forwarded to OutputSurfaceClient
void OutputSurface::SetNeedsRedrawRect(gfx::Rect damage_rect) {
  TRACE_EVENT0("cc", "OutputSurface::SetNeedsRedrawRect");
  client_->SetNeedsRedrawRect(damage_rect);
}

void OutputSurface::SetNeedsBeginFrame(bool enable) {
  TRACE_EVENT1("cc", "OutputSurface::SetNeedsBeginFrame", "enable", enable);
  needs_begin_frame_ = enable;
  begin_frame_pending_ = false;
  if (frame_rate_controller_) {
    BeginFrameArgs skipped = frame_rate_controller_->SetActive(enable);
    if (skipped.IsValid())
      skipped_begin_frame_args_ = skipped;
  }
  if (needs_begin_frame_)
    PostCheckForRetroactiveBeginFrame();
}

void OutputSurface::BeginFrame(const BeginFrameArgs& args) {
  TRACE_EVENT2("cc", "OutputSurface::BeginFrame",
               "begin_frame_pending_", begin_frame_pending_,
               "pending_swap_buffers_", pending_swap_buffers_);
  if (!needs_begin_frame_ || begin_frame_pending_ ||
      (pending_swap_buffers_ >= max_frames_pending_ &&
       max_frames_pending_ > 0)) {
    skipped_begin_frame_args_ = args;
  } else {
    begin_frame_pending_ = true;
    client_->BeginFrame(args);
    // args might be an alias for skipped_begin_frame_args_.
    // Do not reset it before calling BeginFrame!
    skipped_begin_frame_args_ = BeginFrameArgs();
  }
}

base::TimeDelta OutputSurface::AlternateRetroactiveBeginFramePeriod() {
  return BeginFrameArgs::DefaultRetroactiveBeginFramePeriod();
}

void OutputSurface::PostCheckForRetroactiveBeginFrame() {
  if (!skipped_begin_frame_args_.IsValid() ||
      check_for_retroactive_begin_frame_pending_)
    return;

  base::MessageLoop::current()->PostTask(
     FROM_HERE,
     base::Bind(&OutputSurface::CheckForRetroactiveBeginFrame,
                weak_ptr_factory_.GetWeakPtr()));
  check_for_retroactive_begin_frame_pending_ = true;
}

void OutputSurface::CheckForRetroactiveBeginFrame() {
  TRACE_EVENT0("cc", "OutputSurface::CheckForRetroactiveBeginFrame");
  check_for_retroactive_begin_frame_pending_ = false;
  base::TimeTicks now = base::TimeTicks::Now();
  // TODO(brianderson): Remove the alternative deadline once we have better
  // deadline estimations.
  base::TimeTicks alternative_deadline =
      skipped_begin_frame_args_.frame_time +
      AlternateRetroactiveBeginFramePeriod();
  if (now < skipped_begin_frame_args_.deadline ||
      now < alternative_deadline) {
    BeginFrame(skipped_begin_frame_args_);
  }
}

void OutputSurface::DidSwapBuffers() {
  begin_frame_pending_ = false;
  pending_swap_buffers_++;
  TRACE_EVENT1("cc", "OutputSurface::DidSwapBuffers",
               "pending_swap_buffers_", pending_swap_buffers_);
  if (frame_rate_controller_)
    frame_rate_controller_->DidSwapBuffers();
  PostCheckForRetroactiveBeginFrame();
}

void OutputSurface::OnSwapBuffersComplete(const CompositorFrameAck* ack) {
  pending_swap_buffers_--;
  TRACE_EVENT1("cc", "OutputSurface::OnSwapBuffersComplete",
               "pending_swap_buffers_", pending_swap_buffers_);
  client_->OnSwapBuffersComplete(ack);
  if (frame_rate_controller_)
    frame_rate_controller_->DidSwapBuffersComplete();
  PostCheckForRetroactiveBeginFrame();
}

void OutputSurface::DidLoseOutputSurface() {
  TRACE_EVENT0("cc", "OutputSurface::DidLoseOutputSurface");
  begin_frame_pending_ = false;
  pending_swap_buffers_ = 0;
  client_->DidLoseOutputSurface();
}

void OutputSurface::SetExternalStencilTest(bool enabled) {
  client_->SetExternalStencilTest(enabled);
}

void OutputSurface::SetExternalDrawConstraints(const gfx::Transform& transform,
                                               gfx::Rect viewport) {
  client_->SetExternalDrawConstraints(transform, viewport);
}

OutputSurface::~OutputSurface() {
  if (frame_rate_controller_)
    frame_rate_controller_->SetActive(false);
  ResetContext3d();
}

bool OutputSurface::ForcedDrawToSoftwareDevice() const {
  return false;
}

bool OutputSurface::BindToClient(cc::OutputSurfaceClient* client) {
  DCHECK(client);
  client_ = client;
  bool success = true;

  if (context_provider_) {
    success = context_provider_->BindToCurrentThread();
    if (success)
      SetUpContext3d();
  }

  if (!success)
    client_ = NULL;

  return success;
}

bool OutputSurface::InitializeAndSetContext3d(
    scoped_refptr<ContextProvider> context_provider,
    scoped_refptr<ContextProvider> offscreen_context_provider) {
  DCHECK(!context_provider_);
  DCHECK(context_provider);
  DCHECK(client_);

  bool success = false;
  if (context_provider->BindToCurrentThread()) {
    context_provider_ = context_provider;
    SetUpContext3d();
    if (client_->DeferredInitialize(offscreen_context_provider))
      success = true;
  }

  if (!success)
    ResetContext3d();

  return success;
}

void OutputSurface::ReleaseGL() {
  DCHECK(client_);
  DCHECK(context_provider_);
  client_->ReleaseGL();
  ResetContext3d();
}

void OutputSurface::SetUpContext3d() {
  DCHECK(context_provider_);
  DCHECK(client_);

  WebKit::WebGraphicsContext3D* context3d = context_provider_->Context3d();

  string extensions_string =
      UTF16ToASCII(context3d->getString(GL_EXTENSIONS));
  vector<string> extensions_list;
  base::SplitString(extensions_string, ' ', &extensions_list);
  set<string> extensions(extensions_list.begin(), extensions_list.end());
  has_gl_discard_backbuffer_ =
      extensions.count("GL_CHROMIUM_discard_backbuffer") > 0;
  has_swap_buffers_complete_callback_ =
       extensions.count("GL_CHROMIUM_swapbuffers_complete_callback") > 0;

  context_provider_->SetLostContextCallback(
      base::Bind(&OutputSurface::DidLoseOutputSurface,
                 base::Unretained(this)));
  context_provider_->SetSwapBuffersCompleteCallback(
      base::Bind(&OutputSurface::OnSwapBuffersComplete,
                 base::Unretained(this),
                 static_cast<CompositorFrameAck*>(NULL)));
  context_provider_->SetMemoryPolicyChangedCallback(
      base::Bind(&OutputSurface::SetMemoryPolicy,
                 base::Unretained(this)));
}

void OutputSurface::ResetContext3d() {
  if (context_provider_.get()) {
    context_provider_->SetLostContextCallback(
        ContextProvider::LostContextCallback());
    context_provider_->SetSwapBuffersCompleteCallback(
        ContextProvider::SwapBuffersCompleteCallback());
    context_provider_->SetMemoryPolicyChangedCallback(
        ContextProvider::MemoryPolicyChangedCallback());
  }
  context_provider_ = NULL;
}

void OutputSurface::EnsureBackbuffer() {
  if (context_provider_ && has_gl_discard_backbuffer_)
    context_provider_->Context3d()->ensureBackbufferCHROMIUM();
}

void OutputSurface::DiscardBackbuffer() {
  if (context_provider_ && has_gl_discard_backbuffer_)
    context_provider_->Context3d()->discardBackbufferCHROMIUM();
}

void OutputSurface::Reshape(gfx::Size size, float scale_factor) {
  if (size == surface_size_ && scale_factor == device_scale_factor_)
    return;

  surface_size_ = size;
  device_scale_factor_ = scale_factor;
  if (context_provider_) {
    context_provider_->Context3d()->reshapeWithScaleFactor(
        size.width(), size.height(), scale_factor);
  }
  if (software_device_)
    software_device_->Resize(size);
}

gfx::Size OutputSurface::SurfaceSize() const {
  return surface_size_;
}

void OutputSurface::BindFramebuffer() {
  DCHECK(context_provider_);
  context_provider_->Context3d()->bindFramebuffer(GL_FRAMEBUFFER, 0);
}

void OutputSurface::SwapBuffers(cc::CompositorFrame* frame) {
  if (frame->software_frame_data) {
    PostSwapBuffersComplete();
    DidSwapBuffers();
    return;
  }

  DCHECK(context_provider_);
  DCHECK(frame->gl_frame_data);

  if (frame->gl_frame_data->sub_buffer_rect ==
      gfx::Rect(frame->gl_frame_data->size)) {
    // Note that currently this has the same effect as SwapBuffers; we should
    // consider exposing a different entry point on WebGraphicsContext3D.
    context_provider_->Context3d()->prepareTexture();
  } else {
    gfx::Rect sub_buffer_rect = frame->gl_frame_data->sub_buffer_rect;
    context_provider_->Context3d()->postSubBufferCHROMIUM(
        sub_buffer_rect.x(),
        sub_buffer_rect.y(),
        sub_buffer_rect.width(),
        sub_buffer_rect.height());
  }

  if (!has_swap_buffers_complete_callback_)
    PostSwapBuffersComplete();

  DidSwapBuffers();
}

void OutputSurface::PostSwapBuffersComplete() {
  base::MessageLoop::current()->PostTask(
       FROM_HERE,
       base::Bind(&OutputSurface::OnSwapBuffersComplete,
                  weak_ptr_factory_.GetWeakPtr(),
                  static_cast<CompositorFrameAck*>(NULL)));
}

void OutputSurface::SetMemoryPolicy(const ManagedMemoryPolicy& policy,
                                    bool discard_backbuffer) {
  TRACE_EVENT2("cc", "OutputSurface::SetMemoryPolicy",
               "bytes_limit_when_visible", policy.bytes_limit_when_visible,
               "discard_backbuffer", discard_backbuffer);
  // Just ignore the memory manager when it says to set the limit to zero
  // bytes. This will happen when the memory manager thinks that the renderer
  // is not visible (which the renderer knows better).
  if (policy.bytes_limit_when_visible)
    client_->SetMemoryPolicy(policy);
  client_->SetDiscardBackBufferWhenNotVisible(discard_backbuffer);
}

}  // namespace cc
