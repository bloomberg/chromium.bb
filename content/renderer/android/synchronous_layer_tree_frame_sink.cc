// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/android/synchronous_layer_tree_frame_sink.h"

#include <vector>

#include "base/auto_reset.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/layer_tree_frame_sink_client.h"
#include "cc/output/output_surface.h"
#include "cc/output/output_surface_frame.h"
#include "cc/output/software_output_device.h"
#include "cc/output/texture_mailbox_deleter.h"
#include "cc/quads/render_pass.h"
#include "cc/quads/surface_draw_quad.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/common/surfaces/local_surface_id_allocator.h"
#include "components/viz/service/display/display.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "content/common/android/sync_compositor_messages.h"
#include "content/common/view_messages.h"
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
const viz::FrameSinkId kRootFrameSinkId(1, 1);
const viz::FrameSinkId kChildFrameSinkId(1, 2);

// Do not limit number of resources, so use an unrealistically high value.
const size_t kNumResourcesLimit = 10 * 1000 * 1000;

class SoftwareDevice : public cc::SoftwareOutputDevice {
 public:
  SoftwareDevice(SkCanvas** canvas) : canvas_(canvas) {}

  void Resize(const gfx::Size& pixel_size, float device_scale_factor) override {
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

}  // namespace

class SynchronousLayerTreeFrameSink::SoftwareOutputSurface
    : public cc::OutputSurface {
 public:
  SoftwareOutputSurface(std::unique_ptr<SoftwareDevice> software_device)
      : cc::OutputSurface(std::move(software_device)) {}

  // cc::OutputSurface implementation.
  void BindToClient(cc::OutputSurfaceClient* client) override {}
  void EnsureBackbuffer() override {}
  void DiscardBackbuffer() override {}
  void BindFramebuffer() override {}
  void SetDrawRectangle(const gfx::Rect& rect) override {}
  void SwapBuffers(cc::OutputSurfaceFrame frame) override {}
  void Reshape(const gfx::Size& size,
               float scale_factor,
               const gfx::ColorSpace& color_space,
               bool has_alpha,
               bool use_stencil) override {}
  uint32_t GetFramebufferCopyTextureFormat() override { return 0; }
  cc::OverlayCandidateValidator* GetOverlayCandidateValidator() const override {
    return nullptr;
  }
  bool IsDisplayedAsOverlayPlane() const override { return false; }
  unsigned GetOverlayTextureId() const override { return 0; }
  gfx::BufferFormat GetOverlayBufferFormat() const override {
    return gfx::BufferFormat::RGBX_8888;
  }
  bool SurfaceIsSuspendForRecycle() const override { return false; }
  bool HasExternalStencilTest() const override { return false; }
  void ApplyExternalStencil() override {}
};

SynchronousLayerTreeFrameSink::SynchronousLayerTreeFrameSink(
    scoped_refptr<viz::ContextProvider> context_provider,
    scoped_refptr<viz::ContextProvider> worker_context_provider,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    viz::SharedBitmapManager* shared_bitmap_manager,
    int routing_id,
    uint32_t layer_tree_frame_sink_id,
    std::unique_ptr<viz::BeginFrameSource> begin_frame_source,
    SynchronousCompositorRegistry* registry,
    scoped_refptr<FrameSwapMessageQueue> frame_swap_message_queue)
    : cc::LayerTreeFrameSink(std::move(context_provider),
                             std::move(worker_context_provider),
                             gpu_memory_buffer_manager,
                             nullptr),
      routing_id_(routing_id),
      layer_tree_frame_sink_id_(layer_tree_frame_sink_id),
      registry_(registry),
      shared_bitmap_manager_(shared_bitmap_manager),
      sender_(RenderThreadImpl::current()->sync_compositor_message_filter()),
      memory_policy_(0u),
      frame_swap_message_queue_(frame_swap_message_queue),
      local_surface_id_allocator_(new viz::LocalSurfaceIdAllocator),
      begin_frame_source_(std::move(begin_frame_source)) {
  DCHECK(registry_);
  DCHECK(sender_);
  DCHECK(begin_frame_source_);
  thread_checker_.DetachFromThread();
  memory_policy_.priority_cutoff_when_visible =
      gpu::MemoryAllocation::CUTOFF_ALLOW_NICE_TO_HAVE;
}

SynchronousLayerTreeFrameSink::~SynchronousLayerTreeFrameSink() = default;

void SynchronousLayerTreeFrameSink::SetSyncClient(
    SynchronousLayerTreeFrameSinkClient* compositor) {
  DCHECK(CalledOnValidThread());
  sync_client_ = compositor;
  if (sync_client_)
    Send(new SyncCompositorHostMsg_LayerTreeFrameSinkCreated(routing_id_));
}

bool SynchronousLayerTreeFrameSink::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(SynchronousLayerTreeFrameSink, message)
    IPC_MESSAGE_HANDLER(SyncCompositorMsg_SetMemoryPolicy, SetMemoryPolicy)
    IPC_MESSAGE_HANDLER(SyncCompositorMsg_ReclaimResources, OnReclaimResources)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

bool SynchronousLayerTreeFrameSink::BindToClient(
    cc::LayerTreeFrameSinkClient* sink_client) {
  DCHECK(CalledOnValidThread());
  if (!cc::LayerTreeFrameSink::BindToClient(sink_client))
    return false;

  frame_sink_manager_ = std::make_unique<viz::FrameSinkManagerImpl>(
      viz::SurfaceManager::LifetimeType::SEQUENCES);

  DCHECK(begin_frame_source_);
  client_->SetBeginFrameSource(begin_frame_source_.get());
  client_->SetMemoryPolicy(memory_policy_);
  client_->SetTreeActivationCallback(
      base::Bind(&SynchronousLayerTreeFrameSink::DidActivatePendingTree,
                 base::Unretained(this)));
  registry_->RegisterLayerTreeFrameSink(routing_id_, this);

  constexpr bool root_support_is_root = true;
  constexpr bool child_support_is_root = false;
  constexpr bool needs_sync_points = true;
  root_support_ = viz::CompositorFrameSinkSupport::Create(
      this, frame_sink_manager_.get(), kRootFrameSinkId, root_support_is_root,
      needs_sync_points);
  child_support_ = viz::CompositorFrameSinkSupport::Create(
      this, frame_sink_manager_.get(), kChildFrameSinkId, child_support_is_root,
      needs_sync_points);

  viz::RendererSettings software_renderer_settings;

  auto output_surface = base::MakeUnique<SoftwareOutputSurface>(
      base::MakeUnique<SoftwareDevice>(&current_sw_canvas_));
  software_output_surface_ = output_surface.get();

  // The gpu_memory_buffer_manager here is null as the Display is only used for
  // resourcesless software draws, where no resources are included in the frame
  // swapped from the compositor. So there is no need for it.
  // The shared_bitmap_manager_ is provided for the Display to allocate
  // resources.
  // TODO(crbug.com/692814): The Display never sends its resources out of
  // process so there is no reason for it to use a SharedBitmapManager.
  display_ = base::MakeUnique<viz::Display>(
      shared_bitmap_manager_, nullptr /* gpu_memory_buffer_manager */,
      software_renderer_settings, kRootFrameSinkId, std::move(output_surface),
      nullptr /* scheduler */, nullptr /* texture_mailbox_deleter */);
  display_->Initialize(&display_client_,
                       frame_sink_manager_->surface_manager());
  display_->SetVisible(true);
  return true;
}

void SynchronousLayerTreeFrameSink::DetachFromClient() {
  DCHECK(CalledOnValidThread());
  client_->SetBeginFrameSource(nullptr);
  // Destroy the begin frame source on the same thread it was bound on.
  begin_frame_source_ = nullptr;
  registry_->UnregisterLayerTreeFrameSink(routing_id_, this);
  client_->SetTreeActivationCallback(base::Closure());
  root_support_.reset();
  child_support_.reset();
  software_output_surface_ = nullptr;
  display_ = nullptr;
  local_surface_id_allocator_ = nullptr;
  frame_sink_manager_ = nullptr;
  cc::LayerTreeFrameSink::DetachFromClient();
  CancelFallbackTick();
}

void SynchronousLayerTreeFrameSink::SubmitCompositorFrame(
    cc::CompositorFrame frame) {
  DCHECK(CalledOnValidThread());
  DCHECK(sync_client_);

  if (fallback_tick_running_) {
    DCHECK(frame.resource_list.empty());
    std::vector<viz::ReturnedResource> return_resources;
    ReclaimResources(return_resources);
    did_submit_frame_ = true;
    return;
  }

  cc::CompositorFrame submit_frame;

  if (in_software_draw_) {
    // The frame we send to the client is actually just the metadata. Preserve
    // the |frame| for the software path below.
    submit_frame.metadata = frame.metadata.Clone();

    // The layer compositor should be giving a frame that covers the
    // |sw_viewport_for_current_draw_| but at 0,0.
    gfx::Size child_size = sw_viewport_for_current_draw_.size();
    DCHECK(gfx::Rect(child_size) == frame.render_pass_list.back()->output_rect);

    // Make a size that covers from 0,0 and includes the area coming from the
    // layer compositor.
    gfx::Size display_size(sw_viewport_for_current_draw_.right(),
                           sw_viewport_for_current_draw_.bottom());
    display_->Resize(display_size);

    if (!root_local_surface_id_.is_valid() || display_size_ != display_size ||
        device_scale_factor_ != frame.metadata.device_scale_factor) {
      root_local_surface_id_ = local_surface_id_allocator_->GenerateId();
      display_size_ = display_size;
      device_scale_factor_ = frame.metadata.device_scale_factor;
    }

    if (!child_local_surface_id_.is_valid() || child_size_ != child_size ||
        device_scale_factor_ != frame.metadata.device_scale_factor) {
      child_local_surface_id_ = local_surface_id_allocator_->GenerateId();
      child_size_ = child_size;
      device_scale_factor_ = frame.metadata.device_scale_factor;
    }

    display_->SetLocalSurfaceId(root_local_surface_id_,
                                frame.metadata.device_scale_factor);

    // The offset for the child frame relative to the origin of the canvas being
    // drawn into.
    gfx::Transform child_transform;
    child_transform.Translate(
        gfx::Vector2dF(sw_viewport_for_current_draw_.OffsetFromOrigin()));

    // Make a root frame that embeds the frame coming from the layer compositor
    // and positions it based on the provided viewport.
    // TODO(danakj): We could apply the transform here instead of passing it to
    // the LayerTreeFrameSink client too? (We'd have to do the same for
    // hardware frames in SurfacesInstance?)
    cc::CompositorFrame embed_frame;
    embed_frame.metadata.begin_frame_ack = frame.metadata.begin_frame_ack;
    embed_frame.metadata.device_scale_factor =
        frame.metadata.device_scale_factor;
    embed_frame.render_pass_list.push_back(cc::RenderPass::Create());

    // The embedding RenderPass covers the entire Display's area.
    const auto& embed_render_pass = embed_frame.render_pass_list.back();
    embed_render_pass->SetNew(1, gfx::Rect(display_size),
                              gfx::Rect(display_size), gfx::Transform());
    embed_render_pass->has_transparent_background = false;

    // The RenderPass has a single SurfaceDrawQuad (and SharedQuadState for it).
    auto* shared_quad_state =
        embed_render_pass->CreateAndAppendSharedQuadState();
    auto* surface_quad =
        embed_render_pass->CreateAndAppendDrawQuad<cc::SurfaceDrawQuad>();
    shared_quad_state->SetAll(
        child_transform, gfx::Rect(child_size), gfx::Rect(child_size),
        gfx::Rect() /* clip_rect */, false /* is_clipped */, 1.f /* opacity */,
        SkBlendMode::kSrcOver, 0 /* sorting_context_id */);
    surface_quad->SetNew(
        shared_quad_state, gfx::Rect(child_size), gfx::Rect(child_size),
        viz::SurfaceId(kChildFrameSinkId, child_local_surface_id_),
        cc::SurfaceDrawQuadType::PRIMARY, nullptr);

    bool result = child_support_->SubmitCompositorFrame(child_local_surface_id_,
                                                        std::move(frame));
    DCHECK(result);
    result = root_support_->SubmitCompositorFrame(root_local_surface_id_,
                                                  std::move(embed_frame));
    DCHECK(result);
    display_->DrawAndSwap();
  } else {
    // For hardware draws we send the whole frame to the client so it can draw
    // the content in it.
    submit_frame = std::move(frame);
  }

  sync_client_->SubmitCompositorFrame(layer_tree_frame_sink_id_,
                                      std::move(submit_frame));
  did_submit_frame_ = true;
}

void SynchronousLayerTreeFrameSink::DidNotProduceFrame(
    const viz::BeginFrameAck& ack) {
  DCHECK(!ack.has_damage);
  DCHECK_LE(viz::BeginFrameArgs::kStartingFrameNumber, ack.sequence_number);
  Send(new ViewHostMsg_DidNotProduceFrame(routing_id_, ack));
}

void SynchronousLayerTreeFrameSink::CancelFallbackTick() {
  fallback_tick_.Cancel();
  fallback_tick_pending_ = false;
}

void SynchronousLayerTreeFrameSink::FallbackTickFired() {
  DCHECK(CalledOnValidThread());
  TRACE_EVENT0("renderer", "SynchronousLayerTreeFrameSink::FallbackTickFired");
  base::AutoReset<bool> in_fallback_tick(&fallback_tick_running_, true);
  frame_swap_message_queue_->NotifyFramesAreDiscarded(true);
  SkBitmap bitmap;
  bitmap.allocN32Pixels(1, 1);
  bitmap.eraseColor(0);
  SkCanvas canvas(bitmap);
  fallback_tick_pending_ = false;
  DemandDrawSw(&canvas);
  frame_swap_message_queue_->NotifyFramesAreDiscarded(false);
}

void SynchronousLayerTreeFrameSink::Invalidate() {
  DCHECK(CalledOnValidThread());
  if (sync_client_)
    sync_client_->Invalidate();

  if (!fallback_tick_pending_) {
    fallback_tick_.Reset(
        base::Bind(&SynchronousLayerTreeFrameSink::FallbackTickFired,
                   base::Unretained(this)));
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, fallback_tick_.callback(),
        base::TimeDelta::FromMilliseconds(kFallbackTickTimeoutInMilliseconds));
    fallback_tick_pending_ = true;
  }
}

