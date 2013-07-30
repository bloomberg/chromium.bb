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
#include "third_party/WebKit/public/platform/WebGraphicsMemoryAllocation.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

using std::set;
using std::string;
using std::vector;

namespace cc {
namespace {

ManagedMemoryPolicy::PriorityCutoff ConvertPriorityCutoff(
    WebKit::WebGraphicsMemoryAllocation::PriorityCutoff priority_cutoff) {
  // This is simple a 1:1 map, the names differ only because the WebKit names
  // should be to match the cc names.
  switch (priority_cutoff) {
    case WebKit::WebGraphicsMemoryAllocation::PriorityCutoffAllowNothing:
      return ManagedMemoryPolicy::CUTOFF_ALLOW_NOTHING;
    case WebKit::WebGraphicsMemoryAllocation::PriorityCutoffAllowVisibleOnly:
      return ManagedMemoryPolicy::CUTOFF_ALLOW_REQUIRED_ONLY;
    case WebKit::WebGraphicsMemoryAllocation::
        PriorityCutoffAllowVisibleAndNearby:
      return ManagedMemoryPolicy::CUTOFF_ALLOW_NICE_TO_HAVE;
    case WebKit::WebGraphicsMemoryAllocation::PriorityCutoffAllowEverything:
      return ManagedMemoryPolicy::CUTOFF_ALLOW_EVERYTHING;
  }
  NOTREACHED();
  return ManagedMemoryPolicy::CUTOFF_ALLOW_NOTHING;
}

}  // anonymous namespace

class OutputSurfaceCallbacks
    : public WebKit::WebGraphicsContext3D::
        WebGraphicsSwapBuffersCompleteCallbackCHROMIUM,
      public WebKit::WebGraphicsContext3D::WebGraphicsContextLostCallback,
    public WebKit::WebGraphicsContext3D::
      WebGraphicsMemoryAllocationChangedCallbackCHROMIUM {
 public:
  explicit OutputSurfaceCallbacks(OutputSurface* client)
      : client_(client) {
    DCHECK(client_);
  }

  // WK:WGC3D::WGSwapBuffersCompleteCallbackCHROMIUM implementation.
  virtual void onSwapBuffersComplete() { client_->OnSwapBuffersComplete(NULL); }

  // WK:WGC3D::WGContextLostCallback implementation.
  virtual void onContextLost() { client_->DidLoseOutputSurface(); }

  // WK:WGC3D::WGMemoryAllocationChangedCallbackCHROMIUM implementation.
  virtual void onMemoryAllocationChanged(
      WebKit::WebGraphicsMemoryAllocation allocation) {
    ManagedMemoryPolicy policy(
        allocation.bytesLimitWhenVisible,
        ConvertPriorityCutoff(allocation.priorityCutoffWhenVisible),
        allocation.bytesLimitWhenNotVisible,
        ConvertPriorityCutoff(allocation.priorityCutoffWhenNotVisible));
    client_->SetMemoryPolicy(policy, !allocation.suggestHaveBackbuffer);
  }

 private:
  OutputSurface* client_;
};

OutputSurface::OutputSurface(
    scoped_ptr<WebKit::WebGraphicsContext3D> context3d)
    : context3d_(context3d.Pass()),
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
    scoped_ptr<WebKit::WebGraphicsContext3D> context3d,
    scoped_ptr<cc::SoftwareOutputDevice> software_device)
    : context3d_(context3d.Pass()),
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

base::TimeDelta OutputSurface::RetroactiveBeginFramePeriod() {
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
  base::TimeTicks alternative_deadline =
      skipped_begin_frame_args_.frame_time +
      RetroactiveBeginFramePeriod();
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

  if (context3d_) {
    context3d_->setSwapBuffersCompleteCallbackCHROMIUM(NULL);
    context3d_->setContextLostCallback(NULL);
    context3d_->setMemoryAllocationChangedCallbackCHROMIUM(NULL);
  }
}

bool OutputSurface::ForcedDrawToSoftwareDevice() const {
  return false;
}

bool OutputSurface::BindToClient(cc::OutputSurfaceClient* client) {
  DCHECK(client);
  client_ = client;
  bool success = true;

  if (context3d_) {
    success = context3d_->makeContextCurrent();
    if (success)
      SetContext3D(context3d_.Pass());
  }

  if (!success)
    client_ = NULL;

  return success;
}

