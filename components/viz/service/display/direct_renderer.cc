// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/direct_renderer.h"

#include <stddef.h>

#include <utility>
#include <vector>

#include "base/auto_reset.h"
#include "base/containers/circular_deque.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/trace_event/trace_event.h"
#include "cc/base/filter_operations.h"
#include "cc/base/math_util.h"
#include "cc/resources/scoped_resource.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/quads/draw_quad.h"
#include "components/viz/service/display/bsp_tree.h"
#include "components/viz/service/display/bsp_walk_action.h"
#include "components/viz/service/display/output_surface.h"
#include "ui/gfx/geometry/quad_f.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/transform.h"

namespace {

static gfx::Transform OrthoProjectionMatrix(float left,
                                            float right,
                                            float bottom,
                                            float top) {
  // Use the standard formula to map the clipping frustum to the cube from
  // [-1, -1, -1] to [1, 1, 1].
  float delta_x = right - left;
  float delta_y = top - bottom;
  gfx::Transform proj;
  if (!delta_x || !delta_y)
    return proj;
  proj.matrix().set(0, 0, 2.0f / delta_x);
  proj.matrix().set(0, 3, -(right + left) / delta_x);
  proj.matrix().set(1, 1, 2.0f / delta_y);
  proj.matrix().set(1, 3, -(top + bottom) / delta_y);

  // Z component of vertices is always set to zero as we don't use the depth
  // buffer while drawing.
  proj.matrix().set(2, 2, 0);

  return proj;
}

static gfx::Transform window_matrix(int x, int y, int width, int height) {
  gfx::Transform canvas;

  // Map to window position and scale up to pixel coordinates.
  canvas.Translate3d(x, y, 0);
  canvas.Scale3d(width, height, 0);

  // Map from ([-1, -1] to [1, 1]) -> ([0, 0] to [1, 1])
  canvas.Translate3d(0.5, 0.5, 0.5);
  canvas.Scale3d(0.5, 0.5, 0.5);

  return canvas;
}

// Switching between enabling DC layers and not is expensive, so only
// switch away after a large number of frames not needing DC layers have
// been produced.
constexpr int kNumberOfFramesBeforeDisablingDCLayers = 60;

}  // namespace

