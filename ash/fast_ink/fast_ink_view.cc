// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/fast_ink/fast_ink_view.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>

#include <memory>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/shell_observer.h"
#include "base/threading/thread_task_runner_handle.h"
#include "cc/base/math_util.h"
#include "cc/trees/layer_tree_frame_sink.h"
#include "cc/trees/layer_tree_frame_sink_client.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/layout.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

// The outset used to expand the surface damage rectangle in order to
// allow single buffer updates while a frame is in-flight.
const int kSurfaceDamageOutsetDIP = 100;

}  // namespace

struct FastInkView::Resource {
  Resource() {}
  ~Resource() {
    // context_provider might be null in unit tests when ran with --mash
    // TODO(kaznacheev) Have MASH provide a context provider for tests
    // when crbug/772562 is fixed
    if (!context_provider)
      return;
    gpu::gles2::GLES2Interface* gles2 = context_provider->ContextGL();
    if (sync_token.HasData())
      gles2->WaitSyncTokenCHROMIUM(sync_token.GetConstData());
    if (texture)
      gles2->DeleteTextures(1, &texture);
    if (image)
      gles2->DestroyImageCHROMIUM(image);
  }
  scoped_refptr<viz::ContextProvider> context_provider;
  uint32_t texture = 0;
  uint32_t image = 0;
  gpu::Mailbox mailbox;
  gpu::SyncToken sync_token;
};

class FastInkView::LayerTreeFrameSinkHolder
    : public cc::LayerTreeFrameSinkClient,
      public ash::ShellObserver {
 public:
  LayerTreeFrameSinkHolder(FastInkView* view,
                           std::unique_ptr<cc::LayerTreeFrameSink> frame_sink)
      : view_(view), frame_sink_(std::move(frame_sink)) {
    frame_sink_->BindToClient(this);
  }
  ~LayerTreeFrameSinkHolder() override {
    if (frame_sink_)
      frame_sink_->DetachFromClient();
    if (shell_)
      shell_->RemoveShellObserver(this);
  }

  // Delete frame sink after having reclaimed all exported resources.
  // TODO(reveman): Find a better way to handle deletion of in-flight resources.
  // crbug.com/765763
  static void DeleteWhenLastResourceHasBeenReclaimed(
      std::unique_ptr<LayerTreeFrameSinkHolder> holder) {
    // Submit an empty frame to ensure that pending release callbacks will be
    // processed in a finite amount of time.
    viz::CompositorFrame frame;
    frame.metadata.begin_frame_ack.source_id =
        viz::BeginFrameArgs::kManualSourceId;
    frame.metadata.begin_frame_ack.sequence_number =
        viz::BeginFrameArgs::kStartingFrameNumber;
    frame.metadata.begin_frame_ack.has_damage = true;
    frame.metadata.device_scale_factor =
        holder->last_frame_device_scale_factor_;
    std::unique_ptr<viz::RenderPass> pass = viz::RenderPass::Create();
    pass->SetNew(1, gfx::Rect(holder->last_frame_size_in_pixels_), gfx::Rect(),
                 gfx::Transform());
    frame.render_pass_list.push_back(std::move(pass));
    holder->frame_sink_->SubmitCompositorFrame(std::move(frame));

    // Delete sink holder immediately if not waiting for exported resources to
    // be reclaimed.
    if (holder->exported_resources_.empty())
      return;

    ash::Shell* shell = ash::Shell::Get();
    holder->shell_ = shell;
    holder->view_ = nullptr;

    // If we have exported resources to reclaim then extend the lifetime of
    // holder by adding it as a shell observer. The holder will delete itself
    // when shell shuts down or when all exported resources have been reclaimed.
    shell->AddShellObserver(holder.release());
  }

  void SubmitCompositorFrame(viz::CompositorFrame frame,
                             viz::ResourceId resource_id,
                             std::unique_ptr<Resource> resource) {
    exported_resources_[resource_id] = std::move(resource);
    last_frame_size_in_pixels_ = frame.size_in_pixels();
    last_frame_device_scale_factor_ = frame.metadata.device_scale_factor;
    frame_sink_->SubmitCompositorFrame(std::move(frame));
  }

  // Overridden from cc::LayerTreeFrameSinkClient:
  void SetBeginFrameSource(viz::BeginFrameSource* source) override {}
  void ReclaimResources(
      const std::vector<viz::ReturnedResource>& resources) override {
    for (auto& entry : resources) {
      auto it = exported_resources_.find(entry.id);
      DCHECK(it != exported_resources_.end());
      std::unique_ptr<Resource> resource = std::move(it->second);
      exported_resources_.erase(it);
      resource->sync_token = entry.sync_token;
      if (view_ && !entry.lost)
        view_->ReclaimResource(std::move(resource));
    }

    if (shell_ && exported_resources_.empty())
      ScheduleDelete();
  }
  void SetTreeActivationCallback(const base::Closure& callback) override {}
  void DidReceiveCompositorFrameAck() override {
    if (view_)
      view_->DidReceiveCompositorFrameAck();
  }
  void DidLoseLayerTreeFrameSink() override {
    exported_resources_.clear();
    if (shell_)
      ScheduleDelete();
  }
  void OnDraw(const gfx::Transform& transform,
              const gfx::Rect& viewport,
              bool resourceless_software_draw) override {}
  void SetMemoryPolicy(const cc::ManagedMemoryPolicy& policy) override {}
  void SetExternalTilePriorityConstraints(
      const gfx::Rect& viewport_rect,
      const gfx::Transform& transform) override {}

  // Overridden from ash::ShellObserver:
  void OnShellDestroyed() override {
    shell_->RemoveShellObserver(this);
    shell_ = nullptr;
    // Make sure frame sink never outlives the shell.
    frame_sink_->DetachFromClient();
    frame_sink_.reset();
    ScheduleDelete();
  }

 private:
  void ScheduleDelete() {
    if (delete_pending_)
      return;
    delete_pending_ = true;
    base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
  }

  FastInkView* view_;
  std::unique_ptr<cc::LayerTreeFrameSink> frame_sink_;
  base::flat_map<viz::ResourceId, std::unique_ptr<Resource>>
      exported_resources_;
  gfx::Size last_frame_size_in_pixels_;
  float last_frame_device_scale_factor_ = 1.0f;
  ash::Shell* shell_ = nullptr;
  bool delete_pending_ = false;

  DISALLOW_COPY_AND_ASSIGN(LayerTreeFrameSinkHolder);
};

