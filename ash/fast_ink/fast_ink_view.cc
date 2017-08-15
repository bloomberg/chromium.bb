// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/fast_ink/fast_ink_view.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "base/threading/thread_task_runner_handle.h"
#include "cc/base/math_util.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/layer_tree_frame_sink.h"
#include "cc/output/layer_tree_frame_sink_client.h"
#include "cc/quads/texture_draw_quad.h"
#include "components/viz/common/gpu/context_provider.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/layout.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/views/widget/widget.h"

namespace ash {

class FastInkLayerTreeFrameSinkHolder : public cc::LayerTreeFrameSinkClient {
 public:
  FastInkLayerTreeFrameSinkHolder(
      FastInkView* view,
      std::unique_ptr<cc::LayerTreeFrameSink> frame_sink)
      : view_(view), frame_sink_(std::move(frame_sink)) {
    frame_sink_->BindToClient(this);
  }
  ~FastInkLayerTreeFrameSinkHolder() override {
    frame_sink_->DetachFromClient();
  }

  cc::LayerTreeFrameSink* frame_sink() { return frame_sink_.get(); }

  // Called before fast ink view is destroyed.
  void OnFastInkViewDestroying() { view_ = nullptr; }

  // Overridden from cc::LayerTreeFrameSinkClient:
  void SetBeginFrameSource(viz::BeginFrameSource* source) override {}
  void ReclaimResources(
      const std::vector<viz::ReturnedResource>& resources) override {
    if (view_)
      view_->ReclaimResources(resources);
  }
  void SetTreeActivationCallback(const base::Closure& callback) override {}
  void DidReceiveCompositorFrameAck() override {
    if (view_)
      view_->DidReceiveCompositorFrameAck();
  }
  void DidLoseLayerTreeFrameSink() override {}
  void OnDraw(const gfx::Transform& transform,
              const gfx::Rect& viewport,
              bool resourceless_software_draw) override {}
  void SetMemoryPolicy(const cc::ManagedMemoryPolicy& policy) override {}
  void SetExternalTilePriorityConstraints(
      const gfx::Rect& viewport_rect,
      const gfx::Transform& transform) override {}

 private:
  FastInkView* view_;
  std::unique_ptr<cc::LayerTreeFrameSink> frame_sink_;

  DISALLOW_COPY_AND_ASSIGN(FastInkLayerTreeFrameSinkHolder);
};

// This struct contains the resources associated with a fast ink frame.
struct FastInkResource {
  FastInkResource() {}
  ~FastInkResource() {
    if (context_provider) {
      gpu::gles2::GLES2Interface* gles2 = context_provider->ContextGL();
      if (texture)
        gles2->DeleteTextures(1, &texture);
      if (image)
        gles2->DestroyImageCHROMIUM(image);
    }
  }
  scoped_refptr<viz::ContextProvider> context_provider;
  uint32_t texture = 0;
  uint32_t image = 0;
  gpu::Mailbox mailbox;
};

// FastInkView
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

  frame_sink_holder_ = base::MakeUnique<FastInkLayerTreeFrameSinkHolder>(
      this, widget_->GetNativeView()->CreateLayerTreeFrameSink());
}

FastInkView::~FastInkView() {
  frame_sink_holder_->OnFastInkViewDestroying();
}

void FastInkView::DidReceiveCompositorFrameAck() {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&FastInkView::OnDidDrawSurface,
                            weak_ptr_factory_.GetWeakPtr()));
}

void FastInkView::ReclaimResources(
    const std::vector<viz::ReturnedResource>& resources) {
  for (auto& entry : resources) {
    auto it = resources_.find(entry.id);
    DCHECK(it != resources_.end());
    std::unique_ptr<FastInkResource> resource = std::move(it->second);
    resources_.erase(it);

    gpu::gles2::GLES2Interface* gles2 = resource->context_provider->ContextGL();
    if (entry.sync_token.HasData())
      gles2->WaitSyncTokenCHROMIUM(entry.sync_token.GetConstData());

    if (!entry.lost)
      returned_resources_.push_back(std::move(resource));
  }
}

void FastInkView::UpdateDamageRect(const gfx::Rect& rect) {
  buffer_damage_rect_.Union(rect);
}

void FastInkView::RequestRedraw() {
  if (pending_update_buffer_)
    return;

  pending_update_buffer_ = true;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&FastInkView::UpdateBuffer, weak_ptr_factory_.GetWeakPtr()));
}