bool OutputSurface::InitializeAndSetContext3D(
    scoped_ptr<WebKit::WebGraphicsContext3D> context3d,
    scoped_refptr<ContextProvider> offscreen_context_provider) {
  DCHECK(!context3d_);
  DCHECK(context3d);
  DCHECK(client_);

  bool success = false;
  if (context3d->makeContextCurrent()) {
    SetContext3D(context3d.Pass());
    if (client_->DeferredInitialize(offscreen_context_provider))
      success = true;
  }

  if (!success)
    ResetContext3D();

  return success;
}

void OutputSurface::ReleaseGL() {
  DCHECK(client_);
  DCHECK(context3d_);
  client_->ReleaseGL();
  ResetContext3D();
}

void OutputSurface::SetContext3D(
    scoped_ptr<WebKit::WebGraphicsContext3D> context3d) {
  DCHECK(!context3d_);
  DCHECK(context3d);
  DCHECK(client_);

  string extensions_string = UTF16ToASCII(context3d->getString(GL_EXTENSIONS));
  vector<string> extensions_list;
  base::SplitString(extensions_string, ' ', &extensions_list);
  set<string> extensions(extensions_list.begin(), extensions_list.end());
  has_gl_discard_backbuffer_ =
      extensions.count("GL_CHROMIUM_discard_backbuffer") > 0;
  has_swap_buffers_complete_callback_ =
       extensions.count("GL_CHROMIUM_swapbuffers_complete_callback") > 0;


  context3d_ = context3d.Pass();
  callbacks_.reset(new OutputSurfaceCallbacks(this));
  context3d_->setSwapBuffersCompleteCallbackCHROMIUM(callbacks_.get());
  context3d_->setContextLostCallback(callbacks_.get());
  context3d_->setMemoryAllocationChangedCallbackCHROMIUM(callbacks_.get());
}

void OutputSurface::ResetContext3D() {
  context3d_.reset();
  callbacks_.reset();
}

void OutputSurface::EnsureBackbuffer() {
  DCHECK(context3d_);
  if (has_gl_discard_backbuffer_)
    context3d_->ensureBackbufferCHROMIUM();
}

void OutputSurface::DiscardBackbuffer() {
  DCHECK(context3d_);
  if (has_gl_discard_backbuffer_)
    context3d_->discardBackbufferCHROMIUM();
}

void OutputSurface::Reshape(gfx::Size size, float scale_factor) {
  if (size == surface_size_ && scale_factor == device_scale_factor_)
    return;

  surface_size_ = size;
  device_scale_factor_ = scale_factor;
  if (context3d_) {
    context3d_->reshapeWithScaleFactor(
        size.width(), size.height(), scale_factor);
  }
  if (software_device_)
    software_device_->Resize(size);
}

gfx::Size OutputSurface::SurfaceSize() const {
  return surface_size_;
}

void OutputSurface::BindFramebuffer() {
  DCHECK(context3d_);
  context3d_->bindFramebuffer(GL_FRAMEBUFFER, 0);
}

void OutputSurface::SwapBuffers(cc::CompositorFrame* frame) {
  if (frame->software_frame_data) {
    PostSwapBuffersComplete();
    DidSwapBuffers();
    return;
  }

  DCHECK(context3d_);
  DCHECK(frame->gl_frame_data);

  if (frame->gl_frame_data->sub_buffer_rect ==
      gfx::Rect(frame->gl_frame_data->size)) {
    // Note that currently this has the same effect as SwapBuffers; we should
    // consider exposing a different entry point on WebGraphicsContext3D.
    context3d()->prepareTexture();
  } else {
    gfx::Rect sub_buffer_rect = frame->gl_frame_data->sub_buffer_rect;
    context3d()->postSubBufferCHROMIUM(sub_buffer_rect.x(),
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
                                    bool discard_backbuffer_when_not_visible) {
  TRACE_EVENT2("cc", "OutputSurface::SetMemoryPolicy",
               "bytes_limit_when_visible", policy.bytes_limit_when_visible,
               "discard_backbuffer", discard_backbuffer_when_not_visible);
  client_->SetMemoryPolicy(policy, discard_backbuffer_when_not_visible);
}

}  // namespace cc