FastInkView::FastInkView(aura::Window* root_window) : weak_ptr_factory_(this) {
  widget_.reset(new views::Widget);
  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
  params.name = "FastInkOverlay";
  params.accept_events = false;
  params.activatable = views::Widget::InitParams::ACTIVATABLE_NO;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.parent =
      Shell::GetContainer(root_window, kShellWindowId_OverlayContainer);
  params.layer_type = ui::LAYER_SOLID_COLOR;

  widget_->Init(params);
  widget_->Show();
  widget_->SetContentsView(this);
  widget_->SetBounds(root_window->GetBoundsInScreen());
  set_owned_by_client();

  // Take the root transform and apply this during buffer update instead of
  // leaving this up to the compositor. The benefit is that HW requirements
  // for being able to take advantage of overlays and direct scanout are
  // reduced significantly. Frames are submitted to the compositor with the
  // inverse transform to cancel out the transformation that would otherwise
  // be done by the compositor.
  screen_to_buffer_transform_ =
      widget_->GetNativeWindow()->GetHost()->GetRootTransform();

  frame_sink_holder_ = base::MakeUnique<LayerTreeFrameSinkHolder>(
      this, widget_->GetNativeView()->CreateLayerTreeFrameSink());
}

FastInkView::~FastInkView() {
  LayerTreeFrameSinkHolder::DeleteWhenLastResourceHasBeenReclaimed(
      std::move(frame_sink_holder_));
}

void FastInkView::UpdateDamageRect(const gfx::Rect& rect) {
  buffer_damage_rect_.Union(rect);
}