void FastInkView::UpdateBuffer() {
  TRACE_EVENT1("ui", "FastInkView::UpdateBuffer", "damage",
               buffer_damage_rect_.ToString());

  DCHECK(pending_update_buffer_);
  pending_update_buffer_ = false;

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

  needs_update_surface_ = true;

  // Early out if waiting for last surface update to be drawn.
  if (pending_draw_surface_)
    return;

  UpdateSurface();
}

void FastInkView::UpdateSurface() {
  TRACE_EVENT1("ui", "FastInkView::UpdateSurface", "damage",
               surface_damage_rect_.ToString());

  DCHECK(needs_update_surface_);
  needs_update_surface_ = false;

  std::unique_ptr<FastInkResource> resource;
  // Reuse returned resource if available.
  if (!returned_resources_.empty()) {
    resource = std::move(returned_resources_.back());
    returned_resources_.pop_back();
  }

  // Create new resource if needed.
  if (!resource)
    resource = base::MakeUnique<FastInkResource>();

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

  gfx::Transform buffer_to_screen_transform;
  bool rv = screen_to_buffer_transform_.GetInverse(&buffer_to_screen_transform);
  DCHECK(rv);

  gfx::Rect output_rect(widget_->GetNativeView()->GetBoundsInScreen().size());
  // |quad_rect| is under normal cricumstances equal to |buffer_size| but to
  // be more resilient to rounding errors in the compositor that might cause
  // off-by-one problems when the transform is non-trivial we compute this rect
  // by mapping the output rect back into buffer space and intersecting by
  // buffer bounds. This avoids some corner cases where one row or column
  // would end up outside the screen and we would fail to take advantage of HW
  // overlays.
  gfx::Rect quad_rect = gfx::ToEnclosedRect(cc::MathUtil::MapClippedRect(
      screen_to_buffer_transform_, gfx::RectF(output_rect)));
  quad_rect.Intersect(gfx::Rect(buffer_size));

  const int kRenderPassId = 1;
  std::unique_ptr<cc::RenderPass> render_pass = cc::RenderPass::Create();
  render_pass->SetNew(kRenderPassId, output_rect, surface_damage_rect_,
                      buffer_to_screen_transform);
  surface_damage_rect_ = gfx::Rect();

  cc::SharedQuadState* quad_state =
      render_pass->CreateAndAppendSharedQuadState();
  quad_state->SetAll(buffer_to_screen_transform,
                     /*quad_layer_rect=*/output_rect,
                     /*visible_quad_layer_rect=*/output_rect,
                     /*clip_rect=*/gfx::Rect(),
                     /*is_clipped=*/false, /*opacity=*/1.f,
                     /*blend_mode=*/SkBlendMode::kSrcOver,
                     /*sorting_context_id=*/0);

  cc::CompositorFrame frame;
  // TODO(eseckler): FastInkView should use BeginFrames and set the ack
  // accordingly.
  frame.metadata.begin_frame_ack =
      viz::BeginFrameAck::CreateManualAckWithDamage();
  frame.metadata.device_scale_factor =
      widget_->GetLayer()->device_scale_factor();
  cc::TextureDrawQuad* texture_quad =
      render_pass->CreateAndAppendDrawQuad<cc::TextureDrawQuad>();
  float vertex_opacity[4] = {1.0, 1.0, 1.0, 1.0};
  gfx::PointF uv_top_left(0.f, 0.f);
  gfx::PointF uv_bottom_right(1.f, 1.f);
  texture_quad->SetNew(quad_state, quad_rect, gfx::Rect(), quad_rect,
                       transferable_resource.id, true, uv_top_left,
                       uv_bottom_right, SK_ColorTRANSPARENT, vertex_opacity,
                       false, false, false);
  texture_quad->set_resource_size_in_pixels(transferable_resource.size);
  frame.resource_list.push_back(transferable_resource);
  frame.render_pass_list.push_back(std::move(render_pass));

  frame_sink_holder_->frame_sink()->SubmitCompositorFrame(std::move(frame));

  resources_[transferable_resource.id] = std::move(resource);

  DCHECK(!pending_draw_surface_);
  pending_draw_surface_ = true;
}

void FastInkView::OnDidDrawSurface() {
  pending_draw_surface_ = false;
  if (needs_update_surface_)
    UpdateSurface();
}

}  // namespace ash
