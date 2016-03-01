// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_host_common.h"

#include <stddef.h>

#include <algorithm>

#include "base/trace_event/trace_event.h"
#include "cc/base/math_util.h"
#include "cc/layers/heads_up_display_layer_impl.h"
#include "cc/layers/layer.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/layer_iterator.h"
#include "cc/layers/render_surface_draw_properties.h"
#include "cc/layers/render_surface_impl.h"
#include "cc/proto/begin_main_frame_and_commit_state.pb.h"
#include "cc/proto/gfx_conversions.h"
#include "cc/trees/draw_property_utils.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_impl.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/vector2d_conversions.h"
#include "ui/gfx/transform.h"
#include "ui/gfx/transform_util.h"

namespace cc {

LayerTreeHostCommon::CalcDrawPropsMainInputs::CalcDrawPropsMainInputs(
    Layer* root_layer,
    const gfx::Size& device_viewport_size,
    const gfx::Transform& device_transform,
    float device_scale_factor,
    float page_scale_factor,
    const Layer* page_scale_layer,
    const Layer* inner_viewport_scroll_layer,
    const Layer* outer_viewport_scroll_layer)
    : root_layer(root_layer),
      device_viewport_size(device_viewport_size),
      device_transform(device_transform),
      device_scale_factor(device_scale_factor),
      page_scale_factor(page_scale_factor),
      page_scale_layer(page_scale_layer),
      inner_viewport_scroll_layer(inner_viewport_scroll_layer),
      outer_viewport_scroll_layer(outer_viewport_scroll_layer) {}

LayerTreeHostCommon::CalcDrawPropsMainInputs::CalcDrawPropsMainInputs(
    Layer* root_layer,
    const gfx::Size& device_viewport_size,
    const gfx::Transform& device_transform)
    : CalcDrawPropsMainInputs(root_layer,
                              device_viewport_size,
                              device_transform,
                              1.f,
                              1.f,
                              NULL,
                              NULL,
                              NULL) {}

LayerTreeHostCommon::CalcDrawPropsMainInputs::CalcDrawPropsMainInputs(
    Layer* root_layer,
    const gfx::Size& device_viewport_size)
    : CalcDrawPropsMainInputs(root_layer,
                              device_viewport_size,
                              gfx::Transform()) {}

LayerTreeHostCommon::CalcDrawPropsImplInputs::CalcDrawPropsImplInputs(
    LayerImpl* root_layer,
    const gfx::Size& device_viewport_size,
    const gfx::Transform& device_transform,
    float device_scale_factor,
    float page_scale_factor,
    const LayerImpl* page_scale_layer,
    const LayerImpl* inner_viewport_scroll_layer,
    const LayerImpl* outer_viewport_scroll_layer,
    const gfx::Vector2dF& elastic_overscroll,
    const LayerImpl* elastic_overscroll_application_layer,
    int max_texture_size,
    bool can_use_lcd_text,
    bool layers_always_allowed_lcd_text,
    bool can_render_to_separate_surface,
    bool can_adjust_raster_scales,
    LayerImplList* render_surface_layer_list,
    int current_render_surface_layer_list_id,
    PropertyTrees* property_trees)
    : root_layer(root_layer),
      device_viewport_size(device_viewport_size),
      device_transform(device_transform),
      device_scale_factor(device_scale_factor),
      page_scale_factor(page_scale_factor),
      page_scale_layer(page_scale_layer),
      inner_viewport_scroll_layer(inner_viewport_scroll_layer),
      outer_viewport_scroll_layer(outer_viewport_scroll_layer),
      elastic_overscroll(elastic_overscroll),
      elastic_overscroll_application_layer(
          elastic_overscroll_application_layer),
      max_texture_size(max_texture_size),
      can_use_lcd_text(can_use_lcd_text),
      layers_always_allowed_lcd_text(layers_always_allowed_lcd_text),
      can_render_to_separate_surface(can_render_to_separate_surface),
      can_adjust_raster_scales(can_adjust_raster_scales),
      render_surface_layer_list(render_surface_layer_list),
      current_render_surface_layer_list_id(
          current_render_surface_layer_list_id),
      property_trees(property_trees) {}

LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting::
    CalcDrawPropsImplInputsForTesting(LayerImpl* root_layer,
                                      const gfx::Size& device_viewport_size,
                                      const gfx::Transform& device_transform,
                                      LayerImplList* render_surface_layer_list,
                                      int current_render_surface_layer_list_id)
    : CalcDrawPropsImplInputs(root_layer,
                              device_viewport_size,
                              device_transform,
                              1.f,
                              1.f,
                              NULL,
                              NULL,
                              NULL,
                              gfx::Vector2dF(),
                              NULL,
                              std::numeric_limits<int>::max() / 2,
                              false,
                              false,
                              true,
                              false,
                              render_surface_layer_list,
                              current_render_surface_layer_list_id,
                              GetPropertyTrees(root_layer)) {
  DCHECK(root_layer);
  DCHECK(render_surface_layer_list);
}

LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting::
    CalcDrawPropsImplInputsForTesting(LayerImpl* root_layer,
                                      const gfx::Size& device_viewport_size,
                                      LayerImplList* render_surface_layer_list,
                                      int current_render_surface_layer_list_id)
    : CalcDrawPropsImplInputsForTesting(root_layer,
                                        device_viewport_size,
                                        gfx::Transform(),
                                        render_surface_layer_list,
                                        current_render_surface_layer_list_id) {}

bool LayerTreeHostCommon::ScrollUpdateInfo::operator==(
    const LayerTreeHostCommon::ScrollUpdateInfo& other) const {
  return layer_id == other.layer_id && scroll_delta == other.scroll_delta;
}

void LayerTreeHostCommon::ScrollUpdateInfo::ToProtobuf(
    proto::ScrollUpdateInfo* proto) const {
  proto->set_layer_id(layer_id);
  Vector2dToProto(scroll_delta, proto->mutable_scroll_delta());
}

void LayerTreeHostCommon::ScrollUpdateInfo::FromProtobuf(
    const proto::ScrollUpdateInfo& proto) {
  layer_id = proto.layer_id();
  scroll_delta = ProtoToVector2d(proto.scroll_delta());
}

ScrollAndScaleSet::ScrollAndScaleSet()
    : page_scale_delta(1.f), top_controls_delta(0.f) {
}

ScrollAndScaleSet::~ScrollAndScaleSet() {}

bool ScrollAndScaleSet::EqualsForTesting(const ScrollAndScaleSet& other) const {
  return scrolls == other.scrolls &&
         page_scale_delta == other.page_scale_delta &&
         elastic_overscroll_delta == other.elastic_overscroll_delta &&
         top_controls_delta == other.top_controls_delta;
}

void ScrollAndScaleSet::ToProtobuf(proto::ScrollAndScaleSet* proto) const {
  for (const auto& scroll : scrolls)
    scroll.ToProtobuf(proto->add_scrolls());
  proto->set_page_scale_delta(page_scale_delta);
  Vector2dFToProto(elastic_overscroll_delta,
                   proto->mutable_elastic_overscroll_delta());
  proto->set_top_controls_delta(top_controls_delta);
}

void ScrollAndScaleSet::FromProtobuf(const proto::ScrollAndScaleSet& proto) {
  DCHECK_EQ(scrolls.size(), 0u);
  for (int i = 0; i < proto.scrolls_size(); ++i) {
    scrolls.push_back(LayerTreeHostCommon::ScrollUpdateInfo());
    scrolls[i].FromProtobuf(proto.scrolls(i));
  }
  page_scale_delta = proto.page_scale_delta();
  elastic_overscroll_delta = ProtoToVector2dF(proto.elastic_overscroll_delta());
  top_controls_delta = proto.top_controls_delta();
}

inline gfx::Rect CalculateVisibleRectWithCachedLayerRect(
    const gfx::Rect& target_surface_rect,
    const gfx::Rect& layer_bound_rect,
    const gfx::Rect& layer_rect_in_target_space,
    const gfx::Transform& transform) {
  if (layer_rect_in_target_space.IsEmpty())
    return gfx::Rect();

  // Is this layer fully contained within the target surface?
  if (target_surface_rect.Contains(layer_rect_in_target_space))
    return layer_bound_rect;

  // If the layer doesn't fill up the entire surface, then find the part of
  // the surface rect where the layer could be visible. This avoids trying to
  // project surface rect points that are behind the projection point.
  gfx::Rect minimal_surface_rect = target_surface_rect;
  minimal_surface_rect.Intersect(layer_rect_in_target_space);

  if (minimal_surface_rect.IsEmpty())
    return gfx::Rect();

  // Project the corners of the target surface rect into the layer space.
  // This bounding rectangle may be larger than it needs to be (being
  // axis-aligned), but is a reasonable filter on the space to consider.
  // Non-invertible transforms will create an empty rect here.

  gfx::Transform surface_to_layer(gfx::Transform::kSkipInitialization);
  if (!transform.GetInverse(&surface_to_layer)) {
    // Because we cannot use the surface bounds to determine what portion of
    // the layer is visible, we must conservatively assume the full layer is
    // visible.
    return layer_bound_rect;
  }

  gfx::Rect layer_rect = MathUtil::ProjectEnclosingClippedRect(
      surface_to_layer, minimal_surface_rect);
  layer_rect.Intersect(layer_bound_rect);
  return layer_rect;
}

gfx::Rect LayerTreeHostCommon::CalculateVisibleRect(
    const gfx::Rect& target_surface_rect,
    const gfx::Rect& layer_bound_rect,
    const gfx::Transform& transform) {
  gfx::Rect layer_in_surface_space =
      MathUtil::MapEnclosingClippedRect(transform, layer_bound_rect);
  return CalculateVisibleRectWithCachedLayerRect(
      target_surface_rect, layer_bound_rect, layer_in_surface_space, transform);
}

static const LayerImpl* NextTargetSurface(const LayerImpl* layer) {
  return layer->parent() ? layer->parent()->render_target() : 0;
}

// Given two layers, this function finds their respective render targets and,
// computes a change of basis translation. It does this by accumulating the
// translation components of the draw transforms of each target between the
// ancestor and descendant. These transforms must be 2D translations, and this
// requirement is enforced at every step.
static gfx::Vector2dF ComputeChangeOfBasisTranslation(
    const LayerImpl& ancestor_layer,
    const LayerImpl& descendant_layer) {
  DCHECK(descendant_layer.HasAncestor(&ancestor_layer));
  const LayerImpl* descendant_target = descendant_layer.render_target();
  DCHECK(descendant_target);
  const LayerImpl* ancestor_target = ancestor_layer.render_target();
  DCHECK(ancestor_target);

  gfx::Vector2dF translation;
  for (const LayerImpl* target = descendant_target; target != ancestor_target;
       target = NextTargetSurface(target)) {
    const gfx::Transform& trans = target->render_surface()->draw_transform();
    // Ensure that this translation is truly 2d.
    DCHECK(trans.IsIdentityOrTranslation());
    DCHECK_EQ(0.f, trans.matrix().get(2, 3));
    translation += trans.To2dTranslation();
  }

  return translation;
}

enum TranslateRectDirection {
  TRANSLATE_RECT_DIRECTION_TO_ANCESTOR,
  TRANSLATE_RECT_DIRECTION_TO_DESCENDANT
};

static gfx::Rect TranslateRectToTargetSpace(const LayerImpl& ancestor_layer,
                                            const LayerImpl& descendant_layer,
                                            const gfx::Rect& rect,
                                            TranslateRectDirection direction) {
  gfx::Vector2dF translation =
      ComputeChangeOfBasisTranslation(ancestor_layer, descendant_layer);
  if (direction == TRANSLATE_RECT_DIRECTION_TO_DESCENDANT)
    translation.Scale(-1.f);
  gfx::RectF rect_f = gfx::RectF(rect);
  return gfx::ToEnclosingRect(
      gfx::RectF(rect_f.origin() + translation, rect_f.size()));
}

// We collect an accumulated drawable content rect per render surface.
// Typically, a layer will contribute to only one surface, the surface
// associated with its render target. Clip children, however, may affect
// several surfaces since there may be several surfaces between the clip child
// and its parent.
//
// NB: we accumulate the layer's *clipped* drawable content rect.
struct AccumulatedSurfaceState {
  explicit AccumulatedSurfaceState(LayerImpl* render_target)
      : render_target(render_target) {}

