// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/surface_layer_impl.h"

#include <stdint.h>

#include "base/trace_event/trace_event_argument.h"
#include "cc/debug/debug_colors.h"
#include "cc/layers/append_quads_data.h"
#include "cc/quads/solid_color_draw_quad.h"
#include "cc/quads/surface_draw_quad.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/occlusion.h"

namespace cc {

SurfaceLayerImpl::SurfaceLayerImpl(LayerTreeImpl* tree_impl, int id)
    : LayerImpl(tree_impl, id) {}

SurfaceLayerImpl::~SurfaceLayerImpl() {}

std::unique_ptr<LayerImpl> SurfaceLayerImpl::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return SurfaceLayerImpl::Create(tree_impl, id());
}

void SurfaceLayerImpl::SetPrimarySurfaceInfo(
    const viz::SurfaceInfo& surface_info) {
  if (primary_surface_info_ == surface_info)
    return;

  primary_surface_info_ = surface_info;
  NoteLayerPropertyChanged();
}

void SurfaceLayerImpl::SetFallbackSurfaceInfo(
    const viz::SurfaceInfo& surface_info) {
  if (fallback_surface_info_ == surface_info)
    return;

  fallback_surface_info_ = surface_info;
  NoteLayerPropertyChanged();
}

void SurfaceLayerImpl::SetStretchContentToFillBounds(bool stretch_content) {
  if (stretch_content_to_fill_bounds_ == stretch_content)
    return;

  stretch_content_to_fill_bounds_ = stretch_content;
  NoteLayerPropertyChanged();
}

void SurfaceLayerImpl::PushPropertiesTo(LayerImpl* layer) {
  LayerImpl::PushPropertiesTo(layer);
  SurfaceLayerImpl* layer_impl = static_cast<SurfaceLayerImpl*>(layer);
  layer_impl->SetPrimarySurfaceInfo(primary_surface_info_);
  layer_impl->SetFallbackSurfaceInfo(fallback_surface_info_);
  layer_impl->SetStretchContentToFillBounds(stretch_content_to_fill_bounds_);
}

void SurfaceLayerImpl::AppendQuads(RenderPass* render_pass,
                                   AppendQuadsData* append_quads_data) {
  AppendRainbowDebugBorder(render_pass);
  if (!primary_surface_info_.is_valid())
    return;

  SharedQuadState* common_shared_quad_state = nullptr;
  auto* primary =
      CreateSurfaceDrawQuad(render_pass, SurfaceDrawQuadType::PRIMARY,
                            primary_surface_info_, &common_shared_quad_state);
  // Emitting a fallback SurfaceDrawQuad is unnecessary if the primary and
  // fallback surface Ids match.
  if (primary && fallback_surface_info_.id() != primary_surface_info_.id()) {
    // Add the primary surface ID as a dependency.
    append_quads_data->activation_dependencies.push_back(
        primary_surface_info_.id());
    if (fallback_surface_info_.is_valid()) {
      // We can use the same SharedQuadState as the primary SurfaceDrawQuad if
      // we don't need a different transform on the fallback.
      bool use_common_shared_quad_state =
          !stretch_content_to_fill_bounds_ &&
          primary_surface_info_.device_scale_factor() ==
              fallback_surface_info_.device_scale_factor();
      primary->fallback_quad = CreateSurfaceDrawQuad(
          render_pass, SurfaceDrawQuadType::FALLBACK, fallback_surface_info_,
          use_common_shared_quad_state ? &common_shared_quad_state : nullptr);
    }
  }
}

