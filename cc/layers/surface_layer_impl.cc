// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/surface_layer_impl.h"

#include <stdint.h>

#include "base/trace_event/trace_event_argument.h"
#include "cc/debug/debug_colors.h"
#include "cc/layers/append_quads_data.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/occlusion.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/surface_draw_quad.h"

namespace cc {

SurfaceLayerImpl::SurfaceLayerImpl(LayerTreeImpl* tree_impl, int id)
    : LayerImpl(tree_impl, id) {}

SurfaceLayerImpl::~SurfaceLayerImpl() = default;

std::unique_ptr<LayerImpl> SurfaceLayerImpl::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return SurfaceLayerImpl::Create(tree_impl, id());
}

void SurfaceLayerImpl::SetPrimarySurfaceId(
    const viz::SurfaceId& surface_id,
    base::Optional<uint32_t> deadline_in_frames) {
  if (primary_surface_id_ == surface_id &&
      deadline_in_frames_ == deadline_in_frames) {
    return;
  }
  primary_surface_id_ = surface_id;
  deadline_in_frames_ = deadline_in_frames;
  NoteLayerPropertyChanged();
}

void SurfaceLayerImpl::SetFallbackSurfaceId(const viz::SurfaceId& surface_id) {
  if (fallback_surface_id_ == surface_id)
    return;

  fallback_surface_id_ = surface_id;
  NoteLayerPropertyChanged();
}

void SurfaceLayerImpl::SetStretchContentToFillBounds(bool stretch_content) {
  if (stretch_content_to_fill_bounds_ == stretch_content)
    return;

  stretch_content_to_fill_bounds_ = stretch_content;
  NoteLayerPropertyChanged();
}

void SurfaceLayerImpl::SetHitTestable(bool hit_testable) {
  if (hit_testable_ == hit_testable)
    return;

  hit_testable_ = hit_testable;
  NoteLayerPropertyChanged();
}

void SurfaceLayerImpl::PushPropertiesTo(LayerImpl* layer) {
  LayerImpl::PushPropertiesTo(layer);
  SurfaceLayerImpl* layer_impl = static_cast<SurfaceLayerImpl*>(layer);
  layer_impl->SetPrimarySurfaceId(primary_surface_id_, deadline_in_frames_);
  // Unless the client explicitly specifies otherwise, don't block on
  // |primary_surface_id_| more than once.
  deadline_in_frames_ = 0u;
  layer_impl->SetFallbackSurfaceId(fallback_surface_id_);
  layer_impl->SetStretchContentToFillBounds(stretch_content_to_fill_bounds_);
  layer_impl->SetHitTestable(hit_testable_);
}

void SurfaceLayerImpl::AppendQuads(viz::RenderPass* render_pass,
                                   AppendQuadsData* append_quads_data) {
  AppendRainbowDebugBorder(render_pass);
  if (!primary_surface_id_.is_valid())
    return;

  auto* primary = CreateSurfaceDrawQuad(
      render_pass, primary_surface_id_,
      fallback_surface_id_.is_valid()
          ? base::Optional<viz::SurfaceId>(fallback_surface_id_)
          : base::nullopt);
  if (primary && fallback_surface_id_ != primary_surface_id_) {
    // Add the primary surface ID as a dependency.
    append_quads_data->activation_dependencies.push_back(primary_surface_id_);
    if (deadline_in_frames_) {
      if (!append_quads_data->deadline_in_frames)
        append_quads_data->deadline_in_frames = 0u;
      append_quads_data->deadline_in_frames = std::max(
          *append_quads_data->deadline_in_frames, *deadline_in_frames_);
    } else {
      append_quads_data->use_default_lower_bound_deadline = true;
    }
  }
  // Unless the client explicitly specifies otherwise, don't block on
  // |primary_surface_id_| more than once.
  deadline_in_frames_ = 0u;
}

bool SurfaceLayerImpl::is_surface_layer() const {
  return true;
}

viz::SurfaceDrawQuad* SurfaceLayerImpl::CreateSurfaceDrawQuad(
    viz::RenderPass* render_pass,
    const viz::SurfaceId& primary_surface_id,
    const base::Optional<viz::SurfaceId>& fallback_surface_id) {
  DCHECK(primary_surface_id.is_valid());

  float device_scale_factor = layer_tree_impl()->device_scale_factor();

  gfx::Rect quad_rect(gfx::ScaleToEnclosingRect(
      gfx::Rect(bounds()), device_scale_factor, device_scale_factor));
  gfx::Rect visible_quad_rect =
      draw_properties().occlusion_in_content_space.GetUnoccludedContentRect(
          gfx::Rect(bounds()));

  visible_quad_rect = gfx::ScaleToEnclosingRect(
      visible_quad_rect, device_scale_factor, device_scale_factor);
  visible_quad_rect = gfx::IntersectRects(quad_rect, visible_quad_rect);

  if (visible_quad_rect.IsEmpty())
    return nullptr;

  // If a |common_shared_quad_state| is provided then use that. Otherwise,
  // allocate a new SharedQuadState. Assign the new SharedQuadState to
  // *|common_shared_quad_state| so that it may be reused by another emitted
  // viz::SurfaceDrawQuad.
  viz::SharedQuadState* shared_quad_state =
    shared_quad_state = render_pass->CreateAndAppendSharedQuadState();

  PopulateScaledSharedQuadState(shared_quad_state, device_scale_factor,
                                device_scale_factor, contents_opaque());

  auto* surface_draw_quad =
      render_pass->CreateAndAppendDrawQuad<viz::SurfaceDrawQuad>();
  surface_draw_quad->SetNew(
      shared_quad_state, quad_rect, visible_quad_rect, primary_surface_id,
      fallback_surface_id, background_color(), stretch_content_to_fill_bounds_);

  return surface_draw_quad;
}

