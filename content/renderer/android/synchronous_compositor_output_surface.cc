// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/android/synchronous_compositor_output_surface.h"

#include <vector>

#include "base/auto_reset.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/context_provider.h"
#include "cc/output/output_surface_client.h"
#include "cc/output/renderer_settings.h"
#include "cc/output/software_output_device.h"
#include "cc/output/texture_mailbox_deleter.h"
#include "cc/surfaces/display.h"
#include "cc/surfaces/surface_factory.h"
#include "cc/surfaces/surface_id_allocator.h"
#include "cc/surfaces/surface_manager.h"
#include "content/common/android/sync_compositor_messages.h"
#include "content/renderer/android/synchronous_compositor_filter.h"
#include "content/renderer/android/synchronous_compositor_registry.h"
#include "content/renderer/gpu/frame_swap_message_queue.h"
#include "content/renderer/render_thread_impl.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/common/gpu_memory_allocation.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_sender.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/transform.h"

namespace content {

namespace {

const int64_t kFallbackTickTimeoutInMilliseconds = 100;
const uint32_t kCompositorSurfaceNamespace = 1;

// Do not limit number of resources, so use an unrealistically high value.
const size_t kNumResourcesLimit = 10 * 1000 * 1000;

}  // namespace

class SoftwareDevice : public cc::SoftwareOutputDevice {
 public:
  SoftwareDevice(SkCanvas** canvas) : canvas_(canvas) {}

  void Resize(const gfx::Size& pixel_size, float scale_factor) override {
    // Intentional no-op: canvas size is controlled by the embedder.
  }
  SkCanvas* BeginPaint(const gfx::Rect& damage_rect) override {
    DCHECK(*canvas_) << "BeginPaint with no canvas set";
    return *canvas_;
  }
  void EndPaint() override {}

 private:
  SkCanvas** canvas_;

  DISALLOW_COPY_AND_ASSIGN(SoftwareDevice);
};

class SoftwareOutputSurface : public cc::OutputSurface {
 public:
  SoftwareOutputSurface(std::unique_ptr<SoftwareDevice> software_device)
      : cc::OutputSurface(nullptr, nullptr, std::move(software_device)) {}