void FastInkView::RequestRedraw() {
  if (pending_redraw_)
    return;

  pending_redraw_ = true;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&FastInkView::Redraw, weak_ptr_factory_.GetWeakPtr()));
}

void FastInkView::Redraw() {
  DCHECK(pending_redraw_);
  pending_redraw_ = false;

  if (!buffer_damage_rect_.IsEmpty()) {
    // Defer buffer update if damage exceeds the bounds of the pending
    // draw surface rectangle. This prevents visible artifacts at the
    // border of the draw surface rectangle when compositing.
    // Note: Draw surface rectangle is expanded to prevent this from
    // causing a performance problem during normal usage.
    if (pending_draw_surface_rect_.IsEmpty() ||
        pending_draw_surface_rect_.Contains(buffer_damage_rect_)) {
      UpdateBuffer();
    }
  }

  if (!surface_damage_rect_.IsEmpty()) {
    // Defer surface update if a frame is already in-flight.
    if (!pending_draw_surface_)
      UpdateSurface();
  }
}

void FastInkView::UpdateBuffer() {
  TRACE_EVENT1("ui", "FastInkView::UpdateBuffer", "damage",
               buffer_damage_rect_.ToString());

  gfx::Rect screen_bounds = widget_->GetNativeView()->GetBoundsInScreen();
  gfx::Rect update_rect = buffer_damage_rect_;
  buffer_damage_rect_ = gfx::Rect();

  // Create and map a single GPU memory buffer. The fast ink will be
  // written into this buffer without any buffering. The result is that we
  // might be modifying the buffer while it's being displayed. This provides
  // minimal latency but potential tearing. Note that we have to draw into
  // a temporary surface and copy it into GPU memory buffer to avoid flicker.
  if (!gpu_memory_buffer_) {
    TRACE_EVENT0("ui", "FastInkView::UpdateBuffer::Create");

    gpu_memory_buffer_ =
        aura::Env::GetInstance()
            ->context_factory()
            ->GetGpuMemoryBufferManager()
            ->CreateGpuMemoryBuffer(
                gfx::ToEnclosedRect(cc::MathUtil::MapClippedRect(
                                        screen_to_buffer_transform_,
                                        gfx::RectF(screen_bounds.width(),
                                                   screen_bounds.height())))
                    .size(),
                SK_B32_SHIFT ? gfx::BufferFormat::RGBA_8888
                             : gfx::BufferFormat::BGRA_8888,
                gfx::BufferUsage::SCANOUT_CPU_READ_WRITE,
                gpu::kNullSurfaceHandle);
    if (!gpu_memory_buffer_) {
      LOG(ERROR) << "Failed to allocate GPU memory buffer";
      return;
    }

    // Make sure the first update rectangle covers the whole buffer.
    update_rect = gfx::Rect(screen_bounds.size());
  }

  // Convert update rectangle to pixel coordinates.
  gfx::Rect pixel_rect = cc::MathUtil::MapEnclosingClippedRect(
      screen_to_buffer_transform_, update_rect);

  // Constrain pixel rectangle to buffer size and early out if empty.
  pixel_rect.Intersect(gfx::Rect(gpu_memory_buffer_->GetSize()));
  if (pixel_rect.IsEmpty())
    return;

  // Map buffer for writing.
  if (!gpu_memory_buffer_->Map()) {
    LOG(ERROR) << "Failed to map GPU memory buffer";
    return;
  }

  // Create a temporary canvas for update rectangle.
  gfx::Canvas canvas(pixel_rect.size(), 1.0f, false);
  canvas.Translate(-pixel_rect.OffsetFromOrigin());
  canvas.Transform(screen_to_buffer_transform_);

  {
    TRACE_EVENT1("ui", "FastInkView::UpdateBuffer::Paint", "pixel_rect",
                 pixel_rect.ToString());

    OnRedraw(canvas);
  }

  // Copy result to GPU memory buffer. This is effectively a memcpy and unlike
  // drawing to the buffer directly this ensures that the buffer is never in a
  // state that would result in flicker.
  {
    TRACE_EVENT1("ui", "FastInkView::UpdateBuffer::Copy", "pixel_rect",
                 pixel_rect.ToString());

    uint8_t* data = static_cast<uint8_t*>(gpu_memory_buffer_->memory(0));
    int stride = gpu_memory_buffer_->stride(0);
    canvas.GetBitmap().readPixels(
        SkImageInfo::MakeN32Premul(pixel_rect.width(), pixel_rect.height()),
        data + pixel_rect.y() * stride + pixel_rect.x() * 4, stride, 0, 0);
  }

  // Unmap to flush writes to buffer.
  gpu_memory_buffer_->Unmap();

  // Update surface damage rectangle.
  surface_damage_rect_.Union(update_rect);
}