  // The accumulated drawable content rect for the surface associated with the
  // given |render_target|.
  gfx::Rect drawable_content_rect;

  // The target owning the surface. (We hang onto the target rather than the
  // surface so that we can DCHECK that the surface's draw transform is simply
  // a translation when |render_target| reports that it has no unclipped
  // descendants).
  LayerImpl* render_target;
};

template <typename LayerType>
static inline bool IsRootLayer(LayerType* layer) {
  return !layer->parent();
}

void UpdateAccumulatedSurfaceState(
    LayerImpl* layer,
    const gfx::Rect& drawable_content_rect,
    std::vector<AccumulatedSurfaceState>* accumulated_surface_state) {
  if (IsRootLayer(layer))
    return;

  // We will apply our drawable content rect to the accumulated rects for all
  // surfaces between us and |render_target| (inclusive). This is either our
  // clip parent's target if we are a clip child, or else simply our parent's
  // target. We use our parent's target because we're either the owner of a
  // render surface and we'll want to add our rect to our *surface's* target, or
  // we're not and our target is the same as our parent's. In both cases, the
  // parent's target gives us what we want.
  LayerImpl* render_target = layer->clip_parent()
                                 ? layer->clip_parent()->render_target()
                                 : layer->parent()->render_target();

  // If the layer owns a surface, then the content rect is in the wrong space.
  // Instead, we will use the surface's DrawableContentRect which is in target
  // space as required.
  gfx::Rect target_rect = drawable_content_rect;
  if (layer->render_surface()) {
    target_rect =
        gfx::ToEnclosedRect(layer->render_surface()->DrawableContentRect());
  }

  if (render_target->is_clipped()) {
    gfx::Rect clip_rect = render_target->clip_rect();
    // If the layer has a clip parent, the clip rect may be in the wrong space,
    // so we'll need to transform it before it is applied.
    if (layer->clip_parent()) {
      clip_rect =
          TranslateRectToTargetSpace(*layer->clip_parent(), *layer, clip_rect,
                                     TRANSLATE_RECT_DIRECTION_TO_DESCENDANT);
    }
    target_rect.Intersect(clip_rect);
  }

  // We must have at least one entry in the vector for the root.
  DCHECK_LT(0ul, accumulated_surface_state->size());

  typedef std::vector<AccumulatedSurfaceState> AccumulatedSurfaceStateVector;
  typedef AccumulatedSurfaceStateVector::reverse_iterator
      AccumulatedSurfaceStateIterator;
  AccumulatedSurfaceStateIterator current_state =
      accumulated_surface_state->rbegin();

  // Add this rect to the accumulated content rect for all surfaces until we
  // reach the target surface.
  bool found_render_target = false;
  for (; current_state != accumulated_surface_state->rend(); ++current_state) {
    current_state->drawable_content_rect.Union(target_rect);

    // If we've reached |render_target| our work is done and we can bail.
    if (current_state->render_target == render_target) {
      found_render_target = true;
      break;
    }

    // Transform rect from the current target's space to the next.
    LayerImpl* current_target = current_state->render_target;
    DCHECK(current_target->render_surface());
    const gfx::Transform& current_draw_transform =
         current_target->render_surface()->draw_transform();

    // If we have unclipped descendants, the draw transform is a translation.
    DCHECK(!current_target->num_unclipped_descendants() ||
           current_draw_transform.IsIdentityOrTranslation());

    target_rect =
        MathUtil::MapEnclosingClippedRect(current_draw_transform, target_rect);
  }

  // It is an error to not reach |render_target|. If this happens, it means that
  // either the clip parent is not an ancestor of the clip child or the surface
  // state vector is empty, both of which should be impossible.
  DCHECK(found_render_target);
}

template <typename LayerType>
static inline bool LayerIsInExisting3DRenderingContext(LayerType* layer) {
  return layer->Is3dSorted() && layer->parent() &&
         layer->parent()->Is3dSorted() &&
         (layer->parent()->sorting_context_id() == layer->sorting_context_id());
}

static bool IsRootLayerOfNewRenderingContext(LayerImpl* layer) {
  if (layer->parent())
    return !layer->parent()->Is3dSorted() && layer->Is3dSorted();

  return layer->Is3dSorted();
}

static bool IsLayerBackFaceVisible(LayerImpl* layer,
                                   const TransformTree& transform_tree) {
  // The current W3C spec on CSS transforms says that backface visibility should
  // be determined differently depending on whether the layer is in a "3d
  // rendering context" or not. For Chromium code, we can determine whether we
  // are in a 3d rendering context by checking if the parent preserves 3d.

  if (LayerIsInExisting3DRenderingContext(layer)) {
    return DrawTransformFromPropertyTrees(layer, transform_tree)
        .IsBackFaceVisible();
  }

  // In this case, either the layer establishes a new 3d rendering context, or
  // is not in a 3d rendering context at all.
  return layer->transform().IsBackFaceVisible();
}

static bool IsSurfaceBackFaceVisible(LayerImpl* layer,
                                     const gfx::Transform& draw_transform) {
  if (LayerIsInExisting3DRenderingContext(layer))
    return draw_transform.IsBackFaceVisible();

  if (IsRootLayerOfNewRenderingContext(layer))
    return layer->transform().IsBackFaceVisible();

  // If the render_surface is not part of a new or existing rendering context,
  // then the layers that contribute to this surface will decide back-face
  // visibility for themselves.
  return false;
}

template <typename LayerType>
static inline bool LayerClipsSubtree(LayerType* layer) {
  return layer->masks_to_bounds() || layer->mask_layer();
}

static bool LayerShouldBeSkipped(LayerImpl* layer,
                                 bool layer_is_drawn,
                                 const TransformTree& transform_tree) {
  // Layers can be skipped if any of these conditions are met.
  //   - is not drawn due to it or one of its ancestors being hidden (or having
  //     no copy requests).
  //   - does not draw content.
  //   - is transparent.
  //   - has empty bounds
  //   - the layer is not double-sided, but its back face is visible.
  //
  // Some additional conditions need to be computed at a later point after the
  // recursion is finished.
  //   - the intersection of render_surface content and layer clip_rect is empty
  //   - the visible_layer_rect is empty
  //
  // Note, if the layer should not have been drawn due to being fully
  // transparent, we would have skipped the entire subtree and never made it
  // into this function, so it is safe to omit this check here.

  if (!layer_is_drawn)
    return true;

  if (!layer->DrawsContent() || layer->bounds().IsEmpty())
    return true;

  LayerImpl* backface_test_layer = layer;
  if (layer->use_parent_backface_visibility()) {
    DCHECK(layer->parent());
    DCHECK(!layer->parent()->use_parent_backface_visibility());
    backface_test_layer = layer->parent();
  }

  // The layer should not be drawn if (1) it is not double-sided and (2) the
  // back of the layer is known to be facing the screen.
  if (!backface_test_layer->double_sided() &&
      IsLayerBackFaceVisible(backface_test_layer, transform_tree))
    return true;

  return false;
}

template <typename LayerType>
static bool HasInvertibleOrAnimatedTransform(LayerType* layer) {
  return layer->transform_is_invertible() ||
         layer->HasPotentiallyRunningTransformAnimation();
}

static inline bool SubtreeShouldBeSkipped(LayerImpl* layer,
                                          bool layer_is_drawn) {
  // If the layer transform is not invertible, it should not be drawn.
  // TODO(ajuma): Correctly process subtrees with singular transform for the
  // case where we may animate to a non-singular transform and wish to
  // pre-raster.
  if (!HasInvertibleOrAnimatedTransform(layer))
    return true;

  // When we need to do a readback/copy of a layer's output, we can not skip
  // it or any of its ancestors.
  if (layer->num_copy_requests_in_target_subtree() > 0)
    return false;

  // We cannot skip the the subtree if a descendant has a touch handler
  // or the hit testing code will break (it requires fresh transforms, etc).
  if (layer->layer_or_descendant_has_touch_handler())
    return false;

  // If the layer is not drawn, then skip it and its subtree.
  if (!layer_is_drawn)
    return true;

  // If layer is on the pending tree and opacity is being animated then
  // this subtree can't be skipped as we need to create, prioritize and
  // include tiles for this layer when deciding if tree can be activated.
  if (layer->layer_tree_impl()->IsPendingTree() &&
      layer->HasPotentiallyRunningOpacityAnimation())
    return false;

  // If layer has a background filter, don't skip the layer, even it the
  // opacity is 0.
  if (!layer->background_filters().IsEmpty())
    return false;

  // The opacity of a layer always applies to its children (either implicitly
  // via a render surface or explicitly if the parent preserves 3D), so the
  // entire subtree can be skipped if this layer is fully transparent.
  return !layer->EffectiveOpacity();
}

// This function returns a translation matrix that can be applied on a vector
// that's in the layer's target surface coordinate, while the position offset is
// specified in some ancestor layer's coordinate.
gfx::Transform ComputeSizeDeltaCompensation(
    LayerImpl* layer,
    LayerImpl* container,
    const gfx::Vector2dF& position_offset) {
  gfx::Transform result_transform;

  // To apply a translate in the container's layer space,
  // the following steps need to be done:
  //     Step 1a. transform from target surface space to the container's target
  //              surface space
  //     Step 1b. transform from container's target surface space to the
  //              container's layer space
  //     Step 2. apply the compensation
  //     Step 3. transform back to target surface space

  gfx::Transform target_surface_space_to_container_layer_space;
  // Calculate step 1a
  LayerImpl* container_target_surface = container->render_target();
  for (const LayerImpl* current_target_surface = NextTargetSurface(layer);
       current_target_surface &&
       current_target_surface != container_target_surface;
       current_target_surface = NextTargetSurface(current_target_surface)) {
    // Note: Concat is used here to convert the result coordinate space from
    //       current render surface to the next render surface.
    target_surface_space_to_container_layer_space.ConcatTransform(
        current_target_surface->render_surface()->draw_transform());
  }
  // Calculate step 1b
  gfx::Transform container_layer_space_to_container_target_surface_space =
      container->draw_properties().target_space_transform;
  gfx::Transform container_target_surface_space_to_container_layer_space;
  if (container_layer_space_to_container_target_surface_space.GetInverse(
      &container_target_surface_space_to_container_layer_space)) {
    // Note: Again, Concat is used to conver the result coordinate space from
    //       the container render surface to the container layer.
    target_surface_space_to_container_layer_space.ConcatTransform(
        container_target_surface_space_to_container_layer_space);
  }

  // Apply step 3
  gfx::Transform container_layer_space_to_target_surface_space;
  if (target_surface_space_to_container_layer_space.GetInverse(
          &container_layer_space_to_target_surface_space)) {
    result_transform.PreconcatTransform(
        container_layer_space_to_target_surface_space);
  } else {
    // TODO(shawnsingh): A non-invertible matrix could still make meaningful
    // projection.  For example ScaleZ(0) is non-invertible but the layer is
    // still visible.
    return gfx::Transform();
  }

  // Apply step 2
  result_transform.Translate(position_offset.x(), position_offset.y());

  // Apply step 1
  result_transform.PreconcatTransform(
      target_surface_space_to_container_layer_space);

  return result_transform;
}

void ApplyPositionAdjustment(LayerImpl* layer,
                             LayerImpl* container,
                             const gfx::Transform& scroll_compensation,
                             gfx::Transform* combined_transform) {
  if (!layer->position_constraint().is_fixed_position())
    return;

  // Special case: this layer is a composited fixed-position layer; we need to
  // explicitly compensate for all ancestors' nonzero scroll_deltas to keep
  // this layer fixed correctly.
  // Note carefully: this is Concat, not Preconcat
  // (current_scroll_compensation * combined_transform).
  combined_transform->ConcatTransform(scroll_compensation);

  // For right-edge or bottom-edge anchored fixed position layers,
  // the layer should relocate itself if the container changes its size.
  bool fixed_to_right_edge =
      layer->position_constraint().is_fixed_to_right_edge();
  bool fixed_to_bottom_edge =
      layer->position_constraint().is_fixed_to_bottom_edge();
  gfx::Vector2dF position_offset = container->FixedContainerSizeDelta();
  position_offset.set_x(fixed_to_right_edge ? position_offset.x() : 0);
  position_offset.set_y(fixed_to_bottom_edge ? position_offset.y() : 0);
  if (position_offset.IsZero())
    return;

  // Note: Again, this is Concat. The compensation matrix will be applied on
  //       the vector in target surface space.
  combined_transform->ConcatTransform(
      ComputeSizeDeltaCompensation(layer, container, position_offset));
}

gfx::Transform ComputeScrollCompensationForThisLayer(
    LayerImpl* scrolling_layer,
    const gfx::Transform& parent_matrix,
    const gfx::Vector2dF& scroll_delta) {
  // For every layer that has non-zero scroll_delta, we have to compute a
  // transform that can undo the scroll_delta translation. In particular, we
  // want this matrix to premultiply a fixed-position layer's parent_matrix, so
  // we design this transform in three steps as follows. The steps described
  // here apply from right-to-left, so Step 1 would be the right-most matrix:
  //
  //     Step 1. transform from target surface space to the exact space where
  //           scroll_delta is actually applied.
  //           -- this is inverse of parent_matrix
  //     Step 2. undo the scroll_delta
  //           -- this is just a translation by scroll_delta.
  //     Step 3. transform back to target surface space.
  //           -- this transform is the parent_matrix
  //
  // These steps create a matrix that both start and end in target surface
  // space. So this matrix can pre-multiply any fixed-position layer's
  // draw_transform to undo the scroll_deltas -- as long as that fixed position
  // layer is fixed onto the same render_target as this scrolling_layer.
  //

  gfx::Transform scroll_compensation_for_this_layer = parent_matrix;  // Step 3
  scroll_compensation_for_this_layer.Translate(
      scroll_delta.x(),
      scroll_delta.y());  // Step 2

  gfx::Transform inverse_parent_matrix(gfx::Transform::kSkipInitialization);
  if (!parent_matrix.GetInverse(&inverse_parent_matrix)) {
    // TODO(shawnsingh): Either we need to handle uninvertible transforms
    // here, or DCHECK that the transform is invertible.
  }
  scroll_compensation_for_this_layer.PreconcatTransform(
      inverse_parent_matrix);  // Step 1
  return scroll_compensation_for_this_layer;
}

gfx::Transform ComputeScrollCompensationMatrixForChildren(
    LayerImpl* layer,
    const gfx::Transform& parent_matrix,
    const gfx::Transform& current_scroll_compensation_matrix,
    const gfx::Vector2dF& scroll_delta) {
  // "Total scroll compensation" is the transform needed to cancel out all
  // scroll_delta translations that occurred since the nearest container layer,
  // even if there are render_surfaces in-between.
  //
  // There are some edge cases to be aware of, that are not explicit in the
  // code:
  //  - A layer that is both a fixed-position and container should not be its
  //  own container, instead, that means it is fixed to an ancestor, and is a
  //  container for any fixed-position descendants.
  //  - A layer that is a fixed-position container and has a render_surface
  //  should behave the same as a container without a render_surface, the
  //  render_surface is irrelevant in that case.
  //  - A layer that does not have an explicit container is simply fixed to the
  //  viewport.  (i.e. the root render_surface.)
  //  - If the fixed-position layer has its own render_surface, then the
  //  render_surface is the one who gets fixed.
  //
  // This function needs to be called AFTER layers create their own
  // render_surfaces.
  //

  // Scroll compensation restarts from identity under two possible conditions:
  //  - the current layer is a container for fixed-position descendants
  //  - the current layer is fixed-position itself, so any fixed-position
  //    descendants are positioned with respect to this layer. Thus, any
  //    fixed position descendants only need to compensate for scrollDeltas
  //    that occur below this layer.
  bool current_layer_resets_scroll_compensation_for_descendants =
      layer->IsContainerForFixedPositionLayers() ||
      layer->position_constraint().is_fixed_position();

  // Avoid the overheads (including stack allocation and matrix
  // initialization/copy) if we know that the scroll compensation doesn't need
  // to be reset or adjusted.
  if (!current_layer_resets_scroll_compensation_for_descendants &&
      scroll_delta.IsZero() && !layer->render_surface())
    return current_scroll_compensation_matrix;

  // Start as identity matrix.
  gfx::Transform next_scroll_compensation_matrix;

  // If this layer does not reset scroll compensation, then it inherits the
  // existing scroll compensations.
  if (!current_layer_resets_scroll_compensation_for_descendants)
    next_scroll_compensation_matrix = current_scroll_compensation_matrix;

  // If the current layer has a non-zero scroll_delta, then we should compute
  // its local scroll compensation and accumulate it to the
  // next_scroll_compensation_matrix.
  if (!scroll_delta.IsZero()) {
    gfx::Transform scroll_compensation_for_this_layer =
        ComputeScrollCompensationForThisLayer(
            layer, parent_matrix, scroll_delta);
    next_scroll_compensation_matrix.PreconcatTransform(
        scroll_compensation_for_this_layer);
  }

  // If the layer created its own render_surface, we have to adjust
  // next_scroll_compensation_matrix.  The adjustment allows us to continue
  // using the scroll compensation on the next surface.
  //  Step 1 (right-most in the math): transform from the new surface to the
  //  original ancestor surface
  //  Step 2: apply the scroll compensation
  //  Step 3: transform back to the new surface.
  if (layer->render_surface() &&
      !next_scroll_compensation_matrix.IsIdentity()) {
    gfx::Transform inverse_surface_draw_transform(
        gfx::Transform::kSkipInitialization);
    if (!layer->render_surface()->draw_transform().GetInverse(
            &inverse_surface_draw_transform)) {
      // TODO(shawnsingh): Either we need to handle uninvertible transforms
      // here, or DCHECK that the transform is invertible.
    }
    next_scroll_compensation_matrix =
        inverse_surface_draw_transform * next_scroll_compensation_matrix *
        layer->render_surface()->draw_transform();
  }

  return next_scroll_compensation_matrix;
}

static inline void MarkLayerWithRenderSurfaceLayerListId(
    LayerImpl* layer,
    int current_render_surface_layer_list_id) {
  layer->draw_properties().last_drawn_render_surface_layer_list_id =
      current_render_surface_layer_list_id;
  layer->set_layer_or_descendant_is_drawn(
      !!current_render_surface_layer_list_id);
}

static inline void MarkMasksWithRenderSurfaceLayerListId(
    LayerImpl* layer,
    int current_render_surface_layer_list_id) {
  if (layer->mask_layer()) {
    MarkLayerWithRenderSurfaceLayerListId(layer->mask_layer(),
                                          current_render_surface_layer_list_id);
  }
  if (layer->replica_layer() && layer->replica_layer()->mask_layer()) {
    MarkLayerWithRenderSurfaceLayerListId(layer->replica_layer()->mask_layer(),
                                          current_render_surface_layer_list_id);
  }
}

static inline void MarkLayerListWithRenderSurfaceLayerListId(
    LayerImplList* layer_list,
    int current_render_surface_layer_list_id) {
  for (LayerImplList::iterator it = layer_list->begin();
       it != layer_list->end(); ++it) {
    MarkLayerWithRenderSurfaceLayerListId(*it,
                                          current_render_surface_layer_list_id);
    MarkMasksWithRenderSurfaceLayerListId(*it,
                                          current_render_surface_layer_list_id);
  }
}

static inline void RemoveSurfaceForEarlyExit(
    LayerImpl* layer_to_remove,
    LayerImplList* render_surface_layer_list) {
  DCHECK(layer_to_remove->render_surface());
  // Technically, we know that the layer we want to remove should be
  // at the back of the render_surface_layer_list. However, we have had
  // bugs before that added unnecessary layers here
  // (https://bugs.webkit.org/show_bug.cgi?id=74147), but that causes
  // things to crash. So here we proactively remove any additional
  // layers from the end of the list.
  while (render_surface_layer_list->back() != layer_to_remove) {
    MarkLayerListWithRenderSurfaceLayerListId(
        &render_surface_layer_list->back()->render_surface()->layer_list(), 0);
    MarkLayerWithRenderSurfaceLayerListId(render_surface_layer_list->back(), 0);

    render_surface_layer_list->back()->ClearRenderSurfaceLayerList();
    render_surface_layer_list->pop_back();
  }
  DCHECK_EQ(render_surface_layer_list->back(), layer_to_remove);
  MarkLayerListWithRenderSurfaceLayerListId(
      &layer_to_remove->render_surface()->layer_list(), 0);
  MarkLayerWithRenderSurfaceLayerListId(layer_to_remove, 0);
  render_surface_layer_list->pop_back();
  layer_to_remove->ClearRenderSurfaceLayerList();
}

struct PreCalculateMetaInformationRecursiveData {
  size_t num_unclipped_descendants;
  int num_layer_or_descendants_with_copy_request;
  int num_layer_or_descendants_with_touch_handler;
  int num_descendants_that_draw_content;