void SynchronousLayerTreeFrameSink::DemandDrawHw(
    const gfx::Size& viewport_size,
    const gfx::Rect& viewport_rect_for_tile_priority,
    const gfx::Transform& transform_for_tile_priority) {
  DCHECK(CalledOnValidThread());
  DCHECK(HasClient());
  DCHECK(context_provider_.get());
  CancelFallbackTick();

  client_->SetExternalTilePriorityConstraints(viewport_rect_for_tile_priority,
                                              transform_for_tile_priority);
  InvokeComposite(gfx::Transform(), gfx::Rect(viewport_size));
}

void SynchronousLayerTreeFrameSink::DemandDrawSw(SkCanvas* canvas) {
  DCHECK(CalledOnValidThread());
  DCHECK(canvas);
  DCHECK(!current_sw_canvas_);
  CancelFallbackTick();

  base::AutoReset<SkCanvas*> canvas_resetter(&current_sw_canvas_, canvas);

  SkIRect canvas_clip = canvas->getDeviceClipBounds();
  gfx::Rect viewport = gfx::SkIRectToRect(canvas_clip);

  gfx::Transform transform(gfx::Transform::kSkipInitialization);
  transform.matrix() = canvas->getTotalMatrix();  // Converts 3x3 matrix to 4x4.

  // We will resize the Display to ensure it covers the entire |viewport|, so
  // save it for later.
  sw_viewport_for_current_draw_ = viewport;

  base::AutoReset<bool> set_in_software_draw(&in_software_draw_, true);
  InvokeComposite(transform, viewport);
}