void FastInkView::UpdateSurface() {
  TRACE_EVENT1("ui", "FastInkView::UpdateSurface", "damage",
               surface_damage_rect_.ToString());

  std::unique_ptr<Resource> resource;
  // Reuse returned resource if available.
  if (!returned_resources_.empty()) {
    resource = std::move(returned_resources_.back());
    returned_resources_.pop_back();
  }

  // Create new resource if needed.
  if (!resource)
    resource = std::make_unique<Resource>();

  // Acquire context provider for resource if needed.
  // Note: We make no attempts to recover if the context provider is later
  // lost. It is expected that this class is short-lived and requiring a
  // new instance to be created in lost context situations is acceptable and
  // keeps the code simple.
  if (!resource->context_provider) {
    resource->context_provider = aura::Env::GetInstance()
                                     ->context_factory()
                                     ->SharedMainThreadContextProvider();
    if (!resource->context_provider) {
      LOG(ERROR) << "Failed to acquire a context provider";
      return;
    }
  }

  gpu::gles2::GLES2Interface* gles2 = resource->context_provider->ContextGL();

  if (resource->sync_token.HasData()) {
    gles2->WaitSyncTokenCHROMIUM(resource->sync_token.GetConstData());
    resource->sync_token = gpu::SyncToken();
  }

  if (resource->texture) {
    gles2->ActiveTexture(GL_TEXTURE0);
    gles2->BindTexture(GL_TEXTURE_2D, resource->texture);
  } else {
    gles2->GenTextures(1, &resource->texture);
    gles2->ActiveTexture(GL_TEXTURE0);
    gles2->BindTexture(GL_TEXTURE_2D, resource->texture);
    gles2->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gles2->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gles2->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gles2->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gles2->GenMailboxCHROMIUM(resource->mailbox.name);
    gles2->ProduceTextureCHROMIUM(GL_TEXTURE_2D, resource->mailbox.name);
  }

  gfx::Size buffer_size = gpu_memory_buffer_->GetSize();

  if (resource->image) {
    gles2->ReleaseTexImage2DCHROMIUM(GL_TEXTURE_2D, resource->image);
  } else {
    resource->image = gles2->CreateImageCHROMIUM(
        gpu_memory_buffer_->AsClientBuffer(), buffer_size.width(),
        buffer_size.height(), SK_B32_SHIFT ? GL_RGBA : GL_BGRA_EXT);
    if (!resource->image) {
      LOG(ERROR) << "Failed to create image";
      return;
    }
  }
  gles2->BindTexImage2DCHROMIUM(GL_TEXTURE_2D, resource->image);

  gpu::SyncToken sync_token;
  uint64_t fence_sync = gles2->InsertFenceSyncCHROMIUM();
  gles2->OrderingBarrierCHROMIUM();
  gles2->GenUnverifiedSyncTokenCHROMIUM(fence_sync, sync_token.GetData());

  viz::TransferableResource transferable_resource;
  transferable_resource.id = next_resource_id_++;
  transferable_resource.format = viz::RGBA_8888;
  transferable_resource.filter = GL_LINEAR;
  transferable_resource.size = buffer_size;
  transferable_resource.mailbox_holder =
      gpu::MailboxHolder(resource->mailbox, sync_token, GL_TEXTURE_2D);
  transferable_resource.is_overlay_candidate = true;

  float device_scale_factor = widget_->GetLayer()->device_scale_factor();
  gfx::Transform target_to_buffer_transform(screen_to_buffer_transform_);
  target_to_buffer_transform.Scale(1.f / device_scale_factor,
                                   1.f / device_scale_factor);

  gfx::Transform buffer_to_target_transform;
  bool rv = target_to_buffer_transform.GetInverse(&buffer_to_target_transform);
  DCHECK(rv);

  gfx::Rect output_rect(gfx::ConvertSizeToPixel(
      device_scale_factor,
      widget_->GetNativeView()->GetBoundsInScreen().size()));
  gfx::Rect quad_rect(buffer_size);
  bool needs_blending = true;

  // Expand surface damage to allow single buffer updates while frame
  // is in-flight.
  surface_damage_rect_.Inset(-kSurfaceDamageOutsetDIP,
                             -kSurfaceDamageOutsetDIP);
  pending_draw_surface_rect_ = surface_damage_rect_;

  gfx::Rect damage_rect =
      gfx::ConvertRectToPixel(device_scale_factor, surface_damage_rect_);
  surface_damage_rect_ = gfx::Rect();
  // Constrain damage rectangle to output rectangle.
  damage_rect.Intersect(output_rect);

  const int kRenderPassId = 1;
  std::unique_ptr<viz::RenderPass> render_pass = viz::RenderPass::Create();
  render_pass->SetNew(kRenderPassId, output_rect, damage_rect,
                      buffer_to_target_transform);

  viz::SharedQuadState* quad_state =
      render_pass->CreateAndAppendSharedQuadState();
  quad_state->SetAll(
      buffer_to_target_transform,
      /*quad_layer_rect=*/output_rect,
      /*visible_quad_layer_rect=*/output_rect,
      /*clip_rect=*/gfx::Rect(),
      /*is_clipped=*/false, /*are_contents_opaque=*/false, /*opacity=*/1.f,
      /*blend_mode=*/SkBlendMode::kSrcOver, /*sorting_context_id=*/0);

  viz::CompositorFrame frame;
  // TODO(eseckler): FastInkView should use BeginFrames and set the ack
  // accordingly.
  frame.metadata.begin_frame_ack =
      viz::BeginFrameAck::CreateManualAckWithDamage();
  frame.metadata.device_scale_factor = device_scale_factor;
  viz::TextureDrawQuad* texture_quad =
      render_pass->CreateAndAppendDrawQuad<viz::TextureDrawQuad>();
  float vertex_opacity[4] = {1.0, 1.0, 1.0, 1.0};
  gfx::PointF uv_top_left(0.f, 0.f);
  gfx::PointF uv_bottom_right(1.f, 1.f);
  texture_quad->SetNew(quad_state, quad_rect, quad_rect, needs_blending,
                       transferable_resource.id, true, uv_top_left,
                       uv_bottom_right, SK_ColorTRANSPARENT, vertex_opacity,
                       false, false, false);
  texture_quad->set_resource_size_in_pixels(transferable_resource.size);
  frame.resource_list.push_back(transferable_resource);
  frame.render_pass_list.push_back(std::move(render_pass));

  frame_sink_holder_->SubmitCompositorFrame(
      std::move(frame), transferable_resource.id, std::move(resource));

  DCHECK(!pending_draw_surface_);
  pending_draw_surface_ = true;
}

void FastInkView::DidReceiveCompositorFrameAck() {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&FastInkView::OnDidDrawSurface,
                            weak_ptr_factory_.GetWeakPtr()));
}

void FastInkView::ReclaimResource(std::unique_ptr<Resource> resource) {
  returned_resources_.push_back(std::move(resource));
}

void FastInkView::OnDidDrawSurface() {
  pending_draw_surface_ = false;
  pending_draw_surface_rect_ = gfx::Rect();
  if (!buffer_damage_rect_.IsEmpty())
    UpdateBuffer();
  if (!surface_damage_rect_.IsEmpty())
    UpdateSurface();
}

}  // namespace ash
