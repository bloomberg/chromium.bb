// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/output_surface.h"

#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/compositor_frame_ack.h"
#include "cc/output/managed_memory_policy.h"
#include "cc/output/output_surface_client.h"
#include "cc/scheduler/delay_based_time_source.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "ui/gfx/frame_time.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

using std::set;
using std::string;
using std::vector;

namespace {

const size_t kGpuLatencyHistorySize = 60;
const double kGpuLatencyEstimationPercentile = 100.0;

}

namespace cc {

OutputSurface::OutputSurface(scoped_refptr<ContextProvider> context_provider)
    : context_provider_(context_provider),
      device_scale_factor_(-1),
      max_frames_pending_(0),
      pending_swap_buffers_(0),
      needs_begin_frame_(false),
      client_ready_for_begin_frame_(true),
      client_(NULL),
      check_for_retroactive_begin_frame_pending_(false),
      external_stencil_test_enabled_(false),
      weak_ptr_factory_(this),
      gpu_latency_history_(kGpuLatencyHistorySize) {}

OutputSurface::OutputSurface(scoped_ptr<SoftwareOutputDevice> software_device)
    : software_device_(software_device.Pass()),
      device_scale_factor_(-1),
      max_frames_pending_(0),
      pending_swap_buffers_(0),
      needs_begin_frame_(false),
      client_ready_for_begin_frame_(true),
      client_(NULL),
      check_for_retroactive_begin_frame_pending_(false),
      external_stencil_test_enabled_(false),
      weak_ptr_factory_(this),
      gpu_latency_history_(kGpuLatencyHistorySize) {}

OutputSurface::OutputSurface(scoped_refptr<ContextProvider> context_provider,
                             scoped_ptr<SoftwareOutputDevice> software_device)
    : context_provider_(context_provider),
      software_device_(software_device.Pass()),
      device_scale_factor_(-1),
      max_frames_pending_(0),
      pending_swap_buffers_(0),
      needs_begin_frame_(false),
      client_ready_for_begin_frame_(true),
      client_(NULL),
      check_for_retroactive_begin_frame_pending_(false),
      external_stencil_test_enabled_(false),
      weak_ptr_factory_(this),
      gpu_latency_history_(kGpuLatencyHistorySize) {}

void OutputSurface::InitializeBeginFrameEmulation(
    base::SingleThreadTaskRunner* task_runner,
    bool throttle_frame_production,
    base::TimeDelta interval) {
  if (throttle_frame_production) {
    scoped_refptr<DelayBasedTimeSource> time_source;
    if (gfx::FrameTime::TimestampsAreHighRes())
      time_source = DelayBasedTimeSourceHighRes::Create(interval, task_runner);
    else
      time_source = DelayBasedTimeSource::Create(interval, task_runner);
    frame_rate_controller_.reset(new FrameRateController(time_source));
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

void OutputSurface::CommitVSyncParameters(base::TimeTicks timebase,
                                          base::TimeDelta interval) {
  TRACE_EVENT2("cc",
               "OutputSurface::CommitVSyncParameters",
               "timebase",
               (timebase - base::TimeTicks()).InSecondsF(),
               "interval",
               interval.InSecondsF());
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
void OutputSurface::SetNeedsRedrawRect(const gfx::Rect& damage_rect) {
  TRACE_EVENT0("cc", "OutputSurface::SetNeedsRedrawRect");
  client_->SetNeedsRedrawRect(damage_rect);
}

void OutputSurface::SetNeedsBeginFrame(bool enable) {
  TRACE_EVENT1("cc", "OutputSurface::SetNeedsBeginFrame", "enable", enable);
  needs_begin_frame_ = enable;
  client_ready_for_begin_frame_ = true;
  if (frame_rate_controller_) {
    BeginFrameArgs skipped = frame_rate_controller_->SetActive(enable);
    if (skipped.IsValid())
      skipped_begin_frame_args_ = skipped;
  }
  if (needs_begin_frame_)
    PostCheckForRetroactiveBeginFrame();
}

void OutputSurface::BeginFrame(const BeginFrameArgs& args) {
  TRACE_EVENT2("cc",
               "OutputSurface::BeginFrame",
               "client_ready_for_begin_frame_",
               client_ready_for_begin_frame_,
               "pending_swap_buffers_",
               pending_swap_buffers_);
  if (!needs_begin_frame_ || !client_ready_for_begin_frame_ ||
      (pending_swap_buffers_ >= max_frames_pending_ &&
       max_frames_pending_ > 0)) {
    skipped_begin_frame_args_ = args;
  } else {
    client_ready_for_begin_frame_ = false;
    client_->BeginFrame(args);
    // args might be an alias for skipped_begin_frame_args_.
    // Do not reset it before calling BeginFrame!
    skipped_begin_frame_args_ = BeginFrameArgs();
  }
}

base::TimeTicks OutputSurface::RetroactiveBeginFrameDeadline() {
  // TODO(brianderson): Remove the alternative deadline once we have better
  // deadline estimations.
  base::TimeTicks alternative_deadline =
      skipped_begin_frame_args_.frame_time +
      BeginFrameArgs::DefaultRetroactiveBeginFramePeriod();
  return std::max(skipped_begin_frame_args_.deadline, alternative_deadline);
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
  if (gfx::FrameTime::Now() < RetroactiveBeginFrameDeadline())
    BeginFrame(skipped_begin_frame_args_);
}

void OutputSurface::DidSwapBuffers() {
  pending_swap_buffers_++;
  TRACE_EVENT1("cc", "OutputSurface::DidSwapBuffers",
               "pending_swap_buffers_", pending_swap_buffers_);
  client_->DidSwapBuffers();
  if (frame_rate_controller_)
    frame_rate_controller_->DidSwapBuffers();
  PostCheckForRetroactiveBeginFrame();
}

void OutputSurface::OnSwapBuffersComplete() {
  pending_swap_buffers_--;
  TRACE_EVENT1("cc", "OutputSurface::OnSwapBuffersComplete",
               "pending_swap_buffers_", pending_swap_buffers_);
  client_->OnSwapBuffersComplete();
  if (frame_rate_controller_)
    frame_rate_controller_->DidSwapBuffersComplete();
  PostCheckForRetroactiveBeginFrame();
}

void OutputSurface::ReclaimResources(const CompositorFrameAck* ack) {
  client_->ReclaimResources(ack);
}

void OutputSurface::DidLoseOutputSurface() {
  TRACE_EVENT0("cc", "OutputSurface::DidLoseOutputSurface");
  client_ready_for_begin_frame_ = true;
  pending_swap_buffers_ = 0;
  skipped_begin_frame_args_ = BeginFrameArgs();
  if (frame_rate_controller_)
    frame_rate_controller_->SetActive(false);
  pending_gpu_latency_query_ids_.clear();
  available_gpu_latency_query_ids_.clear();
  client_->DidLoseOutputSurface();
}

void OutputSurface::SetExternalStencilTest(bool enabled) {
  external_stencil_test_enabled_ = enabled;
}

void OutputSurface::SetExternalDrawConstraints(const gfx::Transform& transform,
                                               const gfx::Rect& viewport,
                                               const gfx::Rect& clip,
                                               bool valid_for_tile_management) {
  client_->SetExternalDrawConstraints(
      transform, viewport, clip, valid_for_tile_management);
}

OutputSurface::~OutputSurface() {
  if (frame_rate_controller_)
    frame_rate_controller_->SetActive(false);
  ResetContext3d();
}

bool OutputSurface::HasExternalStencilTest() const {
  return external_stencil_test_enabled_;
}

bool OutputSurface::ForcedDrawToSoftwareDevice() const { return false; }

bool OutputSurface::BindToClient(OutputSurfaceClient* client) {
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

  context_provider_->SetLostContextCallback(
      base::Bind(&OutputSurface::DidLoseOutputSurface,
                 base::Unretained(this)));
  context_provider_->ContextSupport()->SetSwapBuffersCompleteCallback(
      base::Bind(&OutputSurface::OnSwapBuffersComplete,
                 base::Unretained(this)));
  context_provider_->SetMemoryPolicyChangedCallback(
      base::Bind(&OutputSurface::SetMemoryPolicy,
                 base::Unretained(this)));
}

void OutputSurface::ResetContext3d() {
  if (context_provider_.get()) {
    while (!pending_gpu_latency_query_ids_.empty()) {
      unsigned query_id = pending_gpu_latency_query_ids_.front();
      pending_gpu_latency_query_ids_.pop_front();
      context_provider_->ContextGL()->DeleteQueriesEXT(1, &query_id);
    }
    while (!available_gpu_latency_query_ids_.empty()) {
      unsigned query_id = available_gpu_latency_query_ids_.front();
      available_gpu_latency_query_ids_.pop_front();
      context_provider_->ContextGL()->DeleteQueriesEXT(1, &query_id);
    }
    context_provider_->SetLostContextCallback(
        ContextProvider::LostContextCallback());
    context_provider_->SetMemoryPolicyChangedCallback(
        ContextProvider::MemoryPolicyChangedCallback());
    if (gpu::ContextSupport* support = context_provider_->ContextSupport())
      support->SetSwapBuffersCompleteCallback(base::Closure());
  }
  context_provider_ = NULL;
}

void OutputSurface::EnsureBackbuffer() {
  if (software_device_)
    software_device_->EnsureBackbuffer();
}

void OutputSurface::DiscardBackbuffer() {
  if (context_provider_)
    context_provider_->ContextGL()->DiscardBackbufferCHROMIUM();
  if (software_device_)
    software_device_->DiscardBackbuffer();
}

void OutputSurface::Reshape(const gfx::Size& size, float scale_factor) {
  if (size == surface_size_ && scale_factor == device_scale_factor_)
    return;

  surface_size_ = size;
  device_scale_factor_ = scale_factor;
  if (context_provider_) {
    context_provider_->ContextGL()->ResizeCHROMIUM(
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
  context_provider_->ContextGL()->BindFramebuffer(GL_FRAMEBUFFER, 0);
}

void OutputSurface::SwapBuffers(CompositorFrame* frame) {
  if (frame->software_frame_data) {
    PostSwapBuffersComplete();
    DidSwapBuffers();
    return;
  }

  DCHECK(context_provider_);
  DCHECK(frame->gl_frame_data);

  UpdateAndMeasureGpuLatency();
  if (frame->gl_frame_data->sub_buffer_rect ==
      gfx::Rect(frame->gl_frame_data->size)) {
    context_provider_->ContextSupport()->Swap();
  } else {
    context_provider_->ContextSupport()->PartialSwapBuffers(
        frame->gl_frame_data->sub_buffer_rect);
  }

  DidSwapBuffers();
}

base::TimeDelta OutputSurface::GpuLatencyEstimate() {
  if (context_provider_ && !capabilities_.adjust_deadline_for_parent)
    return gpu_latency_history_.Percentile(kGpuLatencyEstimationPercentile);
  else
    return base::TimeDelta();
}

void OutputSurface::UpdateAndMeasureGpuLatency() {
  // http://crbug.com/306690  tracks re-enabling latency queries.
#if 0
  // We only care about GPU latency for surfaces that do not have a parent
  // compositor, since surfaces that do have a parent compositor can use
  // mailboxes or delegated rendering to send frames to their parent without
  // incurring GPU latency.
  if (capabilities_.adjust_deadline_for_parent)
    return;

  while (pending_gpu_latency_query_ids_.size()) {
    unsigned query_id = pending_gpu_latency_query_ids_.front();
    unsigned query_complete = 1;
    context_provider_->ContextGL()->GetQueryObjectuivEXT(
        query_id, GL_QUERY_RESULT_AVAILABLE_EXT, &query_complete);
    if (!query_complete)
      break;

    unsigned value = 0;
    context_provider_->ContextGL()->GetQueryObjectuivEXT(
        query_id, GL_QUERY_RESULT_EXT, &value);
    pending_gpu_latency_query_ids_.pop_front();
    available_gpu_latency_query_ids_.push_back(query_id);

    base::TimeDelta latency = base::TimeDelta::FromMicroseconds(value);
    base::TimeDelta latency_estimate = GpuLatencyEstimate();
    gpu_latency_history_.InsertSample(latency);

    base::TimeDelta latency_overestimate;
    base::TimeDelta latency_underestimate;
    if (latency > latency_estimate)
      latency_underestimate = latency - latency_estimate;
    else
      latency_overestimate = latency_estimate - latency;
    UMA_HISTOGRAM_CUSTOM_TIMES("Renderer.GpuLatency",
                               latency,
                               base::TimeDelta::FromMilliseconds(1),
                               base::TimeDelta::FromMilliseconds(100),
                               50);
    UMA_HISTOGRAM_CUSTOM_TIMES("Renderer.GpuLatencyUnderestimate",
                               latency_underestimate,
                               base::TimeDelta::FromMilliseconds(1),
                               base::TimeDelta::FromMilliseconds(100),
                               50);
    UMA_HISTOGRAM_CUSTOM_TIMES("Renderer.GpuLatencyOverestimate",
                               latency_overestimate,
                               base::TimeDelta::FromMilliseconds(1),
                               base::TimeDelta::FromMilliseconds(100),
                               50);
  }

  unsigned gpu_latency_query_id;
  if (available_gpu_latency_query_ids_.size()) {
    gpu_latency_query_id = available_gpu_latency_query_ids_.front();
    available_gpu_latency_query_ids_.pop_front();
  } else {
    context_provider_->ContextGL()->GenQueriesEXT(1, &gpu_latency_query_id);
  }

  context_provider_->ContextGL()->BeginQueryEXT(GL_LATENCY_QUERY_CHROMIUM,
                                                gpu_latency_query_id);
  context_provider_->ContextGL()->EndQueryEXT(GL_LATENCY_QUERY_CHROMIUM);
  pending_gpu_latency_query_ids_.push_back(gpu_latency_query_id);
#endif
}

void OutputSurface::PostSwapBuffersComplete() {
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&OutputSurface::OnSwapBuffersComplete,
                 weak_ptr_factory_.GetWeakPtr()));
}

void OutputSurface::SetMemoryPolicy(const ManagedMemoryPolicy& policy) {
  TRACE_EVENT1("cc", "OutputSurface::SetMemoryPolicy",
               "bytes_limit_when_visible", policy.bytes_limit_when_visible);
  // Just ignore the memory manager when it says to set the limit to zero
  // bytes. This will happen when the memory manager thinks that the renderer
  // is not visible (which the renderer knows better).
  if (policy.bytes_limit_when_visible)
    client_->SetMemoryPolicy(policy);
}

}  // namespace cc