void SynchronousLayerTreeFrameSink::InvokeComposite(
    const gfx::Transform& transform,
    const gfx::Rect& viewport) {
  did_submit_frame_ = false;
  // Adjust transform so that the layer compositor draws the |viewport| rect
  // at its origin. The offset of the |viewport| we pass to the layer compositor
  // is ignored for drawing, so its okay to not match the transform.
  // TODO(danakj): Why do we pass a viewport origin and then not really use it
  // (only for comparing to the viewport passed in
  // SetExternalTilePriorityConstraints), surely this could be more clear?
  gfx::Transform adjusted_transform = transform;
  adjusted_transform.matrix().postTranslate(-viewport.x(), -viewport.y(), 0);
  client_->OnDraw(adjusted_transform, viewport, in_software_draw_);

  if (did_submit_frame_) {
    // This must happen after unwinding the stack and leaving the compositor.
    // Usually it is a separate task but we just defer it until OnDraw completes
    // instead.
    client_->DidReceiveCompositorFrameAck();
  }
}

void SynchronousLayerTreeFrameSink::OnReclaimResources(
    uint32_t layer_tree_frame_sink_id,
    const std::vector<viz::ReturnedResource>& resources) {
  // Ignore message if it's a stale one coming from a different output surface
  // (e.g. after a lost context).
  if (layer_tree_frame_sink_id != layer_tree_frame_sink_id_)
    return;
  client_->ReclaimResources(resources);
}