  // cc::OutputSurface implementation.
  uint32_t GetFramebufferCopyTextureFormat() override { return 0; }
  void SwapBuffers(cc::CompositorFrame frame) override {}
};

SynchronousCompositorOutputSurface::SynchronousCompositorOutputSurface(
    scoped_refptr<cc::ContextProvider> context_provider,
    scoped_refptr<cc::ContextProvider> worker_context_provider,
    int routing_id,
    uint32_t output_surface_id,
    SynchronousCompositorRegistry* registry,
    scoped_refptr<FrameSwapMessageQueue> frame_swap_message_queue)
    : cc::OutputSurface(std::move(context_provider),
                        std::move(worker_context_provider),
                        nullptr),
      routing_id_(routing_id),
      output_surface_id_(output_surface_id),
      registry_(registry),
      sender_(RenderThreadImpl::current()->sync_compositor_message_filter()),
      memory_policy_(0u),
      frame_swap_message_queue_(frame_swap_message_queue),
      surface_manager_(new cc::SurfaceManager),
      surface_id_allocator_(
          new cc::SurfaceIdAllocator(kCompositorSurfaceNamespace)),
      surface_factory_(new cc::SurfaceFactory(surface_manager_.get(), this)) {
  DCHECK(registry_);
  DCHECK(sender_);
  thread_checker_.DetachFromThread();
  capabilities_.adjust_deadline_for_parent = false;
  capabilities_.delegated_rendering = true;
  memory_policy_.priority_cutoff_when_visible =
      gpu::MemoryAllocation::CUTOFF_ALLOW_NICE_TO_HAVE;
  surface_id_allocator_->RegisterSurfaceIdNamespace(surface_manager_.get());
}

SynchronousCompositorOutputSurface::~SynchronousCompositorOutputSurface() {}

void SynchronousCompositorOutputSurface::SetSyncClient(
    SynchronousCompositorOutputSurfaceClient* compositor) {
  DCHECK(CalledOnValidThread());
  sync_client_ = compositor;
  if (sync_client_)
    Send(new SyncCompositorHostMsg_OutputSurfaceCreated(routing_id_));
}

bool SynchronousCompositorOutputSurface::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(SynchronousCompositorOutputSurface, message)
    IPC_MESSAGE_HANDLER(SyncCompositorMsg_SetMemoryPolicy, SetMemoryPolicy)
    IPC_MESSAGE_HANDLER(SyncCompositorMsg_ReclaimResources, OnReclaimResources)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

bool SynchronousCompositorOutputSurface::BindToClient(
    cc::OutputSurfaceClient* surface_client) {
  DCHECK(CalledOnValidThread());
  if (!cc::OutputSurface::BindToClient(surface_client))
    return false;

  client_->SetMemoryPolicy(memory_policy_);
  client_->SetTreeActivationCallback(
      base::Bind(&SynchronousCompositorOutputSurface::DidActivatePendingTree,
                 base::Unretained(this)));
  registry_->RegisterOutputSurface(routing_id_, this);
  registered_ = true;

  surface_manager_->RegisterSurfaceFactoryClient(
      surface_id_allocator_->id_namespace(), this);

  cc::RendererSettings software_renderer_settings;

  // The shared_bitmap_manager and gpu_memory_buffer_manager here are null as
  // this Display is only used for resourcesless software draws, where no
  // resources are included in the frame swapped from the compositor. So there
  // is no need for these.
  display_.reset(new cc::Display(
      surface_manager_.get(), nullptr /* shared_bitmap_manager */,
      nullptr /* gpu_memory_buffer_manager */, software_renderer_settings,
      surface_id_allocator_->id_namespace(), nullptr /* begin_frame_source */,
      base::MakeUnique<SoftwareOutputSurface>(
          base::MakeUnique<SoftwareDevice>(&current_sw_canvas_)),
      nullptr /* scheduler */, nullptr /* texture_mailbox_deleter */));
  display_->Initialize(&display_client_);
  return true;
}

void SynchronousCompositorOutputSurface::DetachFromClient() {
  DCHECK(CalledOnValidThread());
  if (registered_) {
    registry_->UnregisterOutputSurface(routing_id_, this);
  }
  client_->SetTreeActivationCallback(base::Closure());
  if (!delegated_surface_id_.is_null())
    surface_factory_->Destroy(delegated_surface_id_);
  surface_manager_->UnregisterSurfaceFactoryClient(
      surface_id_allocator_->id_namespace());
  display_ = nullptr;
  surface_factory_ = nullptr;
  surface_id_allocator_ = nullptr;
  surface_manager_ = nullptr;
  cc::OutputSurface::DetachFromClient();
  CancelFallbackTick();
}

void SynchronousCompositorOutputSurface::Reshape(
    const gfx::Size& size,
    float scale_factor,
    const gfx::ColorSpace& color_space,
    bool has_alpha) {
  // Intentional no-op: surface size is controlled by the embedder.
}

static void NoOpDrawCallback(cc::SurfaceDrawStatus s) {}

void SynchronousCompositorOutputSurface::SwapBuffers(
    cc::CompositorFrame frame) {
  DCHECK(CalledOnValidThread());
  DCHECK(sync_client_);

  if (fallback_tick_running_) {
    client_->DidSwapBuffers();
    client_->DidSwapBuffersComplete();
    DCHECK(frame.delegated_frame_data->resource_list.empty());
    cc::ReturnedResourceArray return_resources;
    ReturnResources(return_resources);
    return;
  }

  cc::CompositorFrame swap_frame;

  if (in_software_draw_) {
    // The frame we send to the client is actually just the metadata. Preserve
    // the |frame| for the software path below.
    swap_frame.metadata = frame.metadata.Clone();

    if (delegated_surface_id_.is_null()) {
      delegated_surface_id_ = surface_id_allocator_->GenerateId();
      surface_factory_->Create(delegated_surface_id_);
    }

    display_->SetSurfaceId(delegated_surface_id_,
                           frame.metadata.device_scale_factor);

    gfx::Size frame_size =
        frame.delegated_frame_data->render_pass_list.back()->output_rect.size();
    display_->Resize(frame_size);

    surface_factory_->SubmitCompositorFrame(
        delegated_surface_id_, std::move(frame), base::Bind(&NoOpDrawCallback));
    display_->DrawAndSwap();
  } else {
    // For hardware draws we send the whole frame to the client so it can draw
    // the content in it.
    swap_frame = std::move(frame);
  }

  sync_client_->SwapBuffers(output_surface_id_, std::move(swap_frame));
  DeliverMessages();
  client_->DidSwapBuffers();
  did_swap_ = true;
}

void SynchronousCompositorOutputSurface::CancelFallbackTick() {
  fallback_tick_.Cancel();
  fallback_tick_pending_ = false;
}

void SynchronousCompositorOutputSurface::FallbackTickFired() {
  DCHECK(CalledOnValidThread());
  TRACE_EVENT0("renderer",
               "SynchronousCompositorOutputSurface::FallbackTickFired");
  base::AutoReset<bool> in_fallback_tick(&fallback_tick_running_, true);
  SkBitmap bitmap;
  bitmap.allocN32Pixels(1, 1);
  bitmap.eraseColor(0);
  SkCanvas canvas(bitmap);
  fallback_tick_pending_ = false;
  DemandDrawSw(&canvas);
}

void SynchronousCompositorOutputSurface::Invalidate() {
  DCHECK(CalledOnValidThread());
  if (sync_client_)
    sync_client_->Invalidate();

  if (!fallback_tick_pending_) {
    fallback_tick_.Reset(
        base::Bind(&SynchronousCompositorOutputSurface::FallbackTickFired,
                   base::Unretained(this)));
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, fallback_tick_.callback(),
        base::TimeDelta::FromMilliseconds(kFallbackTickTimeoutInMilliseconds));
    fallback_tick_pending_ = true;
  }
}