  PreCalculateMetaInformationRecursiveData()
      : num_unclipped_descendants(0),
        num_layer_or_descendants_with_copy_request(0),
        num_layer_or_descendants_with_touch_handler(0),
        num_descendants_that_draw_content(0) {}

  void Merge(const PreCalculateMetaInformationRecursiveData& data) {
    num_layer_or_descendants_with_copy_request +=
        data.num_layer_or_descendants_with_copy_request;
    num_layer_or_descendants_with_touch_handler +=
        data.num_layer_or_descendants_with_touch_handler;
    num_unclipped_descendants += data.num_unclipped_descendants;
    num_descendants_that_draw_content += data.num_descendants_that_draw_content;
  }
};

static bool IsMetaInformationRecomputationNeeded(Layer* layer) {
  return layer->layer_tree_host()->needs_meta_info_recomputation();
}

static void UpdateMetaInformationSequenceNumber(Layer* root_layer) {
  root_layer->layer_tree_host()->IncrementMetaInformationSequenceNumber();
}

static void UpdateMetaInformationSequenceNumber(LayerImpl* root_layer) {
}

// Recursively walks the layer tree(if needed) to compute any information
// that is needed before doing the main recursion.
static void PreCalculateMetaInformationInternal(
    Layer* layer,
    PreCalculateMetaInformationRecursiveData* recursive_data) {
  if (!IsMetaInformationRecomputationNeeded(layer)) {
    DCHECK(IsRootLayer(layer));
    return;
  }

  layer->set_sorted_for_recursion(false);
  layer->set_layer_or_descendant_is_drawn(false);
  layer->set_visited(false);

  if (layer->clip_parent())
    recursive_data->num_unclipped_descendants++;

  if (!HasInvertibleOrAnimatedTransform(layer)) {
    // Layers with singular transforms should not be drawn, the whole subtree
    // can be skipped.
    return;
  }

  for (size_t i = 0; i < layer->children().size(); ++i) {
    Layer* child_layer = layer->child_at(i);

    PreCalculateMetaInformationRecursiveData data_for_child;
    PreCalculateMetaInformationInternal(child_layer, &data_for_child);
    recursive_data->Merge(data_for_child);
  }

  if (layer->clip_children()) {
    size_t num_clip_children = layer->clip_children()->size();
    DCHECK_GE(recursive_data->num_unclipped_descendants, num_clip_children);
    recursive_data->num_unclipped_descendants -= num_clip_children;
  }

  if (layer->HasCopyRequest())
    recursive_data->num_layer_or_descendants_with_copy_request++;

  if (!layer->touch_event_handler_region().IsEmpty())
    recursive_data->num_layer_or_descendants_with_touch_handler++;

  layer->set_num_unclipped_descendants(
      recursive_data->num_unclipped_descendants);

  if (IsRootLayer(layer))
    layer->layer_tree_host()->SetNeedsMetaInfoRecomputation(false);
}

static void PreCalculateMetaInformationInternal(
    LayerImpl* layer,
    PreCalculateMetaInformationRecursiveData* recursive_data) {
  layer->set_sorted_for_recursion(false);
  layer->draw_properties().has_child_with_a_scroll_parent = false;
  layer->set_layer_or_descendant_is_drawn(false);
  layer->set_visited(false);

  if (layer->clip_parent())
    recursive_data->num_unclipped_descendants++;

  if (!HasInvertibleOrAnimatedTransform(layer)) {
    // Layers with singular transforms should not be drawn, the whole subtree
    // can be skipped.
    return;
  }

  for (size_t i = 0; i < layer->children().size(); ++i) {
    LayerImpl* child_layer = layer->child_at(i);

    PreCalculateMetaInformationRecursiveData data_for_child;
    PreCalculateMetaInformationInternal(child_layer, &data_for_child);

    if (child_layer->scroll_parent())
      layer->draw_properties().has_child_with_a_scroll_parent = true;
    recursive_data->Merge(data_for_child);
  }

  if (layer->clip_children()) {
    size_t num_clip_children = layer->clip_children()->size();
    DCHECK_GE(recursive_data->num_unclipped_descendants, num_clip_children);
    recursive_data->num_unclipped_descendants -= num_clip_children;
  }

  if (layer->HasCopyRequest())
    recursive_data->num_layer_or_descendants_with_copy_request++;

  if (!layer->touch_event_handler_region().IsEmpty())
    recursive_data->num_layer_or_descendants_with_touch_handler++;

  layer->draw_properties().num_unclipped_descendants =
      recursive_data->num_unclipped_descendants;
  layer->set_layer_or_descendant_has_touch_handler(
      (recursive_data->num_layer_or_descendants_with_touch_handler != 0));
  // TODO(enne): this should be synced from the main thread, so is only
  // for tests constructing layers on the compositor thread.
  layer->SetNumDescendantsThatDrawContent(
      recursive_data->num_descendants_that_draw_content);

  if (layer->DrawsContent())
    recursive_data->num_descendants_that_draw_content++;
}

void LayerTreeHostCommon::PreCalculateMetaInformation(Layer* root_layer) {
  PreCalculateMetaInformationRecursiveData recursive_data;
  PreCalculateMetaInformationInternal(root_layer, &recursive_data);
}

void LayerTreeHostCommon::PreCalculateMetaInformationForTesting(
    LayerImpl* root_layer) {
  PreCalculateMetaInformationRecursiveData recursive_data;
  PreCalculateMetaInformationInternal(root_layer, &recursive_data);
}

void LayerTreeHostCommon::PreCalculateMetaInformationForTesting(
    Layer* root_layer) {
  UpdateMetaInformationSequenceNumber(root_layer);
  PreCalculateMetaInformationRecursiveData recursive_data;
  PreCalculateMetaInformationInternal(root_layer, &recursive_data);
}

struct SubtreeGlobals {
  int max_texture_size;
  float device_scale_factor;
  float page_scale_factor;
  const LayerImpl* page_scale_layer;
  gfx::Vector2dF elastic_overscroll;
  const LayerImpl* elastic_overscroll_application_layer;
  bool can_adjust_raster_scales;
  bool can_render_to_separate_surface;
  bool layers_always_allowed_lcd_text;
};

struct DataForRecursion {
  // The accumulated sequence of transforms a layer will use to determine its
  // own draw transform.
  gfx::Transform parent_matrix;

