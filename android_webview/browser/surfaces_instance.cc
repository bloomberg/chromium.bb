// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/surfaces_instance.h"

#include <algorithm>
#include <utility>

#include "android_webview/browser/aw_gl_surface.h"
#include "android_webview/browser/aw_render_thread_context_provider.h"
#include "android_webview/browser/deferred_gpu_command_service.h"
#include "android_webview/browser/parent_output_surface.h"
#include "base/memory/ptr_util.h"
#include "cc/output/renderer_settings.h"
#include "cc/output/texture_mailbox_deleter.h"
#include "cc/quads/solid_color_draw_quad.h"
#include "cc/quads/surface_draw_quad.h"
#include "cc/scheduler/begin_frame_source.h"
#include "components/viz/common/surfaces/local_surface_id_allocator.h"
#include "components/viz/service/display/display.h"
#include "components/viz/service/display/display_scheduler.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/transform.h"

namespace android_webview {

namespace {
// The client_id used here should not conflict with the client_id generated
// from RenderWidgetHostImpl.
constexpr uint32_t kDefaultClientId = 0u;
SurfacesInstance* g_surfaces_instance = nullptr;
}  // namespace

// static
scoped_refptr<SurfacesInstance> SurfacesInstance::GetOrCreateInstance() {
  if (g_surfaces_instance)
    return make_scoped_refptr(g_surfaces_instance);
  return make_scoped_refptr(new SurfacesInstance);
}

SurfacesInstance::SurfacesInstance()
    : frame_sink_id_allocator_(kDefaultClientId),
      frame_sink_id_(AllocateFrameSinkId()) {
  cc::RendererSettings settings;

  // Should be kept in sync with compositor_impl_android.cc.
  settings.allow_antialiasing = false;
  settings.highp_threshold_min = 2048;

  // Webview does not own the surface so should not clear it.
  settings.should_clear_root_render_pass = false;

  frame_sink_manager_.reset(new viz::FrameSinkManagerImpl);
  local_surface_id_allocator_.reset(new viz::LocalSurfaceIdAllocator());

  constexpr bool is_root = true;
  constexpr bool handles_frame_sink_id_invalidation = true;
  constexpr bool needs_sync_points = true;
  support_ = viz::CompositorFrameSinkSupport::Create(
      this, frame_sink_manager_.get(), frame_sink_id_, is_root,
      handles_frame_sink_id_invalidation, needs_sync_points);

  begin_frame_source_.reset(new cc::StubBeginFrameSource);
  std::unique_ptr<cc::TextureMailboxDeleter> texture_mailbox_deleter(
      new cc::TextureMailboxDeleter(nullptr));
  std::unique_ptr<ParentOutputSurface> output_surface_holder(
      new ParentOutputSurface(AwRenderThreadContextProvider::Create(
          make_scoped_refptr(new AwGLSurface),
          DeferredGpuCommandService::GetInstance())));
  output_surface_ = output_surface_holder.get();
  auto scheduler = base::MakeUnique<viz::DisplayScheduler>(
      begin_frame_source_.get(), nullptr,
      output_surface_holder->capabilities().max_frames_pending);
  display_ = base::MakeUnique<viz::Display>(
      nullptr /* shared_bitmap_manager */,
      nullptr /* gpu_memory_buffer_manager */, settings, frame_sink_id_,
      std::move(output_surface_holder), std::move(scheduler),
      std::move(texture_mailbox_deleter));
  display_->Initialize(this, frame_sink_manager_->surface_manager());
  // TODO(ccameron): WebViews that are embedded in WCG windows will want to
  // specify gfx::ColorSpace::CreateExtendedSRGB(). This situation is not yet
  // detected.
  // https://crbug.com/735658
  gfx::ColorSpace display_color_space = gfx::ColorSpace::CreateSRGB();
  display_->SetColorSpace(display_color_space, display_color_space);
  frame_sink_manager_->RegisterBeginFrameSource(begin_frame_source_.get(),
                                                frame_sink_id_);

  display_->SetVisible(true);

  DCHECK(!g_surfaces_instance);
  g_surfaces_instance = this;
}

SurfacesInstance::~SurfacesInstance() {
  DCHECK_EQ(g_surfaces_instance, this);
  frame_sink_manager_->UnregisterBeginFrameSource(begin_frame_source_.get());
  g_surfaces_instance = nullptr;
  DCHECK(child_ids_.empty());
}

void SurfacesInstance::DisplayOutputSurfaceLost() {
  // Android WebView does not handle context loss.
  LOG(FATAL) << "Render thread context loss";
}

viz::FrameSinkId SurfacesInstance::AllocateFrameSinkId() {
  return frame_sink_id_allocator_.NextFrameSinkId();
}

viz::FrameSinkManagerImpl* SurfacesInstance::GetFrameSinkManager() {
  return frame_sink_manager_.get();
}

void SurfacesInstance::DrawAndSwap(const gfx::Size& viewport,
                                   const gfx::Rect& clip,
                                   const gfx::Transform& transform,
                                   const gfx::Size& frame_size,
                                   const viz::SurfaceId& child_id) {
  DCHECK(std::find(child_ids_.begin(), child_ids_.end(), child_id) !=
         child_ids_.end());

  // Create a frame with a single SurfaceDrawQuad referencing the child
  // Surface and transformed using the given transform.
  std::unique_ptr<cc::RenderPass> render_pass = cc::RenderPass::Create();
  render_pass->SetNew(1, gfx::Rect(viewport), clip, gfx::Transform());
  render_pass->has_transparent_background = false;

  cc::SharedQuadState* quad_state =
      render_pass->CreateAndAppendSharedQuadState();
  quad_state->quad_to_target_transform = transform;
  quad_state->quad_layer_rect = gfx::Rect(frame_size);
  quad_state->visible_quad_layer_rect = gfx::Rect(frame_size);
  quad_state->clip_rect = clip;
  quad_state->is_clipped = true;
  quad_state->opacity = 1.f;

  cc::SurfaceDrawQuad* surface_quad =
      render_pass->CreateAndAppendDrawQuad<cc::SurfaceDrawQuad>();
  surface_quad->SetNew(quad_state, gfx::Rect(quad_state->quad_layer_rect),
                       gfx::Rect(quad_state->quad_layer_rect), child_id,
                       cc::SurfaceDrawQuadType::PRIMARY, nullptr);

  cc::CompositorFrame frame;
  // We draw synchronously, so acknowledge a manual BeginFrame.
  frame.metadata.begin_frame_ack =
      cc::BeginFrameAck::CreateManualAckWithDamage();
  frame.render_pass_list.push_back(std::move(render_pass));
  frame.metadata.device_scale_factor = 1.f;
  frame.metadata.referenced_surfaces = child_ids_;

  if (!root_id_.is_valid() || viewport != surface_size_) {
    root_id_ = local_surface_id_allocator_->GenerateId();
    surface_size_ = viewport;
    display_->SetLocalSurfaceId(root_id_, 1.f);
  }
  bool result = support_->SubmitCompositorFrame(root_id_, std::move(frame));
  DCHECK(result);

  display_->Resize(viewport);
  display_->DrawAndSwap();
}

void SurfacesInstance::AddChildId(const viz::SurfaceId& child_id) {
  DCHECK(std::find(child_ids_.begin(), child_ids_.end(), child_id) ==
         child_ids_.end());
  child_ids_.push_back(child_id);
  if (root_id_.is_valid())
    SetSolidColorRootFrame();
}

void SurfacesInstance::RemoveChildId(const viz::SurfaceId& child_id) {
  auto itr = std::find(child_ids_.begin(), child_ids_.end(), child_id);
  DCHECK(itr != child_ids_.end());
  child_ids_.erase(itr);
  if (root_id_.is_valid())
    SetSolidColorRootFrame();
}

void SurfacesInstance::SetSolidColorRootFrame() {
  DCHECK(!surface_size_.IsEmpty());
  gfx::Rect rect(surface_size_);
  std::unique_ptr<cc::RenderPass> render_pass = cc::RenderPass::Create();
  render_pass->SetNew(1, rect, rect, gfx::Transform());
  cc::SharedQuadState* quad_state =
      render_pass->CreateAndAppendSharedQuadState();
  quad_state->SetAll(gfx::Transform(), rect, rect, rect, false, 1.f,
                     SkBlendMode::kSrcOver, 0);
  cc::SolidColorDrawQuad* solid_quad =
      render_pass->CreateAndAppendDrawQuad<cc::SolidColorDrawQuad>();
  solid_quad->SetNew(quad_state, rect, rect, SK_ColorBLACK, false);
  cc::CompositorFrame frame;
  frame.render_pass_list.push_back(std::move(render_pass));
  // We draw synchronously, so acknowledge a manual BeginFrame.
  frame.metadata.begin_frame_ack =
      cc::BeginFrameAck::CreateManualAckWithDamage();
  frame.metadata.referenced_surfaces = child_ids_;
  frame.metadata.device_scale_factor = 1;
  bool result = support_->SubmitCompositorFrame(root_id_, std::move(frame));
  DCHECK(result);
}

void SurfacesInstance::DidReceiveCompositorFrameAck(
    const std::vector<cc::ReturnedResource>& resources) {
  ReclaimResources(resources);
}

void SurfacesInstance::OnBeginFrame(const cc::BeginFrameArgs& args) {}

void SurfacesInstance::WillDrawSurface(
    const viz::LocalSurfaceId& local_surface_id,
    const gfx::Rect& damage_rect) {}

void SurfacesInstance::ReclaimResources(
    const std::vector<cc::ReturnedResource>& resources) {
  // Root surface should have no resources to return.
  CHECK(resources.empty());
}

}  // namespace android_webview