void SynchronousLayerTreeFrameSink::SetMemoryPolicy(size_t bytes_limit) {
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

void SynchronousLayerTreeFrameSink::DidActivatePendingTree() {
  DCHECK(CalledOnValidThread());
  if (sync_client_)
    sync_client_->DidActivatePendingTree();
  DeliverMessages();
}

void SynchronousLayerTreeFrameSink::DeliverMessages() {
  std::vector<std::unique_ptr<IPC::Message>> messages;
  std::unique_ptr<FrameSwapMessageQueue::SendMessageScope> send_message_scope =
      frame_swap_message_queue_->AcquireSendMessageScope();
  frame_swap_message_queue_->DrainMessages(&messages);
  for (auto& msg : messages) {
    Send(msg.release());
  }
}

bool SynchronousLayerTreeFrameSink::Send(IPC::Message* message) {
  DCHECK(CalledOnValidThread());
  return sender_->Send(message);
}

bool SynchronousLayerTreeFrameSink::CalledOnValidThread() const {
  return thread_checker_.CalledOnValidThread();
}

void SynchronousLayerTreeFrameSink::DidReceiveCompositorFrameAck(
    const std::vector<viz::ReturnedResource>& resources) {
  ReclaimResources(resources);
}

void SynchronousLayerTreeFrameSink::OnBeginFrame(
    const viz::BeginFrameArgs& args) {}

void SynchronousLayerTreeFrameSink::ReclaimResources(
    const std::vector<viz::ReturnedResource>& resources) {
  DCHECK(resources.empty());
  client_->ReclaimResources(resources);
}

void SynchronousLayerTreeFrameSink::WillDrawSurface(
    const viz::LocalSurfaceId& local_surface_id,
    const gfx::Rect& damage_rect) {}

void SynchronousLayerTreeFrameSink::OnBeginFramePausedChanged(bool paused) {}

}  // namespace content