void SynchronousCompositorOutputSurface::BindFramebuffer() {
  // This is a delegating output surface, no framebuffer/direct drawing support.
  NOTREACHED();
}

uint32_t SynchronousCompositorOutputSurface::GetFramebufferCopyTextureFormat() {
  // This is a delegating output surface, no framebuffer/direct drawing support.
  NOTREACHED();
  return 0;
}

void SynchronousCompositorOutputSurface::DemandDrawHw(
    const gfx::Size& surface_size,
    const gfx::Transform& transform,
    const gfx::Rect& viewport,
    const gfx::Rect& clip,
    const gfx::Rect& viewport_rect_for_tile_priority,
    const gfx::Transform& transform_for_tile_priority) {
  DCHECK(CalledOnValidThread());
  DCHECK(HasClient());
  DCHECK(context_provider_.get());
  CancelFallbackTick();

  surface_size_ = surface_size;
  client_->SetExternalTilePriorityConstraints(viewport_rect_for_tile_priority,
                                              transform_for_tile_priority);
  InvokeComposite(transform, viewport, clip);
}

void SynchronousCompositorOutputSurface::DemandDrawSw(SkCanvas* canvas) {
  DCHECK(CalledOnValidThread());
  DCHECK(canvas);
  DCHECK(!current_sw_canvas_);
  CancelFallbackTick();

  base::AutoReset<SkCanvas*> canvas_resetter(&current_sw_canvas_, canvas);

  SkIRect canvas_clip;
  canvas->getClipDeviceBounds(&canvas_clip);
  gfx::Rect clip = gfx::SkIRectToRect(canvas_clip);

  gfx::Transform transform(gfx::Transform::kSkipInitialization);
  transform.matrix() = canvas->getTotalMatrix();  // Converts 3x3 matrix to 4x4.

  surface_size_ = gfx::Size(canvas->getBaseLayerSize().width(),
                            canvas->getBaseLayerSize().height());
  base::AutoReset<bool> set_in_software_draw(&in_software_draw_, true);
  InvokeComposite(transform, clip, clip);
}

void SynchronousCompositorOutputSurface::InvokeComposite(
    const gfx::Transform& transform,
    const gfx::Rect& viewport,
    const gfx::Rect& clip) {
  gfx::Transform adjusted_transform = transform;
  adjusted_transform.matrix().postTranslate(-viewport.x(), -viewport.y(), 0);
  did_swap_ = false;
  client_->OnDraw(adjusted_transform, viewport, clip, in_software_draw_);

  if (did_swap_)
    client_->DidSwapBuffersComplete();
}

void SynchronousCompositorOutputSurface::OnReclaimResources(
    uint32_t output_surface_id,
    const cc::CompositorFrameAck& ack) {
  // Ignore message if it's a stale one coming from a different output surface
  // (e.g. after a lost context).
  if (output_surface_id != output_surface_id_)
    return;
  ReclaimResources(&ack);
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

void SynchronousCompositorOutputSurface::DidActivatePendingTree() {
  DCHECK(CalledOnValidThread());
  if (sync_client_)
    sync_client_->DidActivatePendingTree();
  DeliverMessages();
}

void SynchronousCompositorOutputSurface::DeliverMessages() {
  std::vector<std::unique_ptr<IPC::Message>> messages;
  std::unique_ptr<FrameSwapMessageQueue::SendMessageScope> send_message_scope =
      frame_swap_message_queue_->AcquireSendMessageScope();
  frame_swap_message_queue_->DrainMessages(&messages);
  for (auto& msg : messages) {
    Send(msg.release());
  }
}

bool SynchronousCompositorOutputSurface::Send(IPC::Message* message) {
  DCHECK(CalledOnValidThread());
  return sender_->Send(message);
}

bool SynchronousCompositorOutputSurface::CalledOnValidThread() const {
  return thread_checker_.CalledOnValidThread();
}

void SynchronousCompositorOutputSurface::ReturnResources(
    const cc::ReturnedResourceArray& resources) {
  DCHECK(resources.empty());
  cc::CompositorFrameAck ack;
  client_->ReclaimResources(&ack);
}

void SynchronousCompositorOutputSurface::SetBeginFrameSource(
    cc::BeginFrameSource* begin_frame_source) {
  // Software output is synchronous and doesn't use a BeginFrameSource.
  NOTREACHED();
}

}  // namespace content