  // The accumulated sequence of transforms a layer will use to determine its
  // own screen-space transform.
  gfx::Transform full_hierarchy_matrix;

  // The transform that removes all scrolling that may have occurred between a
  // fixed-position layer and its container, so that the layer actually does
  // remain fixed.
  gfx::Transform scroll_compensation_matrix;

  // The ancestor that would be the container for any fixed-position / sticky
  // layers.
  LayerImpl* fixed_container;

  // This is the normal clip rect that is propagated from parent to child.
  gfx::Rect clip_rect_in_target_space;

  // When the layer's children want to compute their visible content rect, they
  // want to know what their target surface's clip rect will be. BUT - they
  // want to know this clip rect represented in their own target space. This
  // requires inverse-projecting the surface's clip rect from the surface's
  // render target space down to the surface's own space. Instead of computing
  // this value redundantly for each child layer, it is computed only once
  // while dealing with the parent layer, and then this precomputed value is
  // passed down the recursion to the children that actually use it.
  gfx::Rect clip_rect_of_target_surface_in_target_space;

  // The maximum amount by which this layer will be scaled during the lifetime
  // of currently running animations, considering only scales at keyframes not
  // including the starting keyframe of each animation.
  float maximum_animation_contents_scale;

  // The maximum amout by which this layer will be scaled during the lifetime of
  // currently running animations, consdering only the starting scale of each
  // animation.
  float starting_animation_contents_scale;

