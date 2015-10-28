// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/android/synchronous_compositor_output_surface.h"

#include "base/auto_reset.h"
#include "base/logging.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/context_provider.h"
#include "cc/output/output_surface_client.h"
#include "cc/output/software_output_device.h"
#include "content/renderer/android/synchronous_compositor_external_begin_frame_source.h"
#include "content/renderer/android/synchronous_compositor_registry.h"
#include "content/renderer/gpu/frame_swap_message_queue.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/common/gpu_memory_allocation.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/transform.h"

namespace content {

namespace {

// Do not limit number of resources, so use an unrealistically high value.
const size_t kNumResourcesLimit = 10 * 1000 * 1000;

}  // namespace

class SynchronousCompositorOutputSurface::SoftwareDevice
  : public cc::SoftwareOutputDevice {
 public:
  SoftwareDevice(SynchronousCompositorOutputSurface* surface)
    : surface_(surface) {
  }
  void Resize(const gfx::Size& pixel_size, float scale_factor) override {
    // Intentional no-op: canvas size is controlled by the embedder.
  }
  SkCanvas* BeginPaint(const gfx::Rect& damage_rect) override {
    if (!surface_->current_sw_canvas_) {
      NOTREACHED() << "BeginPaint with no canvas set";
      return &null_canvas_;
    }
    LOG_IF(WARNING, surface_->frame_holder_.get())
        << "Mutliple calls to BeginPaint per frame";
    return surface_->current_sw_canvas_;
  }
  void EndPaint() override {}

 private:
  SynchronousCompositorOutputSurface* surface_;
  SkCanvas null_canvas_;

  DISALLOW_COPY_AND_ASSIGN(SoftwareDevice);
};

SynchronousCompositorOutputSurface::SynchronousCompositorOutputSurface(
    const scoped_refptr<cc::ContextProvider>& context_provider,
    const scoped_refptr<cc::ContextProvider>& worker_context_provider,
    int routing_id,
    SynchronousCompositorRegistry* registry,
    scoped_refptr<FrameSwapMessageQueue> frame_swap_message_queue)
    : cc::OutputSurface(
          context_provider,
          worker_context_provider,
          scoped_ptr<cc::SoftwareOutputDevice>(new SoftwareDevice(this))),
      routing_id_(routing_id),
      registry_(registry),
      registered_(false),
      sync_client_(nullptr),
      next_hardware_draw_needs_damage_(false),
      current_sw_canvas_(nullptr),
      memory_policy_(0u),
      frame_swap_message_queue_(frame_swap_message_queue) {
  thread_checker_.DetachFromThread();
  DCHECK(registry_);
  capabilities_.adjust_deadline_for_parent = false;
  capabilities_.delegated_rendering = true;
  capabilities_.max_frames_pending = 1;
  memory_policy_.priority_cutoff_when_visible =
      gpu::MemoryAllocation::CUTOFF_ALLOW_NICE_TO_HAVE;
}

SynchronousCompositorOutputSurface::~SynchronousCompositorOutputSurface() {}

void SynchronousCompositorOutputSurface::SetSyncClient(
    SynchronousCompositorOutputSurfaceClient* compositor) {
  DCHECK(CalledOnValidThread());
  sync_client_ = compositor;
}

bool SynchronousCompositorOutputSurface::BindToClient(
    cc::OutputSurfaceClient* surface_client) {
  DCHECK(CalledOnValidThread());
  if (!cc::OutputSurface::BindToClient(surface_client))
    return false;

  client_->SetMemoryPolicy(memory_policy_);
  registry_->RegisterOutputSurface(routing_id_, this);
  registered_ = true;
  return true;
}

void SynchronousCompositorOutputSurface::DetachFromClient() {
  DCHECK(CalledOnValidThread());
  if (registered_) {
    registry_->UnregisterOutputSurface(routing_id_, this);
  }
  cc::OutputSurface::DetachFromClient();
}

void SynchronousCompositorOutputSurface::Reshape(
    const gfx::Size& size, float scale_factor) {
  // Intentional no-op: surface size is controlled by the embedder.
}

void SynchronousCompositorOutputSurface::SwapBuffers(
    cc::CompositorFrame* frame) {
  DCHECK(CalledOnValidThread());
  frame_holder_.reset(new cc::CompositorFrame);
  frame->AssignTo(frame_holder_.get());
  client_->DidSwapBuffers();
}

void SynchronousCompositorOutputSurface::Invalidate() {
  DCHECK(CalledOnValidThread());
  if (sync_client_)
    sync_client_->Invalidate();
}

scoped_ptr<cc::CompositorFrame>
SynchronousCompositorOutputSurface::DemandDrawHw(
    const gfx::Size& surface_size,
    const gfx::Transform& transform,
    const gfx::Rect& viewport,
    const gfx::Rect& clip,
    const gfx::Rect& viewport_rect_for_tile_priority,
    const gfx::Transform& transform_for_tile_priority) {
  DCHECK(CalledOnValidThread());
  DCHECK(HasClient());
  DCHECK(context_provider_.get());

  surface_size_ = surface_size;
  InvokeComposite(transform, viewport, clip, viewport_rect_for_tile_priority,
                  transform_for_tile_priority, true);

  return frame_holder_.Pass();
}

scoped_ptr<cc::CompositorFrame>
SynchronousCompositorOutputSurface::DemandDrawSw(SkCanvas* canvas) {
  DCHECK(CalledOnValidThread());
  DCHECK(canvas);
  DCHECK(!current_sw_canvas_);

  base::AutoReset<SkCanvas*> canvas_resetter(&current_sw_canvas_, canvas);

  SkIRect canvas_clip;
  canvas->getClipDeviceBounds(&canvas_clip);
  gfx::Rect clip = gfx::SkIRectToRect(canvas_clip);

  gfx::Transform transform(gfx::Transform::kSkipInitialization);
  transform.matrix() = canvas->getTotalMatrix();  // Converts 3x3 matrix to 4x4.

  surface_size_ = gfx::Size(canvas->getDeviceSize().width(),
                            canvas->getDeviceSize().height());

  // Pass in the cached hw viewport and transform for tile priority to avoid
  // tile thrashing when the WebView is alternating between hardware and
  // software draws.
  InvokeComposite(transform,
                  clip,
                  clip,
                  cached_hw_viewport_rect_for_tile_priority_,
                  cached_hw_transform_for_tile_priority_,
                  false);

  return frame_holder_.Pass();
}

void SynchronousCompositorOutputSurface::InvokeComposite(
    const gfx::Transform& transform,
    const gfx::Rect& viewport,
    const gfx::Rect& clip,
    const gfx::Rect& viewport_rect_for_tile_priority,
    const gfx::Transform& transform_for_tile_priority,
    bool hardware_draw) {
  DCHECK(!frame_holder_.get());

  gfx::Transform adjusted_transform = transform;
  adjusted_transform.matrix().postTranslate(-viewport.x(), -viewport.y(), 0);
  SetExternalDrawConstraints(adjusted_transform, viewport, clip,
                             viewport_rect_for_tile_priority,
                             transform_for_tile_priority, !hardware_draw);
  if (!hardware_draw || next_hardware_draw_needs_damage_) {
    next_hardware_draw_needs_damage_ = false;
    SetNeedsRedrawRect(gfx::Rect(viewport.size()));
  }

  client_->OnDraw();

  // After software draws (which might move the viewport arbitrarily), restore
  // the previous hardware viewport to allow CC's tile manager to prioritize
  // properly.
  if (hardware_draw) {
    cached_hw_transform_ = adjusted_transform;
    cached_hw_viewport_ = viewport;
    cached_hw_clip_ = clip;
    cached_hw_viewport_rect_for_tile_priority_ =
        viewport_rect_for_tile_priority;
    cached_hw_transform_for_tile_priority_ = transform_for_tile_priority;
  } else {
    bool resourceless_software_draw = false;
    SetExternalDrawConstraints(cached_hw_transform_,
                               cached_hw_viewport_,
                               cached_hw_clip_,
                               cached_hw_viewport_rect_for_tile_priority_,
                               cached_hw_transform_for_tile_priority_,
                               resourceless_software_draw);
    // This draw may have reset all damage, which would lead to subsequent
    // incorrect hardware draw, so explicitly set damage for next hardware
    // draw as well.
    next_hardware_draw_needs_damage_ = true;
  }

  if (frame_holder_.get())
    client_->DidSwapBuffersComplete();
}

void SynchronousCompositorOutputSurface::ReturnResources(
    const cc::CompositorFrameAck& frame_ack) {
  ReclaimResources(&frame_ack);
}

void SynchronousCompositorOutputSurface::SetMemoryPolicy(size_t bytes_limit) {
  DCHECK(CalledOnValidThread());
  bool became_zero = memory_policy_.bytes_limit_when_visible && !bytes_limit;
  bool became_non_zero =
      !memory_policy_.bytes_limit_when_visible && bytes_limit;
  memory_policy_.bytes_limit_when_visible = bytes_limit;
  memory_policy_.num_resources_limit = kNumResourcesLimit;

  if (client_)
    client_->SetMemoryPolicy(memory_policy_);

  if (became_zero) {
    // This is small hack to drop context resources without destroying it
    // when this compositor is put into the background.
    context_provider()->ContextSupport()->SetAggressivelyFreeResources(
        true /* aggressively_free_resources */);
  } else if (became_non_zero) {
    context_provider()->ContextSupport()->SetAggressivelyFreeResources(
        false /* aggressively_free_resources */);
  }
}

void SynchronousCompositorOutputSurface::SetTreeActivationCallback(
    const base::Closure& callback) {
  DCHECK(client_);
  client_->SetTreeActivationCallback(callback);
}

void SynchronousCompositorOutputSurface::GetMessagesToDeliver(
    ScopedVector<IPC::Message>* messages) {
  DCHECK(CalledOnValidThread());
  scoped_ptr<FrameSwapMessageQueue::SendMessageScope> send_message_scope =
      frame_swap_message_queue_->AcquireSendMessageScope();
  frame_swap_message_queue_->DrainMessages(messages);
}

bool SynchronousCompositorOutputSurface::CalledOnValidThread() const {
  return thread_checker_.CalledOnValidThread();
}

}  // namespace content