void SurfaceLayerImpl::GetDebugBorderProperties(SkColor* color,
                                                float* width) const {
  *color = DebugColors::SurfaceLayerBorderColor();
  *width = DebugColors::SurfaceLayerBorderWidth(
      layer_tree_impl() ? layer_tree_impl()->device_scale_factor() : 1);
}

void SurfaceLayerImpl::AppendRainbowDebugBorder(viz::RenderPass* render_pass) {
  if (!ShowDebugBorders(DebugBorderType::SURFACE))
    return;

  viz::SharedQuadState* shared_quad_state =
      render_pass->CreateAndAppendSharedQuadState();
  PopulateSharedQuadState(shared_quad_state, contents_opaque());

  SkColor color;
  float border_width;
  GetDebugBorderProperties(&color, &border_width);

  SkColor colors[] = {
      0x80ff0000,  // Red.
      0x80ffa500,  // Orange.
      0x80ffff00,  // Yellow.
      0x80008000,  // Green.
      0x800000ff,  // Blue.
      0x80ee82ee,  // Violet.
  };
  const int kNumColors = arraysize(colors);

  const int kStripeWidth = 300;
  const int kStripeHeight = 300;

  for (int i = 0;; ++i) {
    // For horizontal lines.
    int x = kStripeWidth * i;
    int width = std::min(kStripeWidth, bounds().width() - x - 1);

    // For vertical lines.
    int y = kStripeHeight * i;
    int height = std::min(kStripeHeight, bounds().height() - y - 1);

    gfx::Rect top(x, 0, width, border_width);
    gfx::Rect bottom(x, bounds().height() - border_width, width, border_width);
    gfx::Rect left(0, y, border_width, height);
    gfx::Rect right(bounds().width() - border_width, y, border_width, height);

    if (top.IsEmpty() && left.IsEmpty())
      break;

    if (!top.IsEmpty()) {
      bool force_anti_aliasing_off = false;
      auto* top_quad =
          render_pass->CreateAndAppendDrawQuad<viz::SolidColorDrawQuad>();
      top_quad->SetNew(shared_quad_state, top, top, colors[i % kNumColors],
                       force_anti_aliasing_off);

      auto* bottom_quad =
          render_pass->CreateAndAppendDrawQuad<viz::SolidColorDrawQuad>();
      bottom_quad->SetNew(shared_quad_state, bottom, bottom,
                          colors[kNumColors - 1 - (i % kNumColors)],
                          force_anti_aliasing_off);

      if (contents_opaque()) {
        // Draws a stripe filling the layer vertically with the same color and
        // width as the horizontal stipes along the layer's top border.
        auto* solid_quad =
            render_pass->CreateAndAppendDrawQuad<viz::SolidColorDrawQuad>();
        // The inner fill is more transparent then the border.
        static const float kFillOpacity = 0.1f;
        SkColor fill_color = SkColorSetA(
            colors[i % kNumColors],
            static_cast<uint8_t>(SkColorGetA(colors[i % kNumColors]) *
                                 kFillOpacity));
        gfx::Rect fill_rect(x, 0, width, bounds().height());
        solid_quad->SetNew(shared_quad_state, fill_rect, fill_rect, fill_color,
                           force_anti_aliasing_off);
      }
    }
    if (!left.IsEmpty()) {
      bool force_anti_aliasing_off = false;
      auto* left_quad =
          render_pass->CreateAndAppendDrawQuad<viz::SolidColorDrawQuad>();
      left_quad->SetNew(shared_quad_state, left, left,
                        colors[kNumColors - 1 - (i % kNumColors)],
                        force_anti_aliasing_off);

      auto* right_quad =
          render_pass->CreateAndAppendDrawQuad<viz::SolidColorDrawQuad>();
      right_quad->SetNew(shared_quad_state, right, right,
                         colors[i % kNumColors], force_anti_aliasing_off);
    }
  }
}

void SurfaceLayerImpl::AsValueInto(base::trace_event::TracedValue* dict) const {
  LayerImpl::AsValueInto(dict);
  dict->SetString("primar_surface_id", primary_surface_id_.ToString());
  dict->SetString("fallback_surface_id", fallback_surface_id_.ToString());
}

const char* SurfaceLayerImpl::LayerTypeAsString() const {
  return "cc::SurfaceLayerImpl";
}

}  // namespace cc