SurfaceDrawQuad* SurfaceLayerImpl::CreateSurfaceDrawQuad(
    RenderPass* render_pass,
    SurfaceDrawQuadType surface_draw_quad_type,
    const viz::SurfaceInfo& surface_info,
    SharedQuadState** common_shared_quad_state) {
  DCHECK(surface_info.is_valid());

  gfx::Rect quad_rect(surface_info.size_in_pixels());
  gfx::Rect visible_quad_rect =
      draw_properties().occlusion_in_content_space.GetUnoccludedContentRect(
          gfx::Rect(bounds()));

  float layer_to_content_scale_x, layer_to_content_scale_y;
  if (stretch_content_to_fill_bounds_) {
    // Stretches the surface contents to exactly fill the layer bounds,
    // regardless of scale or aspect ratio differences.
    layer_to_content_scale_x =
        static_cast<float>(surface_info.size_in_pixels().width()) /
        bounds().width();
    layer_to_content_scale_y =
        static_cast<float>(surface_info.size_in_pixels().height()) /
        bounds().height();
  } else {
    layer_to_content_scale_x = layer_to_content_scale_y =
        surface_info.device_scale_factor();
  }

  visible_quad_rect = gfx::ScaleToEnclosingRect(
      visible_quad_rect, layer_to_content_scale_x, layer_to_content_scale_y);
  visible_quad_rect = gfx::IntersectRects(quad_rect, visible_quad_rect);

  if (visible_quad_rect.IsEmpty())
    return nullptr;

  // If a |common_shared_quad_state| is provided then use that. Otherwise,
  // allocate a new SharedQuadState. Assign the new SharedQuadState to
  // *|common_shared_quad_state| so that it may be reused by another emitted
  // SurfaceDrawQuad.
  SharedQuadState* shared_quad_state =
      common_shared_quad_state ? *common_shared_quad_state : nullptr;
  if (!shared_quad_state) {
    shared_quad_state = render_pass->CreateAndAppendSharedQuadState();
    PopulateScaledSharedQuadState(shared_quad_state, layer_to_content_scale_x,
                                  layer_to_content_scale_y);
  }
  if (common_shared_quad_state)
    *common_shared_quad_state = shared_quad_state;

  SurfaceDrawQuad* surface_draw_quad =
      render_pass->CreateAndAppendDrawQuad<SurfaceDrawQuad>();
  surface_draw_quad->SetNew(shared_quad_state, quad_rect, visible_quad_rect,
                            surface_info.id(), surface_draw_quad_type, nullptr);

  return surface_draw_quad;
}

void SurfaceLayerImpl::GetDebugBorderProperties(SkColor* color,
                                                float* width) const {
  *color = DebugColors::SurfaceLayerBorderColor();
  *width = DebugColors::SurfaceLayerBorderWidth(
      layer_tree_impl() ? layer_tree_impl()->device_scale_factor() : 1);
}

void SurfaceLayerImpl::AppendRainbowDebugBorder(RenderPass* render_pass) {
  if (!ShowDebugBorders(DebugBorderType::SURFACE))
    return;

  SharedQuadState* shared_quad_state =
      render_pass->CreateAndAppendSharedQuadState();
  PopulateSharedQuadState(shared_quad_state);

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
      SolidColorDrawQuad* top_quad =
          render_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
      top_quad->SetNew(shared_quad_state, top, top, colors[i % kNumColors],
                       force_anti_aliasing_off);

      SolidColorDrawQuad* bottom_quad =
          render_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
      bottom_quad->SetNew(shared_quad_state, bottom, bottom,
                          colors[kNumColors - 1 - (i % kNumColors)],
                          force_anti_aliasing_off);

      if (contents_opaque()) {
        // Draws a stripe filling the layer vertically with the same color and
        // width as the horizontal stipes along the layer's top border.
        SolidColorDrawQuad* solid_quad =
            render_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
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
      SolidColorDrawQuad* left_quad =
          render_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
      left_quad->SetNew(shared_quad_state, left, left,
                        colors[kNumColors - 1 - (i % kNumColors)],
                        force_anti_aliasing_off);

      SolidColorDrawQuad* right_quad =
          render_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
      right_quad->SetNew(shared_quad_state, right, right,
                         colors[i % kNumColors], force_anti_aliasing_off);
    }
  }
}

void SurfaceLayerImpl::AsValueInto(base::trace_event::TracedValue* dict) const {
  LayerImpl::AsValueInto(dict);
  dict->SetString("surface_id", primary_surface_info_.id().ToString());
  dict->SetString("fallback_surface_id",
                  fallback_surface_info_.id().ToString());
}

const char* SurfaceLayerImpl::LayerTypeAsString() const {
  return "cc::SurfaceLayerImpl";
}

}  // namespace cc
