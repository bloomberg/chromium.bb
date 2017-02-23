// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/render_surface_impl.h"

#include <stddef.h>

#include <algorithm>

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "cc/base/math_util.h"
#include "cc/debug/debug_colors.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/render_pass_sink.h"
#include "cc/output/filter_operations.h"
#include "cc/quads/debug_border_draw_quad.h"
#include "cc/quads/render_pass.h"
#include "cc/quads/render_pass_draw_quad.h"
#include "cc/quads/shared_quad_state.h"
#include "cc/trees/damage_tracker.h"
#include "cc/trees/draw_property_utils.h"
#include "cc/trees/effect_node.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/occlusion.h"
#include "cc/trees/transform_node.h"
#include "third_party/skia/include/core/SkImageFilter.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/transform.h"

namespace cc {

RenderSurfaceImpl::RenderSurfaceImpl(LayerTreeImpl* layer_tree_impl,
                                     int stable_effect_id)
    : layer_tree_impl_(layer_tree_impl),
      stable_effect_id_(stable_effect_id),
      effect_tree_index_(EffectTree::kInvalidNodeId),
      surface_property_changed_(false),
      ancestor_property_changed_(false),
      contributes_to_drawn_surface_(false),
      nearest_occlusion_immune_ancestor_(nullptr),
      target_render_surface_layer_index_history_(0),
      current_layer_index_history_(0) {
  damage_tracker_ = DamageTracker::Create();
}

RenderSurfaceImpl::~RenderSurfaceImpl() {}

RenderSurfaceImpl* RenderSurfaceImpl::render_target() {
  EffectTree& effect_tree = layer_tree_impl_->property_trees()->effect_tree;
  EffectNode* node = effect_tree.Node(EffectTreeIndex());
  if (node->target_id != EffectTree::kRootNodeId)
    return effect_tree.GetRenderSurface(node->target_id);
  else
    return this;
}

const RenderSurfaceImpl* RenderSurfaceImpl::render_target() const {
  const EffectTree& effect_tree =
      layer_tree_impl_->property_trees()->effect_tree;
  const EffectNode* node = effect_tree.Node(EffectTreeIndex());
  if (node->target_id != EffectTree::kRootNodeId)
    return effect_tree.GetRenderSurface(node->target_id);
  else
    return this;
}

RenderSurfaceImpl::DrawProperties::DrawProperties() {
  draw_opacity = 1.f;
  is_clipped = false;
}

RenderSurfaceImpl::DrawProperties::~DrawProperties() {}

gfx::RectF RenderSurfaceImpl::DrawableContentRect() const {
  if (content_rect().IsEmpty())
    return gfx::RectF();

  gfx::Rect surface_content_rect = content_rect();
  const FilterOperations& filters = Filters();
  if (!filters.IsEmpty()) {
    surface_content_rect =
        filters.MapRect(surface_content_rect, FiltersTransform().matrix());
  }
  gfx::RectF drawable_content_rect = MathUtil::MapClippedRect(
      draw_transform(), gfx::RectF(surface_content_rect));
  if (!filters.IsEmpty() && is_clipped()) {
    // Filter could move pixels around, but still need to be clipped.
    drawable_content_rect.Intersect(gfx::RectF(clip_rect()));
  }

  // If the rect has a NaN coordinate, we return empty rect to avoid crashes in
  // functions (for example, gfx::ToEnclosedRect) that are called on this rect.
  if (std::isnan(drawable_content_rect.x()) ||
      std::isnan(drawable_content_rect.y()) ||
      std::isnan(drawable_content_rect.right()) ||
      std::isnan(drawable_content_rect.bottom()))
    return gfx::RectF();

  return drawable_content_rect;
}

SkBlendMode RenderSurfaceImpl::BlendMode() const {
  return OwningEffectNode()->blend_mode;
}

bool RenderSurfaceImpl::UsesDefaultBlendMode() const {
  return BlendMode() == SkBlendMode::kSrcOver;
}

SkColor RenderSurfaceImpl::GetDebugBorderColor() const {
  return DebugColors::SurfaceBorderColor();
}

float RenderSurfaceImpl::GetDebugBorderWidth() const {
  return DebugColors::SurfaceBorderWidth(layer_tree_impl_);
}

LayerImpl* RenderSurfaceImpl::MaskLayer() {
  int mask_layer_id = OwningEffectNode()->mask_layer_id;
  return layer_tree_impl_->LayerById(mask_layer_id);
}

bool RenderSurfaceImpl::HasMask() const {
  return OwningEffectNode()->mask_layer_id != EffectTree::kInvalidNodeId;
}

const FilterOperations& RenderSurfaceImpl::Filters() const {
  return OwningEffectNode()->filters;
}

gfx::PointF RenderSurfaceImpl::FiltersOrigin() const {
  return OwningEffectNode()->filters_origin;
}

gfx::Transform RenderSurfaceImpl::FiltersTransform() const {
  gfx::Transform filters_transform;
  filters_transform.Scale(OwningEffectNode()->surface_contents_scale.x(),
                          OwningEffectNode()->surface_contents_scale.y());
  return filters_transform;
}

const FilterOperations& RenderSurfaceImpl::BackgroundFilters() const {
  return OwningEffectNode()->background_filters;
}

bool RenderSurfaceImpl::HasCopyRequest() const {
  return OwningEffectNode()->has_copy_request;
}

int RenderSurfaceImpl::TransformTreeIndex() const {
  return OwningEffectNode()->transform_id;
}

int RenderSurfaceImpl::ClipTreeIndex() const {
  return OwningEffectNode()->clip_id;
}

int RenderSurfaceImpl::EffectTreeIndex() const {
  DCHECK_EQ(effect_tree_index_,
            layer_tree_impl_->property_trees()
                ->layer_id_to_effect_node_index[stable_effect_id_]);
  return effect_tree_index_;
}

const EffectNode* RenderSurfaceImpl::OwningEffectNode() const {
  return layer_tree_impl_->property_trees()->effect_tree.Node(
      EffectTreeIndex());
}

void RenderSurfaceImpl::SetClipRect(const gfx::Rect& clip_rect) {
  if (clip_rect == draw_properties_.clip_rect)
    return;

  surface_property_changed_ = true;
  draw_properties_.clip_rect = clip_rect;
}

void RenderSurfaceImpl::SetContentRect(const gfx::Rect& content_rect) {
  if (content_rect == draw_properties_.content_rect)
    return;

  surface_property_changed_ = true;
  draw_properties_.content_rect = content_rect;
}

void RenderSurfaceImpl::SetContentRectForTesting(const gfx::Rect& rect) {
  SetContentRect(rect);
}

gfx::Rect RenderSurfaceImpl::CalculateExpandedClipForFilters(
    const gfx::Transform& target_to_surface) {
  gfx::Rect clip_in_surface_space =
      MathUtil::ProjectEnclosingClippedRect(target_to_surface, clip_rect());
  gfx::Rect expanded_clip_in_surface_space = Filters().MapRectReverse(
      clip_in_surface_space, FiltersTransform().matrix());
  gfx::Rect expanded_clip_in_target_space = MathUtil::MapEnclosingClippedRect(
      draw_transform(), expanded_clip_in_surface_space);
  return expanded_clip_in_target_space;
}

gfx::Rect RenderSurfaceImpl::CalculateClippedAccumulatedContentRect() {
  if (HasCopyRequest() || !is_clipped())
    return accumulated_content_rect();

  if (accumulated_content_rect().IsEmpty())
    return gfx::Rect();

  // Calculate projection from the target surface rect to local
  // space. Non-invertible draw transforms means no able to bring clipped rect
  // in target space back to local space, early out without clip.
  gfx::Transform target_to_surface(gfx::Transform::kSkipInitialization);
  if (!draw_transform().GetInverse(&target_to_surface))
    return accumulated_content_rect();

  // Clip rect is in target space. Bring accumulated content rect to
  // target space in preparation for clipping.
  gfx::Rect accumulated_rect_in_target_space =
      MathUtil::MapEnclosingClippedRect(draw_transform(),
                                        accumulated_content_rect());
  // If accumulated content rect is contained within clip rect, early out
  // without clipping.
  if (clip_rect().Contains(accumulated_rect_in_target_space))
    return accumulated_content_rect();

  gfx::Rect clipped_accumulated_rect_in_target_space;
  if (Filters().HasFilterThatMovesPixels()) {
    clipped_accumulated_rect_in_target_space =
        CalculateExpandedClipForFilters(target_to_surface);
  } else {
    clipped_accumulated_rect_in_target_space = clip_rect();
  }
  clipped_accumulated_rect_in_target_space.Intersect(
      accumulated_rect_in_target_space);

  if (clipped_accumulated_rect_in_target_space.IsEmpty())
    return gfx::Rect();

  gfx::Rect clipped_accumulated_rect_in_local_space =
      MathUtil::ProjectEnclosingClippedRect(
          target_to_surface, clipped_accumulated_rect_in_target_space);
  // Bringing clipped accumulated rect back to local space may result
  // in inflation due to axis-alignment.
  clipped_accumulated_rect_in_local_space.Intersect(accumulated_content_rect());
  return clipped_accumulated_rect_in_local_space;
}

void RenderSurfaceImpl::CalculateContentRectFromAccumulatedContentRect(
    int max_texture_size) {
  // Root render surface use viewport, and does not calculate content rect.
  DCHECK_NE(render_target(), this);

  // Surface's content rect is the clipped accumulated content rect. By default
  // use accumulated content rect, and then try to clip it.
  gfx::Rect surface_content_rect = CalculateClippedAccumulatedContentRect();

  // The RenderSurfaceImpl backing texture cannot exceed the maximum
  // supported texture size.
  surface_content_rect.set_width(
      std::min(surface_content_rect.width(), max_texture_size));
  surface_content_rect.set_height(
      std::min(surface_content_rect.height(), max_texture_size));

  SetContentRect(surface_content_rect);
}

void RenderSurfaceImpl::SetContentRectToViewport() {
  // Only root render surface use viewport as content rect.
  DCHECK_EQ(render_target(), this);
  gfx::Rect viewport = gfx::ToEnclosingRect(
      layer_tree_impl_->property_trees()->clip_tree.ViewportClip());
  SetContentRect(viewport);
}

void RenderSurfaceImpl::ClearAccumulatedContentRect() {
  accumulated_content_rect_ = gfx::Rect();
}

void RenderSurfaceImpl::AccumulateContentRectFromContributingLayer(
    LayerImpl* layer) {
  DCHECK(layer->DrawsContent());
  DCHECK_EQ(this, layer->render_target());

  // Root render surface doesn't accumulate content rect, it always uses
  // viewport for content rect.
  if (render_target() == this)
    return;

  accumulated_content_rect_.Union(layer->drawable_content_rect());
}

void RenderSurfaceImpl::AccumulateContentRectFromContributingRenderSurface(
    RenderSurfaceImpl* contributing_surface) {
  DCHECK_NE(this, contributing_surface);
  DCHECK_EQ(this, contributing_surface->render_target());

  // Root render surface doesn't accumulate content rect, it always uses
  // viewport for content rect.
  if (render_target() == this)
    return;

  // The content rect of contributing surface is in its own space. Instead, we
  // will use contributing surface's DrawableContentRect which is in target
  // space (local space for this render surface) as required.
  accumulated_content_rect_.Union(
      gfx::ToEnclosedRect(contributing_surface->DrawableContentRect()));
}

bool RenderSurfaceImpl::SurfacePropertyChanged() const {
  // Surface property changes are tracked as follows:
  //
  // - surface_property_changed_ is flagged when the clip_rect or content_rect
  //   change. As of now, these are the only two properties that can be affected
  //   by descendant layers.
  //
  // - all other property changes come from the surface's property tree nodes
  //   (or some ancestor node that propagates its change to one of these nodes).
  //
  return surface_property_changed_ || AncestorPropertyChanged();
}

bool RenderSurfaceImpl::SurfacePropertyChangedOnlyFromDescendant() const {
  return surface_property_changed_ && !AncestorPropertyChanged();
}

bool RenderSurfaceImpl::AncestorPropertyChanged() const {
  const PropertyTrees* property_trees = layer_tree_impl_->property_trees();
  return ancestor_property_changed_ || property_trees->full_tree_damaged ||
         property_trees->transform_tree.Node(TransformTreeIndex())
             ->transform_changed ||
         property_trees->effect_tree.Node(EffectTreeIndex())->effect_changed;
}

void RenderSurfaceImpl::NoteAncestorPropertyChanged() {
  ancestor_property_changed_ = true;
}

gfx::Rect RenderSurfaceImpl::GetDamageRect() {
  gfx::Rect damage_rect;
  bool is_valid_rect = damage_tracker_->GetDamageRectIfValid(&damage_rect);
  if (!is_valid_rect)
    return content_rect();
  return damage_rect;
}

void RenderSurfaceImpl::ResetPropertyChangedFlags() {
  surface_property_changed_ = false;
  ancestor_property_changed_ = false;
}

void RenderSurfaceImpl::ClearLayerLists() {
  layer_list_.clear();
}

int RenderSurfaceImpl::GetRenderPassId() {
  return id();
}

void RenderSurfaceImpl::AppendRenderPasses(RenderPassSink* pass_sink) {
  std::unique_ptr<RenderPass> pass = RenderPass::Create(layer_list_.size());
  gfx::Rect damage_rect = GetDamageRect();
  damage_rect.Intersect(content_rect());
  pass->SetNew(id(), content_rect(), damage_rect,
               draw_properties_.screen_space_transform);
  pass->filters = Filters();
  pass->background_filters = BackgroundFilters();
  pass_sink->AppendRenderPass(std::move(pass));
}

void RenderSurfaceImpl::AppendQuads(RenderPass* render_pass,
                                    AppendQuadsData* append_quads_data) {
  gfx::Rect visible_layer_rect =
      occlusion_in_content_space().GetUnoccludedContentRect(content_rect());
  if (visible_layer_rect.IsEmpty())
    return;

  const PropertyTrees* property_trees = layer_tree_impl_->property_trees();
  int sorting_context_id =
      property_trees->transform_tree.Node(TransformTreeIndex())
          ->sorting_context_id;
  SharedQuadState* shared_quad_state =
      render_pass->CreateAndAppendSharedQuadState();
  shared_quad_state->SetAll(
      draw_transform(), content_rect().size(), content_rect(),
      draw_properties_.clip_rect, draw_properties_.is_clipped,
      draw_properties_.draw_opacity, BlendMode(), sorting_context_id);

  if (layer_tree_impl_->debug_state().show_debug_borders) {
    DebugBorderDrawQuad* debug_border_quad =
        render_pass->CreateAndAppendDrawQuad<DebugBorderDrawQuad>();
    debug_border_quad->SetNew(shared_quad_state, content_rect(),
                              visible_layer_rect, GetDebugBorderColor(),
                              GetDebugBorderWidth());
  }

  ResourceId mask_resource_id = 0;
  gfx::Size mask_texture_size;
  gfx::Vector2dF mask_uv_scale;
  gfx::Vector2dF surface_contents_scale =
      OwningEffectNode()->surface_contents_scale;
  LayerImpl* mask_layer = MaskLayer();
  if (mask_layer && mask_layer->DrawsContent() &&
      !mask_layer->bounds().IsEmpty()) {
    // The software renderer applies mask layer and blending in the wrong
    // order but kDstIn doesn't commute with masking. It is okay to not
    // support this configuration because kDstIn was introduced to replace
    // mask layers.
    DCHECK(BlendMode() != SkBlendMode::kDstIn)
        << "kDstIn blend mode with mask layer is unsupported.";
    mask_layer->GetContentsResourceId(&mask_resource_id, &mask_texture_size);
    gfx::SizeF unclipped_mask_target_size = gfx::ScaleSize(
        gfx::SizeF(OwningEffectNode()->unscaled_mask_target_size),
        surface_contents_scale.x(), surface_contents_scale.y());
    mask_uv_scale = gfx::Vector2dF(1.0f / unclipped_mask_target_size.width(),
                                   1.0f / unclipped_mask_target_size.height());
  }

  RenderPassDrawQuad* quad =
      render_pass->CreateAndAppendDrawQuad<RenderPassDrawQuad>();
  quad->SetNew(
      shared_quad_state, content_rect(), visible_layer_rect, GetRenderPassId(),
      mask_resource_id, gfx::ScaleRect(gfx::RectF(content_rect()),
                                       mask_uv_scale.x(), mask_uv_scale.y()),
      mask_texture_size, surface_contents_scale, FiltersOrigin(), gfx::RectF());
}

}  // namespace cc