namespace viz {

DirectRenderer::DrawingFrame::DrawingFrame() = default;
DirectRenderer::DrawingFrame::~DrawingFrame() = default;

DirectRenderer::DirectRenderer(const RendererSettings* settings,
                               OutputSurface* output_surface,
                               cc::DisplayResourceProvider* resource_provider)
    : settings_(settings),
      output_surface_(output_surface),
      resource_provider_(resource_provider),
      overlay_processor_(std::make_unique<OverlayProcessor>(output_surface)) {}

DirectRenderer::~DirectRenderer() = default;

void DirectRenderer::Initialize() {
  overlay_processor_->Initialize();

  auto* context_provider = output_surface_->context_provider();

  use_partial_swap_ = settings_->partial_swap_enabled && CanPartialSwap();
  allow_empty_swap_ = use_partial_swap_;
  if (context_provider) {
    if (context_provider->ContextCapabilities().commit_overlay_planes)
      allow_empty_swap_ = true;
    if (context_provider->ContextCapabilities().dc_layers)
      supports_dc_layers_ = true;
    if (context_provider->ContextCapabilities()
            .disable_non_empty_post_sub_buffers) {
      use_partial_swap_ = false;
    }
  }

  initialized_ = true;
}

// static
gfx::RectF DirectRenderer::QuadVertexRect() {
  return gfx::RectF(-0.5f, -0.5f, 1.f, 1.f);
}

// static
void DirectRenderer::QuadRectTransform(gfx::Transform* quad_rect_transform,
                                       const gfx::Transform& quad_transform,
                                       const gfx::RectF& quad_rect) {
  *quad_rect_transform = quad_transform;
  quad_rect_transform->Translate(0.5 * quad_rect.width() + quad_rect.x(),
                                 0.5 * quad_rect.height() + quad_rect.y());
  quad_rect_transform->Scale(quad_rect.width(), quad_rect.height());
}

void DirectRenderer::InitializeViewport(DrawingFrame* frame,
                                        const gfx::Rect& draw_rect,
                                        const gfx::Rect& viewport_rect,
                                        const gfx::Size& surface_size) {
  DCHECK_GE(viewport_rect.x(), 0);
  DCHECK_GE(viewport_rect.y(), 0);
  DCHECK_LE(viewport_rect.right(), surface_size.width());
  DCHECK_LE(viewport_rect.bottom(), surface_size.height());
  bool flip_y = FlippedFramebuffer();
  if (flip_y) {
    frame->projection_matrix = OrthoProjectionMatrix(
        draw_rect.x(), draw_rect.right(), draw_rect.bottom(), draw_rect.y());
  } else {
    frame->projection_matrix = OrthoProjectionMatrix(
        draw_rect.x(), draw_rect.right(), draw_rect.y(), draw_rect.bottom());
  }

  gfx::Rect window_rect = viewport_rect;
  if (flip_y)
    window_rect.set_y(surface_size.height() - viewport_rect.bottom());
  frame->window_matrix =
      window_matrix(window_rect.x(), window_rect.y(), window_rect.width(),
                    window_rect.height());
  current_draw_rect_ = draw_rect;
  current_viewport_rect_ = viewport_rect;
  current_surface_size_ = surface_size;
  current_window_space_viewport_ = window_rect;
}

gfx::Rect DirectRenderer::MoveFromDrawToWindowSpace(
    const gfx::Rect& draw_rect) const {
  gfx::Rect window_rect = draw_rect;
  window_rect -= current_draw_rect_.OffsetFromOrigin();
  window_rect += current_viewport_rect_.OffsetFromOrigin();
  if (FlippedFramebuffer())
    window_rect.set_y(current_surface_size_.height() - window_rect.bottom());
  return window_rect;
}

const TileDrawQuad* DirectRenderer::CanPassBeDrawnDirectly(
    const RenderPass* pass) {
  return nullptr;
}

void DirectRenderer::SetVisible(bool visible) {
  DCHECK(initialized_);
  if (visible_ == visible)
    return;
  visible_ = visible;
  DidChangeVisibility();
}

void DirectRenderer::DecideRenderPassAllocationsForFrame(
    const RenderPassList& render_passes_in_draw_order) {
  render_pass_bypass_quads_.clear();

  auto& root_render_pass = render_passes_in_draw_order.back();

  struct RenderPassRequirements {
    gfx::Size size;
    cc::ResourceProvider::TextureHint hint;
  };
  base::flat_map<RenderPassId, RenderPassRequirements> render_passes_in_frame;
  for (const auto& pass : render_passes_in_draw_order) {
    if (pass != root_render_pass) {
      if (const TileDrawQuad* tile_quad = CanPassBeDrawnDirectly(pass.get())) {
        // If the render pass is drawn directly, it will not be drawn from as
        // a render pass so it's not added to the map.
        render_pass_bypass_quads_[pass->id] = *tile_quad;
        continue;
      }
    }
    render_passes_in_frame[pass->id] = {RenderPassTextureSize(pass.get()),
                                        RenderPassTextureHint(pass.get())};
  }

  std::vector<RenderPassId> passes_to_delete;
  for (const auto& pair : render_pass_textures_) {
    auto it = render_passes_in_frame.find(pair.first);
    if (it == render_passes_in_frame.end()) {
      passes_to_delete.push_back(pair.first);
      continue;
    }

    gfx::Size required_size = it->second.size;
    cc::ResourceProvider::TextureHint required_hint = it->second.hint;
    cc::ScopedResource* texture = pair.second.get();
    DCHECK(texture);

    bool size_appropriate = texture->size().width() >= required_size.width() &&
                            texture->size().height() >= required_size.height();
    bool hint_appropriate = (texture->hint() & required_hint) == required_hint;
    if (texture->id() && (!size_appropriate || !hint_appropriate))
      texture->Free();
  }

  // Delete RenderPass textures from the previous frame that will not be used
  // again.
  for (size_t i = 0; i < passes_to_delete.size(); ++i)
    render_pass_textures_.erase(passes_to_delete[i]);

  for (auto& pass : render_passes_in_draw_order) {
    auto& resource = render_pass_textures_[pass->id];
    if (!resource) {
      resource = std::make_unique<cc::ScopedResource>(resource_provider_);

      // |has_damage_from_contributing_content| is used to determine if previous
      // contents can be reused when caching render pass and as a result needs
      // to be true when a new resource is created to ensure that it is updated
      // and not assumed to already contain correct contents.
      pass->has_damage_from_contributing_content = true;
    }
  }
}

void DirectRenderer::DrawFrame(RenderPassList* render_passes_in_draw_order,
                               float device_scale_factor,
                               const gfx::Size& device_viewport_size) {
  DCHECK(visible_);
  TRACE_EVENT0("cc", "DirectRenderer::DrawFrame");
  UMA_HISTOGRAM_COUNTS(
      "Renderer4.renderPassCount",
      base::saturated_cast<int>(render_passes_in_draw_order->size()));

  RenderPass* root_render_pass = render_passes_in_draw_order->back().get();
  DCHECK(root_render_pass);

  bool overdraw_tracing_enabled;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(
      TRACE_DISABLED_BY_DEFAULT("cc.debug.overdraw"),
      &overdraw_tracing_enabled);
  bool overdraw_feedback =
      settings_->show_overdraw_feedback || overdraw_tracing_enabled;
  if (overdraw_feedback && !output_surface_->capabilities().supports_stencil) {
#if DCHECK_IS_ON()
    DLOG_IF(WARNING, !overdraw_feedback_support_missing_logged_once_)
        << "Overdraw feedback enabled on platform without support.";
    overdraw_feedback_support_missing_logged_once_ = true;
#endif
    overdraw_feedback = false;
  }
  base::AutoReset<bool> auto_reset_overdraw_feedback(&overdraw_feedback_,
                                                     overdraw_feedback);

  current_frame_valid_ = true;
  current_frame_ = DrawingFrame();
  current_frame()->render_passes_in_draw_order = render_passes_in_draw_order;
  current_frame()->root_render_pass = root_render_pass;
  current_frame()->root_damage_rect = root_render_pass->damage_rect;
  current_frame()->root_damage_rect.Union(
      overlay_processor_->GetAndResetOverlayDamage());
  current_frame()->root_damage_rect.Intersect(gfx::Rect(device_viewport_size));
  current_frame()->device_viewport_size = device_viewport_size;

  // Only reshape when we know we are going to draw. Otherwise, the reshape
  // can leave the window at the wrong size if we never draw and the proper
  // viewport size is never set.
  bool frame_has_alpha =
      current_frame()->root_render_pass->has_transparent_background;
  bool use_stencil = overdraw_feedback_;
  bool did_reshape = false;
  if (device_viewport_size != reshape_surface_size_ ||
      device_scale_factor != reshape_device_scale_factor_ ||
      root_render_pass->color_space != reshape_device_color_space_ ||
      frame_has_alpha != reshape_has_alpha_ ||
      use_stencil != reshape_use_stencil_) {
    reshape_surface_size_ = device_viewport_size;
    reshape_device_scale_factor_ = device_scale_factor;
    reshape_device_color_space_ = root_render_pass->color_space;
    reshape_has_alpha_ =
        current_frame()->root_render_pass->has_transparent_background;
    reshape_use_stencil_ = overdraw_feedback_;
    output_surface_->Reshape(
        reshape_surface_size_, reshape_device_scale_factor_,
        reshape_device_color_space_, reshape_has_alpha_, reshape_use_stencil_);
    did_reshape = true;
  }

  BeginDrawingFrame();

  for (const auto& pass : *render_passes_in_draw_order) {
    if (!pass->filters.IsEmpty())
      render_pass_filters_[pass->id] = &pass->filters;
    if (!pass->background_filters.IsEmpty())
      render_pass_background_filters_[pass->id] = &pass->background_filters;
  }

  // Create the overlay candidate for the output surface, and mark it as
  // always handled.
  if (output_surface_->IsDisplayedAsOverlayPlane()) {
    cc::OverlayCandidate output_surface_plane;
    output_surface_plane.display_rect =
        gfx::RectF(root_render_pass->output_rect);
    output_surface_plane.format = output_surface_->GetOverlayBufferFormat();
    output_surface_plane.use_output_surface_for_resource = true;
    output_surface_plane.overlay_handled = true;
    current_frame()->overlay_list.push_back(output_surface_plane);
  }

  // Attempt to replace some or all of the quads of the root render pass with
  // overlays.
  overlay_processor_->ProcessForOverlays(
      resource_provider_, render_passes_in_draw_order, render_pass_filters_,
      render_pass_background_filters_, &current_frame()->overlay_list,
      &current_frame()->ca_layer_overlay_list,
      &current_frame()->dc_layer_overlay_list,
      &current_frame()->root_damage_rect,
      &current_frame()->root_content_bounds);

  // Draw all non-root render passes except for the root render pass.
  for (const auto& pass : *render_passes_in_draw_order) {
    if (pass.get() == root_render_pass)
      break;
    DrawRenderPassAndExecuteCopyRequests(pass.get());
  }

  bool was_using_dc_layers = using_dc_layers_;
  if (!current_frame()->dc_layer_overlay_list.empty()) {
    DCHECK(supports_dc_layers_);
    using_dc_layers_ = true;
    frames_since_using_dc_layers_ = 0;
  } else if (++frames_since_using_dc_layers_ >=
             kNumberOfFramesBeforeDisablingDCLayers) {
    using_dc_layers_ = false;
  }
  if (supports_dc_layers_ &&
      (did_reshape || (was_using_dc_layers != using_dc_layers_))) {
    // The entire surface has to be redrawn if it was reshaped or if switching
    // from or to DirectComposition layers, because the previous contents are
    // discarded and some contents would otherwise be undefined.
    current_frame()->root_damage_rect =
        current_frame()->root_render_pass->output_rect;
  }

  // We can skip all drawing if the damage rect is now empty.
  bool skip_drawing_root_render_pass =
      current_frame()->root_damage_rect.IsEmpty() && allow_empty_swap_;

  // If we have to draw but don't support partial swap, the whole output should
  // be considered damaged.
  if (!skip_drawing_root_render_pass && !use_partial_swap_)
    current_frame()->root_damage_rect = root_render_pass->output_rect;

  if (!skip_drawing_root_render_pass)
    DrawRenderPassAndExecuteCopyRequests(root_render_pass);

  FinishDrawingFrame();
  render_passes_in_draw_order->clear();
  render_pass_filters_.clear();
  render_pass_background_filters_.clear();

  current_frame_valid_ = false;
}

gfx::Rect DirectRenderer::DrawingFrame::ComputeScissorRectForRenderPass()
    const {
  if (current_render_pass == root_render_pass)
    return root_damage_rect;

  // If the root damage rect has been expanded due to overlays, all the other
  // damage rect calculations are incorrect.
  if (!root_render_pass->damage_rect.Contains(root_damage_rect))
    return current_render_pass->output_rect;

  DCHECK(
      current_render_pass->copy_requests.empty() ||
      (current_render_pass->damage_rect == current_render_pass->output_rect));
  return current_render_pass->damage_rect;
}

gfx::Rect DirectRenderer::DeviceViewportRectInDrawSpace() const {
  gfx::Rect device_viewport_rect(current_frame()->device_viewport_size);
  device_viewport_rect -= current_viewport_rect_.OffsetFromOrigin();
  device_viewport_rect += current_draw_rect_.OffsetFromOrigin();
  return device_viewport_rect;
}

gfx::Rect DirectRenderer::OutputSurfaceRectInDrawSpace() const {
  if (current_frame()->current_render_pass ==
      current_frame()->root_render_pass) {
    gfx::Rect output_surface_rect(current_frame()->device_viewport_size);
    output_surface_rect -= current_viewport_rect_.OffsetFromOrigin();
    output_surface_rect += current_draw_rect_.OffsetFromOrigin();
    return output_surface_rect;
  } else {
    return current_frame()->current_render_pass->output_rect;
  }
}

bool DirectRenderer::ShouldSkipQuad(const DrawQuad& quad,
                                    const gfx::Rect& render_pass_scissor) {
  if (render_pass_scissor.IsEmpty())
    return true;

  gfx::Rect target_rect = cc::MathUtil::MapEnclosingClippedRect(
      quad.shared_quad_state->quad_to_target_transform, quad.visible_rect);
  if (quad.shared_quad_state->is_clipped)
    target_rect.Intersect(quad.shared_quad_state->clip_rect);

  target_rect.Intersect(render_pass_scissor);
  return target_rect.IsEmpty();
}

void DirectRenderer::SetScissorStateForQuad(
    const DrawQuad& quad,
    const gfx::Rect& render_pass_scissor,
    bool use_render_pass_scissor) {
  if (use_render_pass_scissor) {
    gfx::Rect quad_scissor_rect = render_pass_scissor;
    if (quad.shared_quad_state->is_clipped)
      quad_scissor_rect.Intersect(quad.shared_quad_state->clip_rect);
    SetScissorTestRectInDrawSpace(quad_scissor_rect);
    return;
  } else if (quad.shared_quad_state->is_clipped) {
    SetScissorTestRectInDrawSpace(quad.shared_quad_state->clip_rect);
    return;
  }

  EnsureScissorTestDisabled();
}

void DirectRenderer::SetScissorTestRectInDrawSpace(
    const gfx::Rect& draw_space_rect) {
  gfx::Rect window_space_rect = MoveFromDrawToWindowSpace(draw_space_rect);
  SetScissorTestRect(window_space_rect);
}

void DirectRenderer::DoDrawPolygon(const DrawPolygon& poly,
                                   const gfx::Rect& render_pass_scissor,
                                   bool use_render_pass_scissor) {
  SetScissorStateForQuad(*poly.original_ref(), render_pass_scissor,
                         use_render_pass_scissor);

  // If the poly has not been split, then it is just a normal DrawQuad,
  // and we should save any extra processing that would have to be done.
  if (!poly.is_split()) {
    DoDrawQuad(poly.original_ref(), NULL);
    return;
  }

  std::vector<gfx::QuadF> quads;
  poly.ToQuads2D(&quads);
  for (size_t i = 0; i < quads.size(); ++i) {
    DoDrawQuad(poly.original_ref(), &quads[i]);
  }
}

const cc::FilterOperations* DirectRenderer::FiltersForPass(
    RenderPassId render_pass_id) const {
  auto it = render_pass_filters_.find(render_pass_id);
  return it == render_pass_filters_.end() ? nullptr : it->second;
}

const cc::FilterOperations* DirectRenderer::BackgroundFiltersForPass(
    RenderPassId render_pass_id) const {
  auto it = render_pass_background_filters_.find(render_pass_id);
  return it == render_pass_background_filters_.end() ? nullptr : it->second;
}

void DirectRenderer::FlushPolygons(
    base::circular_deque<std::unique_ptr<DrawPolygon>>* poly_list,
    const gfx::Rect& render_pass_scissor,
    bool use_render_pass_scissor) {
  if (poly_list->empty()) {
    return;
  }

  BspTree bsp_tree(poly_list);
  BspWalkActionDrawPolygon action_handler(this, render_pass_scissor,
                                          use_render_pass_scissor);
  bsp_tree.TraverseWithActionHandler(&action_handler);
  DCHECK(poly_list->empty());
}

void DirectRenderer::DrawRenderPassAndExecuteCopyRequests(
    RenderPass* render_pass) {
  if (render_pass_bypass_quads_.find(render_pass->id) !=
      render_pass_bypass_quads_.end()) {
    return;
  }

  // Repeated draw to simulate a slower device for the evaluation of performance
  // improvements in UI effects.
  for (int i = 0; i < settings_->slow_down_compositing_scale_factor; ++i)
    DrawRenderPass(render_pass);

  bool first_request = true;
  for (auto& copy_request : render_pass->copy_requests) {
    // CopyDrawnRenderPass() can change the binding of the framebuffer target as
    // a part of its usual scaling and readback operations. Therefore, make sure
    // to restore the correct framebuffer between readbacks. (Even if it did
    // not, a Mac-specific bug requires this workaround: http://crbug.com/99393)
    if (!first_request)
      UseRenderPass(render_pass);
    CopyDrawnRenderPass(std::move(copy_request));
    first_request = false;
  }
}

void DirectRenderer::DrawRenderPass(const RenderPass* render_pass) {
  TRACE_EVENT0("cc", "DirectRenderer::DrawRenderPass");
  if (!UseRenderPass(render_pass))
    return;

  const gfx::Rect surface_rect_in_draw_space = OutputSurfaceRectInDrawSpace();
  gfx::Rect render_pass_scissor_in_draw_space = surface_rect_in_draw_space;

  if (current_frame()->current_render_pass ==
      current_frame()->root_render_pass) {
    render_pass_scissor_in_draw_space.Intersect(
        DeviceViewportRectInDrawSpace());
  }

  if (use_partial_swap_) {
    render_pass_scissor_in_draw_space.Intersect(
        current_frame()->ComputeScissorRectForRenderPass());
  }

  bool is_root_render_pass =
      current_frame()->current_render_pass == current_frame()->root_render_pass;

  bool render_pass_is_clipped =
      !render_pass_scissor_in_draw_space.Contains(surface_rect_in_draw_space);

  // The SetDrawRectangleCHROMIUM spec requires that the scissor bit is always
  // set on the root framebuffer or else the rendering may modify something
  // outside the damage rectangle, even if the damage rectangle is the size of
  // the full backbuffer.
  bool render_pass_requires_scissor =
      (supports_dc_layers_ && is_root_render_pass) || render_pass_is_clipped;
  bool has_external_stencil_test =
      is_root_render_pass && output_surface_->HasExternalStencilTest();
  bool should_clear_surface =
      !has_external_stencil_test &&
      (!is_root_render_pass || settings_->should_clear_root_render_pass);

  // If |has_external_stencil_test| we can't discard or clear. Make sure we
  // don't need to.
  DCHECK(!has_external_stencil_test ||
         !current_frame()->current_render_pass->has_transparent_background);

  SurfaceInitializationMode mode;
  if (should_clear_surface && render_pass_requires_scissor) {
    mode = SURFACE_INITIALIZATION_MODE_SCISSORED_CLEAR;
  } else if (should_clear_surface) {
    mode = SURFACE_INITIALIZATION_MODE_FULL_SURFACE_CLEAR;
  } else {
    mode = SURFACE_INITIALIZATION_MODE_PRESERVE;
  }

  PrepareSurfaceForPass(
      mode, MoveFromDrawToWindowSpace(render_pass_scissor_in_draw_space));

  const QuadList& quad_list = render_pass->quad_list;
  base::circular_deque<std::unique_ptr<DrawPolygon>> poly_list;

  int next_polygon_id = 0;
  int last_sorting_context_id = 0;
  for (auto it = quad_list.BackToFrontBegin(); it != quad_list.BackToFrontEnd();
       ++it) {
    const DrawQuad& quad = **it;

    if (render_pass_is_clipped &&
        ShouldSkipQuad(quad, render_pass_scissor_in_draw_space)) {
      continue;
    }

    if (last_sorting_context_id != quad.shared_quad_state->sorting_context_id) {
      last_sorting_context_id = quad.shared_quad_state->sorting_context_id;
      FlushPolygons(&poly_list, render_pass_scissor_in_draw_space,
                    render_pass_requires_scissor);
    }

    // This layer is in a 3D sorting context so we add it to the list of
    // polygons to go into the BSP tree.
    if (quad.shared_quad_state->sorting_context_id != 0) {
      // TODO(danakj): It's sad to do a malloc here to compare. Maybe construct
      // this on the stack and move it into the list.
      auto new_polygon = std::make_unique<DrawPolygon>(
          *it, gfx::RectF(quad.visible_rect),
          quad.shared_quad_state->quad_to_target_transform, next_polygon_id++);
      if (new_polygon->points().size() > 2u) {
        poly_list.push_back(std::move(new_polygon));
      }
      continue;
    }

    // We are not in a 3d sorting context, so we should draw the quad normally.
    SetScissorStateForQuad(quad, render_pass_scissor_in_draw_space,
                           render_pass_requires_scissor);

    DoDrawQuad(&quad, nullptr);
  }
  FlushPolygons(&poly_list, render_pass_scissor_in_draw_space,
                render_pass_requires_scissor);
  FinishDrawingQuadList();

  if (render_pass->generate_mipmap)
    GenerateMipmap();
}

bool DirectRenderer::UseRenderPass(const RenderPass* render_pass) {
  current_frame()->current_render_pass = render_pass;
  current_frame()->current_texture = nullptr;
  if (render_pass == current_frame()->root_render_pass) {
    BindFramebufferToOutputSurface();

    if (supports_dc_layers_) {
      SetEnableDCLayers(using_dc_layers_);
      output_surface_->SetDrawRectangle(current_frame()->root_damage_rect);
    }
    InitializeViewport(current_frame(), render_pass->output_rect,
                       gfx::Rect(current_frame()->device_viewport_size),
                       current_frame()->device_viewport_size);
    return true;
  }

  cc::ScopedResource* texture = render_pass_textures_[render_pass->id].get();
  DCHECK(texture);

  gfx::Size size = RenderPassTextureSize(render_pass);
  size.Enlarge(enlarge_pass_texture_amount_.width(),
               enlarge_pass_texture_amount_.height());
  if (!texture->id()) {
    texture->Allocate(size, RenderPassTextureHint(render_pass),
                      BackbufferFormat(),
                      current_frame()->current_render_pass->color_space);
  } else if (render_pass->cache_render_pass &&
             !render_pass->has_damage_from_contributing_content) {
    return false;
  } else if (current_frame()->ComputeScissorRectForRenderPass().IsEmpty()) {
    return false;
  }
  DCHECK(texture->id());

  if (BindFramebufferToTexture(texture)) {
    InitializeViewport(current_frame(), render_pass->output_rect,
                       gfx::Rect(render_pass->output_rect.size()),
                       texture->size());
    return true;
  }

  return false;
}

bool DirectRenderer::HasAllocatedResourcesForTesting(
    RenderPassId render_pass_id) const {
  auto iter = render_pass_textures_.find(render_pass_id);
  return iter != render_pass_textures_.end() && iter->second->id();
}

// static
gfx::Size DirectRenderer::RenderPassTextureSize(const RenderPass* render_pass) {
  return render_pass->output_rect.size();
}

// static
cc::ResourceProvider::TextureHint DirectRenderer::RenderPassTextureHint(
    const RenderPass* render_pass) {
  return render_pass->generate_mipmap
             ? cc::ResourceProvider::TEXTURE_HINT_MIPMAP_FRAMEBUFFER
             : cc::ResourceProvider::TEXTURE_HINT_FRAMEBUFFER;
}

void DirectRenderer::SetCurrentFrameForTesting(const DrawingFrame& frame) {
  current_frame_valid_ = true;
  current_frame_ = frame;
}

}  // namespace viz