  bool ancestor_is_animating_scale;
  bool ancestor_clips_subtree;
  bool in_subtree_of_page_scale_layer;
  bool subtree_can_use_lcd_text;
  bool subtree_is_visible_from_ancestor;
};

static LayerImpl* GetChildContainingLayer(const LayerImpl& parent,
                                          LayerImpl* layer) {
  for (LayerImpl* ancestor = layer; ancestor; ancestor = ancestor->parent()) {
    if (ancestor->parent() == &parent)
      return ancestor;
  }
  NOTREACHED();
  return 0;
}

static void AddScrollParentChain(std::vector<LayerImpl*>* out,
                                 const LayerImpl& parent,
                                 LayerImpl* layer) {
  // At a high level, this function walks up the chain of scroll parents
  // recursively, and once we reach the end of the chain, we add the child
  // of |parent| containing each scroll ancestor as we unwind. The result is
  // an ordering of parent's children that ensures that scroll parents are
  // visited before their descendants.
  // Take for example this layer tree:
  //
  // + stacking_context
  //   + scroll_child (1)
  //   + scroll_parent_graphics_layer (*)
  //   | + scroll_parent_scrolling_layer
  //   |   + scroll_parent_scrolling_content_layer (2)
  //   + scroll_grandparent_graphics_layer (**)
  //     + scroll_grandparent_scrolling_layer
  //       + scroll_grandparent_scrolling_content_layer (3)
  //
  // The scroll child is (1), its scroll parent is (2) and its scroll
  // grandparent is (3). Note, this doesn't mean that (2)'s scroll parent is
  // (3), it means that (*)'s scroll parent is (3). We don't want our list to
  // look like [ (3), (2), (1) ], even though that does have the ancestor chain
  // in the right order. Instead, we want [ (**), (*), (1) ]. That is, only want
  // (1)'s siblings in the list, but we want them to appear in such an order
  // that the scroll ancestors get visited in the correct order.
  //
  // So our first task at this step of the recursion is to determine the layer
  // that we will potentionally add to the list. That is, the child of parent
  // containing |layer|.
  LayerImpl* child = GetChildContainingLayer(parent, layer);
  if (child->sorted_for_recursion())
    return;

  if (LayerImpl* scroll_parent = child->scroll_parent())
    AddScrollParentChain(out, parent, scroll_parent);

  out->push_back(child);
  bool sorted_for_recursion = true;
  child->set_sorted_for_recursion(sorted_for_recursion);
}

static bool CdpPerfTracingEnabled() {
  bool tracing_enabled;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED("cdp.perf", &tracing_enabled);
  return tracing_enabled;
}

static float TranslationFromActiveTreeLayerScreenSpaceTransform(
    LayerImpl* pending_tree_layer) {
  LayerTreeImpl* layer_tree_impl = pending_tree_layer->layer_tree_impl();
  if (layer_tree_impl) {
    LayerImpl* active_tree_layer =
        layer_tree_impl->list()->FindActiveLayerById(pending_tree_layer->id());
    if (active_tree_layer) {
      gfx::Transform active_tree_screen_space_transform =
          active_tree_layer->draw_properties().screen_space_transform;
      if (active_tree_screen_space_transform.IsIdentity())
        return 0.f;
      if (active_tree_screen_space_transform.ApproximatelyEqual(
              pending_tree_layer->draw_properties().screen_space_transform))
        return 0.f;
      return (active_tree_layer->draw_properties()
                  .screen_space_transform.To2dTranslation() -
              pending_tree_layer->draw_properties()
                  .screen_space_transform.To2dTranslation())
          .Length();
    }
  }
  return 0.f;
}

// A layer jitters if its screen space transform is same on two successive
// commits, but has changed in between the commits. CalculateFrameJitter
// computes the jitter in the entire frame.
int LayerTreeHostCommon::CalculateFrameJitter(LayerImpl* layer) {
  if (!layer)
    return 0.f;
  float jitter = 0.f;
  layer->performance_properties().translation_from_last_frame = 0.f;
  layer->performance_properties().last_commit_screen_space_transform =
      layer->draw_properties().screen_space_transform;

  if (!layer->visible_layer_rect().IsEmpty()) {
    if (layer->draw_properties().screen_space_transform.ApproximatelyEqual(
            layer->performance_properties()
                .last_commit_screen_space_transform)) {
      float translation_from_last_commit =
          TranslationFromActiveTreeLayerScreenSpaceTransform(layer);
      if (translation_from_last_commit > 0.f) {
        layer->performance_properties().num_fixed_point_hits++;
        layer->performance_properties().translation_from_last_frame =
            translation_from_last_commit;
        if (layer->performance_properties().num_fixed_point_hits >
            layer->layer_tree_impl()->kFixedPointHitsThreshold) {
          // Jitter = Translation from fixed point * sqrt(Area of the layer).
          // The square root of the area is used instead of the area to match
          // the dimensions of both terms on the rhs.
          jitter += translation_from_last_commit *
                    sqrt(layer->visible_layer_rect().size().GetArea());
        }
      } else {
        layer->performance_properties().num_fixed_point_hits = 0;
      }
    }
  }
  // Descendants of jittering layer will not contribute to unique jitter.
  if (jitter > 0.f)
    return jitter;

  for (size_t i = 0; i < layer->children().size(); ++i) {
    LayerImpl* child_layer =
        LayerTreeHostCommon::get_layer_as_raw_ptr(layer->children(), i);
    jitter += CalculateFrameJitter(child_layer);
  }
  return jitter;
}

enum PropertyTreeOption {
  BUILD_PROPERTY_TREES_IF_NEEDED,
  DONT_BUILD_PROPERTY_TREES
};

void CalculateRenderTargetInternal(LayerImpl* layer,
                                   PropertyTrees* property_trees,
                                   bool subtree_visible_from_ancestor,
                                   bool can_render_to_separate_surface) {
  bool layer_is_drawn;
  DCHECK_GE(layer->effect_tree_index(), 0);
  layer_is_drawn = property_trees->effect_tree.Node(layer->effect_tree_index())
                       ->data.is_drawn;

  // The root layer cannot be skipped.
  if (!IsRootLayer(layer) && SubtreeShouldBeSkipped(layer, layer_is_drawn)) {
    layer->draw_properties().render_target = nullptr;
    return;
  }

  bool render_to_separate_surface =
      IsRootLayer(layer) ||
      (can_render_to_separate_surface && layer->render_surface());

  if (render_to_separate_surface) {
    DCHECK(layer->render_surface()) << IsRootLayer(layer)
                                    << can_render_to_separate_surface
                                    << layer->has_render_surface();
    layer->draw_properties().render_target = layer;

    if (layer->mask_layer())
      layer->mask_layer()->draw_properties().render_target = layer;

    if (layer->replica_layer() && layer->replica_layer()->mask_layer())
      layer->replica_layer()->mask_layer()->draw_properties().render_target =
          layer;

  } else {
    DCHECK(layer->parent());
    layer->draw_properties().render_target = layer->parent()->render_target();
  }

  for (size_t i = 0; i < layer->children().size(); ++i) {
    CalculateRenderTargetInternal(
        LayerTreeHostCommon::get_layer_as_raw_ptr(layer->children(), i),
        property_trees, layer_is_drawn, can_render_to_separate_surface);
  }
}

void CalculateRenderSurfaceLayerListInternal(
    LayerImpl* layer,
    PropertyTrees* property_trees,
    LayerImplList* render_surface_layer_list,
    LayerImplList* descendants,
    RenderSurfaceImpl* nearest_occlusion_immune_ancestor,
    bool subtree_visible_from_ancestor,
    const bool can_render_to_separate_surface,
    const int current_render_surface_layer_list_id,
    const int max_texture_size) {
  // This calculates top level Render Surface Layer List, and Layer List for all
  // Render Surfaces.

  // |layer| is current layer.

  // |render_surface_layer_list| is the top level RenderSurfaceLayerList.

  // |descendants| is used to determine what's in current layer's render
  // surface's layer list.

  // |subtree_visible_from_ancestor| is set during recursion to affect current
  // layer's subtree.

  // |can_render_to_separate_surface| and |current_render_surface_layer_list_id|
  // are settings that should stay the same during recursion.
  bool layer_is_drawn = false;
  DCHECK_GE(layer->effect_tree_index(), 0);
  layer_is_drawn = property_trees->effect_tree.Node(layer->effect_tree_index())
                       ->data.is_drawn;

  // The root layer cannot be skipped.
  if (!IsRootLayer(layer) && SubtreeShouldBeSkipped(layer, layer_is_drawn)) {
    if (layer->render_surface())
      layer->ClearRenderSurfaceLayerList();
    layer->draw_properties().render_target = nullptr;
    return;
  }

  bool render_to_separate_surface =
      IsRootLayer(layer) ||
      (can_render_to_separate_surface && layer->render_surface());

  if (render_to_separate_surface) {
    DCHECK(layer->render_surface());
    RenderSurfaceDrawProperties draw_properties;
    ComputeSurfaceDrawPropertiesUsingPropertyTrees(
        layer->render_surface(), property_trees, &draw_properties);
    // TODO(ajuma): Once property tree verification is removed, make the above
    // call directly set the surface's properties, so that the copying below
    // is no longer needed.
    layer->render_surface()->SetIsClipped(draw_properties.is_clipped);
    layer->render_surface()->SetDrawOpacity(draw_properties.draw_opacity);
    layer->render_surface()->SetDrawTransform(draw_properties.draw_transform);
    layer->render_surface()->SetScreenSpaceTransform(
        draw_properties.screen_space_transform);
    layer->render_surface()->SetReplicaDrawTransform(
        draw_properties.replica_draw_transform);
    layer->render_surface()->SetReplicaScreenSpaceTransform(
        draw_properties.replica_screen_space_transform);
    layer->render_surface()->SetClipRect(draw_properties.clip_rect);

    if (!layer->double_sided() &&
        IsSurfaceBackFaceVisible(layer,
                                 layer->render_surface()->draw_transform())) {
      layer->ClearRenderSurfaceLayerList();
      layer->draw_properties().render_target = nullptr;
      return;
    }

    if (IsRootLayer(layer)) {
      // The root surface does not contribute to any other surface, it has no
      // target.
      layer->render_surface()->set_contributes_to_drawn_surface(false);
    } else {
      bool contributes_to_drawn_surface =
          property_trees->effect_tree.ContributesToDrawnSurface(
              layer->effect_tree_index());
      layer->render_surface()->set_contributes_to_drawn_surface(
          contributes_to_drawn_surface);
    }

    // Ignore occlusion from outside the surface when surface contents need to
    // be fully drawn. Layers with copy-request need to be complete.
    // We could be smarter about layers with replica and exclude regions
    // where both layer and the replica are occluded, but this seems like an
    // overkill. The same is true for layers with filters that move pixels.
    // TODO(senorblanco): make this smarter for the SkImageFilter case (check
    // for pixel-moving filters)
    if (layer->HasCopyRequest() || layer->has_replica() ||
        layer->filters().HasReferenceFilter() ||
        layer->filters().HasFilterThatMovesPixels()) {
      nearest_occlusion_immune_ancestor = layer->render_surface();
    }
    layer->render_surface()->SetNearestOcclusionImmuneAncestor(
        nearest_occlusion_immune_ancestor);
    layer->ClearRenderSurfaceLayerList();

    render_surface_layer_list->push_back(layer);

    descendants = &(layer->render_surface()->layer_list());
  }

  size_t descendants_size = descendants->size();

  bool layer_should_be_skipped = LayerShouldBeSkipped(
      layer, layer_is_drawn, property_trees->transform_tree);
  if (!layer_should_be_skipped) {
    MarkLayerWithRenderSurfaceLayerListId(layer,
                                          current_render_surface_layer_list_id);
    descendants->push_back(layer);
  }


  // Clear the old accumulated content rect of surface.
  if (render_to_separate_surface)
    layer->render_surface()->SetAccumulatedContentRect(gfx::Rect());

  for (const auto& child_layer : layer->children()) {
    CalculateRenderSurfaceLayerListInternal(
        child_layer.get(), property_trees, render_surface_layer_list,
        descendants, nearest_occlusion_immune_ancestor, layer_is_drawn,
        can_render_to_separate_surface, current_render_surface_layer_list_id,
        max_texture_size);

    // If the child is its own render target, then it has a render surface.
    if (child_layer->render_target() == child_layer.get() &&
        !child_layer->render_surface()->layer_list().empty() &&
        !child_layer->render_surface()->content_rect().IsEmpty()) {
      // This child will contribute its render surface, which means
      // we need to mark just the mask layer (and replica mask layer)
      // with the id.
      MarkMasksWithRenderSurfaceLayerListId(
          child_layer.get(), current_render_surface_layer_list_id);
      descendants->push_back(child_layer.get());
    }

    if (child_layer->layer_or_descendant_is_drawn()) {
      bool layer_or_descendant_is_drawn = true;
      layer->set_layer_or_descendant_is_drawn(layer_or_descendant_is_drawn);
    }
  }

  if (render_to_separate_surface && !IsRootLayer(layer) &&
      layer->render_surface()->layer_list().empty()) {
    RemoveSurfaceForEarlyExit(layer, render_surface_layer_list);
    return;
  }

  // The render surface's content rect is the union of drawable content rects
  // of the layers that draw into the surface. If the render surface is clipped,
  // it is also intersected with the render's surface clip rect.
  if (!IsRootLayer(layer)) {
    if (render_to_separate_surface) {
      gfx::Rect surface_content_rect =
          layer->render_surface()->accumulated_content_rect();
      // If the owning layer of a render surface draws content, the content
      // rect of the render surface is expanded to include the drawable
      // content rect of the layer.
      if (layer->DrawsContent())
        surface_content_rect.Union(layer->drawable_content_rect());

      if (!layer->replica_layer() && !layer->HasCopyRequest() &&
          layer->render_surface()->is_clipped()) {
        // Here, we clip the render surface's content rect with its clip rect.
        // As the clip rect of render surface is in the surface's target
        // space, we first map the content rect into the target space,
        // intersect it with clip rect and project back the result to the
        // surface space.
        if (!surface_content_rect.IsEmpty()) {
          gfx::Rect surface_clip_rect =
              LayerTreeHostCommon::CalculateVisibleRect(
                  layer->render_surface()->clip_rect(), surface_content_rect,
                  layer->render_surface()->draw_transform());
          surface_content_rect.Intersect(surface_clip_rect);
        }
      }
      // The RenderSurfaceImpl backing texture cannot exceed the maximum
      // supported texture size.
      surface_content_rect.set_width(
          std::min(surface_content_rect.width(), max_texture_size));
      surface_content_rect.set_height(
          std::min(surface_content_rect.height(), max_texture_size));
      layer->render_surface()->SetContentRect(surface_content_rect);
    }
    const LayerImpl* parent_target = layer->parent()->render_target();
    if (!IsRootLayer(parent_target)) {
      gfx::Rect surface_content_rect =
          parent_target->render_surface()->accumulated_content_rect();
      if (render_to_separate_surface) {
        // If the layer owns a surface, then the content rect is in the wrong
        // space. Instead, we will use the surface's DrawableContentRect which
        // is in target space as required. We also need to clip it with the
        // target's clip if the target is clipped.
        surface_content_rect.Union(gfx::ToEnclosedRect(
            layer->render_surface()->DrawableContentRect()));
        if (parent_target->is_clipped())
          surface_content_rect.Intersect(parent_target->clip_rect());
      } else if (layer->DrawsContent()) {
        surface_content_rect.Union(layer->drawable_content_rect());
      }
      parent_target->render_surface()->SetAccumulatedContentRect(
          surface_content_rect);
    }
  } else {
    // The root layer's surface content rect is always the entire viewport.
    gfx::Rect viewport =
        gfx::ToEnclosingRect(property_trees->clip_tree.ViewportClip());
    layer->render_surface()->SetContentRect(viewport);
  }

  if (render_to_separate_surface && !IsRootLayer(layer) &&
      layer->render_surface()->DrawableContentRect().IsEmpty()) {
    RemoveSurfaceForEarlyExit(layer, render_surface_layer_list);
    return;
  }

  // If neither this layer nor any of its children were added, early out.
  if (descendants_size == descendants->size()) {
    DCHECK(!render_to_separate_surface || IsRootLayer(layer));
    return;
  }
}

void CalculateRenderTarget(
    LayerTreeHostCommon::CalcDrawPropsImplInputs* inputs) {
  CalculateRenderTargetInternal(inputs->root_layer, inputs->property_trees,
                                true, inputs->can_render_to_separate_surface);
}

void CalculateRenderSurfaceLayerList(
    LayerTreeHostCommon::CalcDrawPropsImplInputs* inputs) {
  const bool subtree_visible_from_ancestor = true;
  DCHECK_EQ(
      inputs->current_render_surface_layer_list_id,
      inputs->root_layer->layer_tree_impl()->current_render_surface_list_id());
  CalculateRenderSurfaceLayerListInternal(
      inputs->root_layer, inputs->property_trees,
      inputs->render_surface_layer_list, nullptr, nullptr,
      subtree_visible_from_ancestor, inputs->can_render_to_separate_surface,
      inputs->current_render_surface_layer_list_id, inputs->max_texture_size);
}

static void ComputeMaskLayerDrawProperties(const LayerImpl* layer,
                                           LayerImpl* mask_layer) {
  DrawProperties& mask_layer_draw_properties = mask_layer->draw_properties();
  mask_layer_draw_properties.visible_layer_rect = gfx::Rect(layer->bounds());
  mask_layer_draw_properties.target_space_transform =
      layer->draw_properties().target_space_transform;
  mask_layer_draw_properties.screen_space_transform =
      layer->draw_properties().screen_space_transform;
  mask_layer_draw_properties.maximum_animation_contents_scale =
      layer->draw_properties().maximum_animation_contents_scale;
  mask_layer_draw_properties.starting_animation_contents_scale =
      layer->draw_properties().starting_animation_contents_scale;
}

void CalculateDrawPropertiesInternal(
    LayerTreeHostCommon::CalcDrawPropsImplInputs* inputs,
    PropertyTreeOption property_tree_option) {
  inputs->render_surface_layer_list->clear();

  UpdateMetaInformationSequenceNumber(inputs->root_layer);
  PreCalculateMetaInformationRecursiveData recursive_data;
  PreCalculateMetaInformationInternal(inputs->root_layer, &recursive_data);
  const bool should_measure_property_tree_performance =
      property_tree_option == BUILD_PROPERTY_TREES_IF_NEEDED;

  LayerImplList visible_layer_list;
  switch (property_tree_option) {
    case BUILD_PROPERTY_TREES_IF_NEEDED: {
      // The translation from layer to property trees is an intermediate
      // state. We will eventually get these data passed directly to the
      // compositor.
      if (should_measure_property_tree_performance) {
        TRACE_EVENT_BEGIN0(
            TRACE_DISABLED_BY_DEFAULT("cc.debug.cdp-perf"),
            "LayerTreeHostCommon::ComputeVisibleRectsWithPropertyTrees");
      }

      BuildPropertyTreesAndComputeVisibleRects(
          inputs->root_layer, inputs->page_scale_layer,
          inputs->inner_viewport_scroll_layer,
          inputs->outer_viewport_scroll_layer,
          inputs->elastic_overscroll_application_layer,
          inputs->elastic_overscroll, inputs->page_scale_factor,
          inputs->device_scale_factor, gfx::Rect(inputs->device_viewport_size),
          inputs->device_transform, inputs->can_render_to_separate_surface,
          inputs->property_trees, &visible_layer_list);

      // Property trees are normally constructed on the main thread and
      // passed to compositor thread. Source to parent updates on them are not
      // allowed in the compositor thread. Some tests build them on the
      // compositor thread, so we need to explicitly disallow source to parent
      // updates when they are built on compositor thread.
      inputs->property_trees->transform_tree
          .set_source_to_parent_updates_allowed(false);
      if (should_measure_property_tree_performance) {
        TRACE_EVENT_END0(
            TRACE_DISABLED_BY_DEFAULT("cc.debug.cdp-perf"),
            "LayerTreeHostCommon::ComputeVisibleRectsWithPropertyTrees");
      }

      break;
    }
    case DONT_BUILD_PROPERTY_TREES: {
      TRACE_EVENT0(
          TRACE_DISABLED_BY_DEFAULT("cc.debug.cdp-perf"),
          "LayerTreeHostCommon::ComputeJustVisibleRectsWithPropertyTrees");
      // Since page scale and elastic overscroll are SyncedProperties, changes
      // on the active tree immediately affect the pending tree, so instead of
      // trying to update property trees whenever these values change, we
      // update property trees before using them.
      UpdatePageScaleFactorInPropertyTrees(
          inputs->property_trees, inputs->page_scale_layer,
          inputs->page_scale_factor, inputs->device_scale_factor,
          inputs->device_transform);
      UpdateElasticOverscrollInPropertyTrees(
          inputs->property_trees, inputs->elastic_overscroll_application_layer,
          inputs->elastic_overscroll);
      // Similarly, the device viewport and device transform are shared
      // by both trees.
      inputs->property_trees->clip_tree.SetViewportClip(
          gfx::RectF(gfx::SizeF(inputs->device_viewport_size)));
      inputs->property_trees->transform_tree.SetDeviceTransform(
          inputs->device_transform, inputs->root_layer->position());
      inputs->property_trees->transform_tree.SetDeviceTransformScaleFactor(
          inputs->device_transform);
      ComputeVisibleRectsUsingPropertyTrees(
          inputs->root_layer, inputs->property_trees,
          inputs->can_render_to_separate_surface, &visible_layer_list);
      break;
    }
  }

  if (should_measure_property_tree_performance) {
    TRACE_EVENT_BEGIN0(TRACE_DISABLED_BY_DEFAULT("cc.debug.cdp-perf"),
                       "LayerTreeHostCommon::CalculateDrawProperties");
  }

  std::vector<AccumulatedSurfaceState> accumulated_surface_state;
  DCHECK(inputs->can_render_to_separate_surface ==
         inputs->property_trees->non_root_surfaces_enabled);
  CalculateRenderTarget(inputs);
  for (LayerImpl* layer : visible_layer_list) {
    ComputeLayerDrawPropertiesUsingPropertyTrees(
        layer, inputs->property_trees, inputs->layers_always_allowed_lcd_text,
        inputs->can_use_lcd_text, &layer->draw_properties());
    if (layer->mask_layer())
      ComputeMaskLayerDrawProperties(layer, layer->mask_layer());
    LayerImpl* replica_mask_layer =
        layer->replica_layer() ? layer->replica_layer()->mask_layer() : nullptr;
    if (replica_mask_layer)
      ComputeMaskLayerDrawProperties(layer, replica_mask_layer);
  }

  CalculateRenderSurfaceLayerList(inputs);

  if (should_measure_property_tree_performance) {
    TRACE_EVENT_END0(TRACE_DISABLED_BY_DEFAULT("cc.debug.cdp-perf"),
                     "LayerTreeHostCommon::CalculateDrawProperties");
  }

  // A root layer render_surface should always exist after
  // CalculateDrawProperties.
  DCHECK(inputs->root_layer->render_surface());
}

void LayerTreeHostCommon::CalculateDrawProperties(
    CalcDrawPropsMainInputs* inputs) {
  LayerList update_layer_list;
  bool can_render_to_separate_surface = true;
  PropertyTrees* property_trees =
      inputs->root_layer->layer_tree_host()->property_trees();
  Layer* overscroll_elasticity_layer = nullptr;
  gfx::Vector2dF elastic_overscroll;
  BuildPropertyTreesAndComputeVisibleRects(
      inputs->root_layer, inputs->page_scale_layer,
      inputs->inner_viewport_scroll_layer, inputs->outer_viewport_scroll_layer,
      overscroll_elasticity_layer, elastic_overscroll,
      inputs->page_scale_factor, inputs->device_scale_factor,
      gfx::Rect(inputs->device_viewport_size), inputs->device_transform,
      can_render_to_separate_surface, property_trees, &update_layer_list);
}

void LayerTreeHostCommon::CalculateDrawProperties(
    CalcDrawPropsImplInputs* inputs) {
  CalculateDrawPropertiesInternal(inputs, DONT_BUILD_PROPERTY_TREES);

  if (CdpPerfTracingEnabled()) {
    LayerTreeImpl* layer_tree_impl = inputs->root_layer->layer_tree_impl();
    if (layer_tree_impl->IsPendingTree() &&
        layer_tree_impl->is_first_frame_after_commit()) {
      LayerImpl* active_tree_root =
          layer_tree_impl->list()->FindActiveLayerById(
              inputs->root_layer->id());
      float jitter = 0.f;
      if (active_tree_root) {
        LayerImpl* last_scrolled_layer = layer_tree_impl->list()->LayerById(
            active_tree_root->layer_tree_impl()->LastScrolledLayerId());
        jitter = CalculateFrameJitter(last_scrolled_layer);
      }
      TRACE_EVENT_ASYNC_BEGIN1(
          "cdp.perf", "jitter",
          inputs->root_layer->layer_tree_impl()->source_frame_number(), "value",
          jitter);
      inputs->root_layer->layer_tree_impl()->set_is_first_frame_after_commit(
          false);
      TRACE_EVENT_ASYNC_END1(
          "cdp.perf", "jitter",
          inputs->root_layer->layer_tree_impl()->source_frame_number(), "value",
          jitter);
    }
  }
}

void LayerTreeHostCommon::CalculateDrawProperties(
    CalcDrawPropsImplInputsForTesting* inputs) {
  CalculateDrawPropertiesInternal(inputs, BUILD_PROPERTY_TREES_IF_NEEDED);
}

PropertyTrees* GetPropertyTrees(Layer* layer) {
  return layer->layer_tree_host()->property_trees();
}

PropertyTrees* GetPropertyTrees(LayerImpl* layer) {
  return layer->layer_tree_impl()->property_trees();
}

}  // namespace cc
