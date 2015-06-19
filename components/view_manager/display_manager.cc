// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/view_manager/display_manager.h"

#include "base/numerics/safe_conversions.h"
#include "components/view_manager/display_manager_factory.h"
#include "components/view_manager/gles2/gpu_state.h"
#include "components/view_manager/native_viewport/onscreen_context_provider.h"
#include "components/view_manager/public/interfaces/gpu.mojom.h"
#include "components/view_manager/public/interfaces/quads.mojom.h"
#include "components/view_manager/public/interfaces/surfaces.mojom.h"
#include "components/view_manager/server_view.h"
#include "components/view_manager/view_coordinate_conversions.h"
#include "mojo/application/public/cpp/application_connection.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "mojo/converters/surfaces/surfaces_type_converters.h"
#include "mojo/converters/surfaces/surfaces_utils.h"
#include "mojo/converters/transform/transform_type_converters.h"

using mojo::Rect;
using mojo::Size;

namespace view_manager {
namespace {

void DrawViewTree(mojo::Pass* pass,
                  const ServerView* view,
                  const gfx::Vector2d& parent_to_root_origin_offset,
                  float opacity) {
  if (!view->visible())
    return;

  const gfx::Rect absolute_bounds =
      view->bounds() + parent_to_root_origin_offset;
  std::vector<const ServerView*> children(view->GetChildren());
  const float combined_opacity = opacity * view->opacity();
  for (std::vector<const ServerView*>::reverse_iterator it = children.rbegin();
       it != children.rend();
       ++it) {
    DrawViewTree(pass, *it, absolute_bounds.OffsetFromOrigin(),
                 combined_opacity);
  }

  cc::SurfaceId node_id = view->surface_id();

  auto surface_quad_state = mojo::SurfaceQuadState::New();
  surface_quad_state->surface = mojo::SurfaceId::From(node_id);

  gfx::Transform node_transform;
  node_transform.Translate(absolute_bounds.x(), absolute_bounds.y());

  const gfx::Rect bounds_at_origin(view->bounds().size());
  auto surface_quad = mojo::Quad::New();
  surface_quad->material = mojo::Material::MATERIAL_SURFACE_CONTENT;
  surface_quad->rect = Rect::From(bounds_at_origin);
  surface_quad->opaque_rect = Rect::From(bounds_at_origin);
  surface_quad->visible_rect = Rect::From(bounds_at_origin);
  surface_quad->needs_blending = true;
  surface_quad->shared_quad_state_index =
      base::saturated_cast<int32_t>(pass->shared_quad_states.size());
  surface_quad->surface_quad_state = surface_quad_state.Pass();

  auto sqs = mojo::CreateDefaultSQS(view->bounds().size());
  sqs->blend_mode = mojo::SK_XFERMODE_kSrcOver_Mode;
  sqs->opacity = combined_opacity;
  sqs->quad_to_target_transform = mojo::Transform::From(node_transform);

  pass->quads.push_back(surface_quad.Pass());
  pass->shared_quad_states.push_back(sqs.Pass());
}

}  // namespace

// static
DisplayManagerFactory* DisplayManager::factory_ = nullptr;

// static
DisplayManager* DisplayManager::Create(
    bool is_headless,
    mojo::ApplicationImpl* app_impl,
    const scoped_refptr<gles2::GpuState>& gpu_state) {
  if (factory_)
    return factory_->CreateDisplayManager(is_headless, app_impl, gpu_state);
  return new DefaultDisplayManager(is_headless, app_impl, gpu_state);
}

DefaultDisplayManager::DefaultDisplayManager(
    bool is_headless,
    mojo::ApplicationImpl* app_impl,
    const scoped_refptr<gles2::GpuState>& gpu_state)
    : is_headless_(is_headless),
      app_impl_(app_impl),
      gpu_state_(gpu_state),
      delegate_(nullptr),
      draw_timer_(false, false),
      frame_pending_(false),
      context_provider_(
          new native_viewport::OnscreenContextProvider(gpu_state)),
      weak_factory_(this) {
  metrics_.size_in_pixels = mojo::Size::New();
  metrics_.size_in_pixels->width = 800;
  metrics_.size_in_pixels->height = 600;
}

void DefaultDisplayManager::Init(DisplayManagerDelegate* delegate) {
  delegate_ = delegate;

  platform_viewport_ =
      native_viewport::PlatformViewport::Create(this, is_headless_).Pass();
  platform_viewport_->Init(gfx::Rect(metrics_.size_in_pixels.To<gfx::Size>()));
  platform_viewport_->Show();

  mojo::ContextProviderPtr context_provider;
  context_provider_->Bind(GetProxy(&context_provider).Pass());
  mojo::DisplayFactoryPtr display_factory;
  mojo::URLRequestPtr request(mojo::URLRequest::New());
  request->url = mojo::String::From("mojo:surfaces_service");
  app_impl_->ConnectToService(request.Pass(), &display_factory);
  display_factory->Create(context_provider.Pass(),
                          nullptr,  // returner - we never submit resources.
                          GetProxy(&display_));
}

DefaultDisplayManager::~DefaultDisplayManager() {
  // Destroy before |platform_viewport_| because this will destroy
  // CommandBufferDriver objects that contain child windows. Otherwise if this
  // class destroys its window first, X errors will occur.
  context_provider_.reset();

  // Destroy the NativeViewport early on as it may call us back during
  // destruction and we want to be in a known state.
  platform_viewport_.reset();
}

void DefaultDisplayManager::SchedulePaint(const ServerView* view,
                                          const gfx::Rect& bounds) {
  DCHECK(view);
  if (!view->IsDrawn())
    return;
  const gfx::Rect root_relative_rect =
      ConvertRectBetweenViews(view, delegate_->GetRootView(), bounds);
  if (root_relative_rect.IsEmpty())
    return;
  dirty_rect_.Union(root_relative_rect);
  WantToDraw();
}

void DefaultDisplayManager::SetViewportSize(const gfx::Size& size) {
  platform_viewport_->SetBounds(gfx::Rect(size));
}

const mojo::ViewportMetrics& DefaultDisplayManager::GetViewportMetrics() {
  return metrics_;
}

void DefaultDisplayManager::Draw() {
  gfx::Rect rect(metrics_.size_in_pixels.To<gfx::Size>());
  auto pass = mojo::CreateDefaultPass(1, rect);
  pass->damage_rect = Rect::From(dirty_rect_);

  DrawViewTree(pass.get(), delegate_->GetRootView(), gfx::Vector2d(), 1.0f);

  auto frame = mojo::Frame::New();
  frame->passes.push_back(pass.Pass());
  frame->resources.resize(0u);
  frame_pending_ = true;
  display_->SubmitFrame(
      frame.Pass(),
      base::Bind(&DefaultDisplayManager::DidDraw, base::Unretained(this)));
  dirty_rect_ = gfx::Rect();
}

void DefaultDisplayManager::DidDraw() {
  frame_pending_ = false;
  if (!dirty_rect_.IsEmpty())
    WantToDraw();
}

void DefaultDisplayManager::WantToDraw() {
  if (draw_timer_.IsRunning() || frame_pending_)
    return;

  draw_timer_.Start(
      FROM_HERE, base::TimeDelta(),
      base::Bind(&DefaultDisplayManager::Draw, base::Unretained(this)));
}

void DefaultDisplayManager::OnAcceleratedWidgetAvailable(
    gfx::AcceleratedWidget widget,
    float device_pixel_ratio) {
  context_provider_->SetAcceleratedWidget(widget);
  OnMetricsChanged(metrics_.size_in_pixels.To<gfx::Size>(), device_pixel_ratio);
}

void DefaultDisplayManager::OnAcceleratedWidgetDestroyed() {
  context_provider_->SetAcceleratedWidget(gfx::kNullAcceleratedWidget);
}

void DefaultDisplayManager::OnEvent(mojo::EventPtr event) {
  delegate_->OnEvent(event.Pass());
}

void DefaultDisplayManager::OnMetricsChanged(const gfx::Size& size,
                                             float device_scale_factor) {
  if ((metrics_.size_in_pixels.To<gfx::Size>() == size) &&
      (metrics_.device_pixel_ratio == device_scale_factor)) {
    return;
  }

  mojo::ViewportMetrics metrics;
  metrics.size_in_pixels = mojo::Size::From(size);
  metrics.device_pixel_ratio = device_scale_factor;

  delegate_->GetRootView()->SetBounds(gfx::Rect(size));
  delegate_->OnViewportMetricsChanged(metrics_, metrics);

  metrics_.size_in_pixels = metrics.size_in_pixels.Clone();
  metrics_.device_pixel_ratio = metrics.device_pixel_ratio;
}

void DefaultDisplayManager::OnDestroyed() {
  delegate_->OnDisplayClosed();
}

}  // namespace view_manager
