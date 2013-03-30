// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_host_common.h"

#include <algorithm>

#include "base/debug/trace_event.h"
#include "cc/base/math_util.h"
#include "cc/layers/heads_up_display_layer_impl.h"
#include "cc/layers/layer.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/layer_iterator.h"
#include "cc/layers/render_surface.h"
#include "cc/layers/render_surface_impl.h"
#include "cc/trees/layer_sorter.h"
#include "cc/trees/layer_tree_impl.h"
#include "ui/gfx/point_conversions.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/gfx/transform.h"

namespace cc {

ScrollAndScaleSet::ScrollAndScaleSet() {}

ScrollAndScaleSet::~ScrollAndScaleSet() {}

static void SortLayers(LayerList::iterator forst,
                       LayerList::iterator end,
                       void* layer_sorter) {
  NOTREACHED();
}

static void SortLayers(LayerImplList::iterator first,
                       LayerImplList::iterator end,
                       LayerSorter* layer_sorter) {
  DCHECK(layer_sorter);
  TRACE_EVENT0("cc", "LayerTreeHostCommon::SortLayers");
  layer_sorter->Sort(first, end);
}

inline gfx::Rect CalculateVisibleRectWithCachedLayerRect(
    gfx::Rect target_surface_rect,
    gfx::Rect layer_bound_rect,
    gfx::Rect layer_rect_in_target_space,
    const gfx::Transform& transform) {
  // Is this layer fully contained within the target surface?
  if (target_surface_rect.Contains(layer_rect_in_target_space))
    return layer_bound_rect;

  // If the layer doesn't fill up the entire surface, then find the part of
  // the surface rect where the layer could be visible. This avoids trying to
  // project surface rect points that are behind the projection point.
  gfx::Rect minimal_surface_rect = target_surface_rect;
  minimal_surface_rect.Intersect(layer_rect_in_target_space);

  // Project the corners of the target surface rect into the layer space.
  // This bounding rectangle may be larger than it needs to be (being
  // axis-aligned), but is a reasonable filter on the space to consider.
  // Non-invertible transforms will create an empty rect here.

  gfx::Transform surface_to_layer(gfx::Transform::kSkipInitialization);
  if (!transform.GetInverse(&surface_to_layer)) {
    // TODO(shawnsingh): Either we need to handle uninvertible transforms
    // here, or DCHECK that the transform is invertible.
  }
  gfx::Rect layer_rect = gfx::ToEnclosingRect(MathUtil::ProjectClippedRect(
      surface_to_layer, gfx::RectF(minimal_surface_rect)));
  layer_rect.Intersect(layer_bound_rect);
  return layer_rect;
}

gfx::Rect LayerTreeHostCommon::CalculateVisibleRect(
    gfx::Rect target_surface_rect,
    gfx::Rect layer_bound_rect,
    const gfx::Transform& transform) {
  gfx::Rect layer_in_surface_space =
      MathUtil::MapClippedRect(transform, layer_bound_rect);
  return CalculateVisibleRectWithCachedLayerRect(
      target_surface_rect, layer_bound_rect, layer_in_surface_space, transform);
}

template <typename LayerType> static inline bool IsRootLayer(LayerType* layer) {
  return !layer->parent();
}

template <typename LayerType>
static inline bool LayerIsInExisting3DRenderingContext(LayerType* layer) {
  // According to current W3C spec on CSS transforms, a layer is part of an
  // established 3d rendering context if its parent has transform-style of
  // preserves-3d.
  return layer->parent() && layer->parent()->preserves_3d();
}

template <typename LayerType>
static bool IsRootLayerOfNewRenderingContext(LayerType* layer) {
  // According to current W3C spec on CSS transforms (Section 6.1), a layer is
  // the beginning of 3d rendering context if its parent does not have
  // transform-style: preserve-3d, but this layer itself does.
  if (layer->parent())
    return !layer->parent()->preserves_3d() && layer->preserves_3d();

  return layer->preserves_3d();
}

template <typename LayerType>
static bool IsLayerBackFaceVisible(LayerType* layer) {
  // The current W3C spec on CSS transforms says that backface visibility should
  // be determined differently depending on whether the layer is in a "3d
  // rendering context" or not. For Chromium code, we can determine whether we
  // are in a 3d rendering context by checking if the parent preserves 3d.

  if (LayerIsInExisting3DRenderingContext(layer))
    return layer->draw_transform().IsBackFaceVisible();

  // In this case, either the layer establishes a new 3d rendering context, or
  // is not in a 3d rendering context at all.
  return layer->transform().IsBackFaceVisible();
}

template <typename LayerType>
static bool IsSurfaceBackFaceVisible(LayerType* layer,
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

template <typename LayerType>
static gfx::Rect CalculateVisibleContentRect(
    LayerType* layer,
    gfx::Rect ancestor_clip_rect_in_descendant_surface_space,
    gfx::Rect layer_rect_in_target_space) {
  DCHECK(layer->render_target());

  // Nothing is visible if the layer bounds are empty.
  if (!layer->DrawsContent() || layer->content_bounds().IsEmpty() ||
      layer->drawable_content_rect().IsEmpty())
    return gfx::Rect();

  // Compute visible bounds in target surface space.
  gfx::Rect visible_rect_in_target_surface_space =
      layer->drawable_content_rect();

  if (!layer->render_target()->render_surface()->clip_rect().IsEmpty()) {
    // In this case the target surface does clip layers that contribute to
    // it. So, we have to convert the current surface's clip rect from its
    // ancestor surface space to the current (descendant) surface
    // space. This conversion is done outside this function so that it can
    // be cached instead of computing it redundantly for every layer.
    visible_rect_in_target_surface_space.Intersect(
        ancestor_clip_rect_in_descendant_surface_space);
  }

  if (visible_rect_in_target_surface_space.IsEmpty())
    return gfx::Rect();

  return CalculateVisibleRectWithCachedLayerRect(
      visible_rect_in_target_surface_space,
      gfx::Rect(layer->content_bounds()),
      layer_rect_in_target_space,
      layer->draw_transform());
}

static inline bool TransformToParentIsKnown(LayerImpl* layer) { return true; }

static inline bool TransformToParentIsKnown(Layer* layer) {
  return !layer->TransformIsAnimating();
}

static inline bool TransformToScreenIsKnown(LayerImpl* layer) { return true; }

static inline bool TransformToScreenIsKnown(Layer* layer) {
  return !layer->screen_space_transform_is_animating();
}

template <typename LayerType>
static bool LayerShouldBeSkipped(LayerType* layer) {
  // Layers can be skipped if any of these conditions are met.
  //   - does not draw content.
  //   - is transparent
  //   - has empty bounds
  //   - the layer is not double-sided, but its back face is visible.
  //
  // Some additional conditions need to be computed at a later point after the
  // recursion is finished.
  //   - the intersection of render_surface content and layer clip_rect is empty
  //   - the visible_content_rect is empty
  //
  // Note, if the layer should not have been drawn due to being fully
  // transparent, we would have skipped the entire subtree and never made it
  // into this function, so it is safe to omit this check here.

  if (!layer->DrawsContent() || layer->bounds().IsEmpty())
    return true;

  LayerType* backface_test_layer = layer;
  if (layer->use_parent_backface_visibility()) {
    DCHECK(layer->parent());
    DCHECK(!layer->parent()->use_parent_backface_visibility());
    backface_test_layer = layer->parent();
  }

  // The layer should not be drawn if (1) it is not double-sided and (2) the
  // back of the layer is known to be facing the screen.
  if (!backface_test_layer->double_sided() &&
      TransformToScreenIsKnown(backface_test_layer) &&
      IsLayerBackFaceVisible(backface_test_layer))
    return true;

  return false;
}

static inline bool SubtreeShouldBeSkipped(LayerImpl* layer) {
  // If layer is on the pending tree and opacity is being animated then
  // this subtree can't be skipped as we need to create, prioritize and
  // include tiles for this layer when deciding if tree can be activated.
  if (layer->layer_tree_impl()->IsPendingTree() && layer->OpacityIsAnimating())
    return false;

  // The opacity of a layer always applies to its children (either implicitly
  // via a render surface or explicitly if the parent preserves 3D), so the
  // entire subtree can be skipped if this layer is fully transparent.
  return !layer->opacity();
}

static inline bool SubtreeShouldBeSkipped(Layer* layer) {
  // If the opacity is being animated then the opacity on the main thread is
  // unreliable (since the impl thread may be using a different opacity), so it
  // should not be trusted.
  // In particular, it should not cause the subtree to be skipped.
  // Similarly, for layers that might animate opacity using an impl-only
  // animation, their subtree should also not be skipped.
  return !layer->opacity() && !layer->OpacityIsAnimating() &&
         !layer->OpacityCanAnimateOnImplThread();
}

// Called on each layer that could be drawn after all information from
// CalcDrawProperties has been updated on that layer.  May have some false
// positives (e.g. layers get this called on them but don't actually get drawn).
static inline void UpdateTilePrioritiesForLayer(LayerImpl* layer) {
  layer->UpdateTilePriorities();

  // Mask layers don't get this call, so explicitly update them so they can
  // kick off tile rasterization.
  if (layer->mask_layer())
    layer->mask_layer()->UpdateTilePriorities();
  if (layer->replica_layer() && layer->replica_layer()->mask_layer())
    layer->replica_layer()->mask_layer()->UpdateTilePriorities();
}

static inline void UpdateTilePrioritiesForLayer(Layer* layer) {}

template <typename LayerType>
static bool SubtreeShouldRenderToSeparateSurface(
    LayerType* layer,
    bool axis_aligned_with_respect_to_parent) {
  //
  // A layer and its descendants should render onto a new RenderSurfaceImpl if
  // any of these rules hold:
  //

  // The root layer should always have a render_surface.
  if (IsRootLayer(layer))
    return true;

  // If we force it.
  if (layer->force_render_surface())
    return true;

  // If the layer uses a mask.
  if (layer->mask_layer())
    return true;

  // If the layer has a reflection.
  if (layer->replica_layer())
    return true;

  // If the layer uses a CSS filter.
  if (!layer->filters().isEmpty() || !layer->background_filters().isEmpty() ||
      layer->filter())
    return true;

  int num_descendants_that_draw_content =
      layer->draw_properties().num_descendants_that_draw_content;

  // If the layer flattens its subtree (i.e. the layer doesn't preserve-3d), but
  // it is treated as a 3D object by its parent (i.e. parent does preserve-3d).
  if (LayerIsInExisting3DRenderingContext(layer) && !layer->preserves_3d() &&
      num_descendants_that_draw_content > 0) {
    TRACE_EVENT_INSTANT0(
        "cc",
        "LayerTreeHostCommon::SubtreeShouldRenderToSeparateSurface flattening",
        TRACE_EVENT_SCOPE_THREAD);
    return true;
  }

  // If the layer clips its descendants but it is not axis-aligned with respect
  // to its parent.
  bool layer_clips_external_content =
      LayerClipsSubtree(layer) || layer->HasDelegatedContent();
  if (layer_clips_external_content && !axis_aligned_with_respect_to_parent &&
      !layer->draw_properties().descendants_can_clip_selves) {
    TRACE_EVENT_INSTANT0(
        "cc",
        "LayerTreeHostCommon::SubtreeShouldRenderToSeparateSurface clipping",
        TRACE_EVENT_SCOPE_THREAD);
    return true;
  }

  // If the layer has some translucency and does not have a preserves-3d
  // transform style.  This condition only needs a render surface if two or more
  // layers in the subtree overlap. But checking layer overlaps is unnecessarily
  // costly so instead we conservatively create a surface whenever at least two
  // layers draw content for this subtree.
  bool at_least_two_layers_in_subtree_draw_content =
      num_descendants_that_draw_content > 0 &&
      (layer->DrawsContent() || num_descendants_that_draw_content > 1);

  if (layer->opacity() != 1.f && !layer->preserves_3d() &&
      at_least_two_layers_in_subtree_draw_content) {
    TRACE_EVENT_INSTANT0(
        "cc",
        "LayerTreeHostCommon::SubtreeShouldRenderToSeparateSurface opacity",
        TRACE_EVENT_SCOPE_THREAD);
    return true;
  }

  return false;
}

gfx::Transform ComputeScrollCompensationForThisLayer(
    LayerImpl* scrolling_layer,
    const gfx::Transform& parent_matrix) {
  // For every layer that has non-zero scroll_delta, we have to compute a
  // transform that can undo the scroll_delta translation. In particular, we
  // want this matrix to premultiply a fixed-position layer's parent_matrix, so
  // we design this transform in three steps as follows. The steps described
  // here apply from right-to-left, so Step 1 would be the right-most matrix:
  //
  //     Step 1. transform from target surface space to the exact space where
  //           scroll_delta is actually applied.
  //           -- this is inverse of the matrix in step 3
  //     Step 2. undo the scroll_delta
  //           -- this is just a translation by scroll_delta.
  //     Step 3. transform back to target surface space.
  //           -- this transform is the "partial_layer_origin_transform" =
  //           (parent_matrix * scale(layer->pageScaleDelta()));
  //
  // These steps create a matrix that both start and end in target surface
  // space. So this matrix can pre-multiply any fixed-position layer's
  // draw_transform to undo the scroll_deltas -- as long as that fixed position
  // layer is fixed onto the same render_target as this scrolling_layer.
  //

  gfx::Transform partial_layer_origin_transform = parent_matrix;
  partial_layer_origin_transform.PreconcatTransform(
      scrolling_layer->impl_transform());

  gfx::Transform scroll_compensation_for_this_layer =
      partial_layer_origin_transform;        // Step 3
  scroll_compensation_for_this_layer.Translate(
      scrolling_layer->scroll_delta().x(),
      scrolling_layer->scroll_delta().y());  // Step 2

  gfx::Transform inverse_partial_layer_origin_transform(
      gfx::Transform::kSkipInitialization);
  if (!partial_layer_origin_transform.GetInverse(
          &inverse_partial_layer_origin_transform)) {
    // TODO(shawnsingh): Either we need to handle uninvertible transforms
    // here, or DCHECK that the transform is invertible.
  }
  scroll_compensation_for_this_layer.PreconcatTransform(
      inverse_partial_layer_origin_transform);  // Step 1
  return scroll_compensation_for_this_layer;
}

gfx::Transform ComputeScrollCompensationMatrixForChildren(
    Layer* current_layer,
    const gfx::Transform& current_parent_matrix,
    const gfx::Transform& current_scroll_compensation) {
  // The main thread (i.e. Layer) does not need to worry about scroll
  // compensation.  So we can just return an identity matrix here.
  return gfx::Transform();
}

gfx::Transform ComputeScrollCompensationMatrixForChildren(
    LayerImpl* layer,
    const gfx::Transform& parent_matrix,
    const gfx::Transform& current_scroll_compensation_matrix) {
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

  // Avoid the overheads (including stack allocation and matrix
  // initialization/copy) if we know that the scroll compensation doesn't need
  // to be reset or adjusted.
  if (!layer->is_container_for_fixed_position_layers() &&
      layer->scroll_delta().IsZero() && !layer->render_surface())
    return current_scroll_compensation_matrix;

  // Start as identity matrix.
  gfx::Transform next_scroll_compensation_matrix;

  // If this layer is not a container, then it inherits the existing scroll
  // compensations.
  if (!layer->is_container_for_fixed_position_layers())
    next_scroll_compensation_matrix = current_scroll_compensation_matrix;

  // If the current layer has a non-zero scroll_delta, then we should compute
  // its local scroll compensation and accumulate it to the
  // next_scroll_compensation_matrix.
  if (!layer->scroll_delta().IsZero()) {
    gfx::Transform scroll_compensation_for_this_layer =
        ComputeScrollCompensationForThisLayer(layer, parent_matrix);
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

template <typename LayerType>
static inline void CalculateContentsScale(LayerType* layer,
                                          float contents_scale,
                                          bool animating_transform_to_screen) {
  layer->CalculateContentsScale(contents_scale,
                                animating_transform_to_screen,
                                &layer->draw_properties().contents_scale_x,
                                &layer->draw_properties().contents_scale_y,
                                &layer->draw_properties().content_bounds);

  LayerType* mask_layer = layer->mask_layer();
  if (mask_layer) {
    mask_layer->CalculateContentsScale(
        contents_scale,
        animating_transform_to_screen,
        &mask_layer->draw_properties().contents_scale_x,
        &mask_layer->draw_properties().contents_scale_y,
        &mask_layer->draw_properties().content_bounds);
  }

  LayerType* replica_mask_layer =
      layer->replica_layer() ? layer->replica_layer()->mask_layer() : NULL;
  if (replica_mask_layer) {
    replica_mask_layer->CalculateContentsScale(
        contents_scale,
        animating_transform_to_screen,
        &replica_mask_layer->draw_properties().contents_scale_x,
        &replica_mask_layer->draw_properties().contents_scale_y,
        &replica_mask_layer->draw_properties().content_bounds);
  }
}

static inline void UpdateLayerContentsScale(
    LayerImpl* layer,
    const gfx::Transform& combined_transform,
    float device_scale_factor,
    float page_scale_factor,
    bool animating_transform_to_screen) {
  gfx::Vector2dF transform_scale = MathUtil::ComputeTransform2dScaleComponents(
      combined_transform, device_scale_factor * page_scale_factor);
  float contents_scale = std::max(transform_scale.x(), transform_scale.y());
  CalculateContentsScale(layer, contents_scale, animating_transform_to_screen);
}

static inline void UpdateLayerContentsScale(
    Layer* layer,
    const gfx::Transform& combined_transform,
    float device_scale_factor,
    float page_scale_factor,
    bool animating_transform_to_screen) {
  float raster_scale = layer->raster_scale();

  if (layer->automatically_compute_raster_scale()) {
    gfx::Vector2dF transform_scale =
        MathUtil::ComputeTransform2dScaleComponents(combined_transform, 0.f);
    float combined_scale = std::max(transform_scale.x(), transform_scale.y());
    float ideal_raster_scale = combined_scale / device_scale_factor;
    if (!layer->bounds_contain_page_scale())
      ideal_raster_scale /= page_scale_factor;

    bool need_to_set_raster_scale = !raster_scale;

    // If we've previously saved a raster_scale but the ideal changes, things
    // are unpredictable and we should just use 1.
    if (raster_scale && raster_scale != 1.f &&
        ideal_raster_scale != raster_scale) {
      ideal_raster_scale = 1.f;
      need_to_set_raster_scale = true;
    }

    if (need_to_set_raster_scale) {
      bool use_and_save_ideal_scale =
          ideal_raster_scale >= 1.f && !animating_transform_to_screen;
      if (use_and_save_ideal_scale) {
        raster_scale = ideal_raster_scale;
        layer->SetRasterScale(raster_scale);
      }
    }
  }

  if (!raster_scale)
    raster_scale = 1.f;

  float contents_scale = raster_scale * device_scale_factor;
  if (!layer->bounds_contain_page_scale())
    contents_scale *= page_scale_factor;

  CalculateContentsScale(layer, contents_scale, animating_transform_to_screen);
}

template <typename LayerType, typename LayerList>
static inline void RemoveSurfaceForEarlyExit(
    LayerType* layer_to_remove,
    LayerList* render_surface_layer_list) {
  DCHECK(layer_to_remove->render_surface());
  // Technically, we know that the layer we want to remove should be
  // at the back of the render_surface_layer_list. However, we have had
  // bugs before that added unnecessary layers here
  // (https://bugs.webkit.org/show_bug.cgi?id=74147), but that causes
  // things to crash. So here we proactively remove any additional
  // layers from the end of the list.
  while (render_surface_layer_list->back() != layer_to_remove) {
    render_surface_layer_list->back()->ClearRenderSurface();
    render_surface_layer_list->pop_back();
  }
  DCHECK_EQ(render_surface_layer_list->back(), layer_to_remove);
  render_surface_layer_list->pop_back();
  layer_to_remove->ClearRenderSurface();
}

// Recursively walks the layer tree to compute any information that is needed
// before doing the main recursion.
template <typename LayerType>
static void PreCalculateMetaInformation(LayerType* layer) {
  if (layer->HasDelegatedContent()) {
    // Layers with delegated content need to be treated as if they have as many
    // children as the number of layers they own delegated quads for. Since we
    // don't know this number right now, we choose one that acts like infinity
    // for our purposes.
    layer->draw_properties().num_descendants_that_draw_content = 1000;
    layer->draw_properties().descendants_can_clip_selves = false;
    return;
  }

  int num_descendants_that_draw_content = 0;
  bool descendants_can_clip_selves = true;
  bool sublayer_transform_prevents_clip =
      !layer->sublayer_transform().IsPositiveScaleOrTranslation();

  for (size_t i = 0; i < layer->children().size(); ++i) {
    LayerType* child_layer = layer->children()[i];
    PreCalculateMetaInformation<LayerType>(child_layer);

    num_descendants_that_draw_content += child_layer->DrawsContent() ? 1 : 0;
    num_descendants_that_draw_content +=
        child_layer->draw_properties().num_descendants_that_draw_content;

    if ((child_layer->DrawsContent() && !child_layer->CanClipSelf()) ||
        !child_layer->draw_properties().descendants_can_clip_selves ||
        sublayer_transform_prevents_clip ||
        !child_layer->transform().IsPositiveScaleOrTranslation())
      descendants_can_clip_selves = false;
  }

  layer->draw_properties().num_descendants_that_draw_content =
      num_descendants_that_draw_content;
  layer->draw_properties().descendants_can_clip_selves =
      descendants_can_clip_selves;
}

static void RoundTranslationComponents(gfx::Transform* transform) {
  transform->matrix().
      setDouble(0, 3, MathUtil::Round(transform->matrix().getDouble(0, 3)));
  transform->matrix().
      setDouble(1, 3, MathUtil::Round(transform->matrix().getDouble(1, 3)));
}

// Recursively walks the layer tree starting at the given node and computes all
// the necessary transformations, clip rects, render surfaces, etc.
template <typename LayerType, typename LayerList, typename RenderSurfaceType>
static void CalculateDrawPropertiesInternal(
    LayerType* layer,
    const gfx::Transform& parent_matrix,
    const gfx::Transform& full_hierarchy_matrix,
    const gfx::Transform& current_scroll_compensation_matrix,
    gfx::Rect clip_rect_from_ancestor,
    gfx::Rect clip_rect_from_ancestor_in_descendant_space,
    bool ancestor_clips_subtree,
    RenderSurfaceType* nearest_ancestor_that_moves_pixels,
    LayerList* render_surface_layer_list,
    LayerList* layer_list,
    LayerSorter* layer_sorter,
    int max_texture_size,
    float device_scale_factor,
    float page_scale_factor,
    bool subtree_can_use_lcd_text,
    gfx::Rect* drawable_content_rect_of_subtree,
    bool update_tile_priorities) {
  // This function computes the new matrix transformations recursively for this
  // layer and all its descendants. It also computes the appropriate render
  // surfaces.
  // Some important points to remember:
  //
  // 0. Here, transforms are notated in Matrix x Vector order, and in words we
  // describe what the transform does from left to right.
  //
  // 1. In our terminology, the "layer origin" refers to the top-left corner of
  // a layer, and the positive Y-axis points downwards. This interpretation is
  // valid because the orthographic projection applied at draw time flips the Y
  // axis appropriately.
  //
  // 2. The anchor point, when given as a PointF object, is specified in "unit
  // layer space", where the bounds of the layer map to [0, 1]. However, as a
  // Transform object, the transform to the anchor point is specified in "layer
  // space", where the bounds of the layer map to [bounds.width(),
  // bounds.height()].
  //
  // 3. Definition of various transforms used:
  //        M[parent] is the parent matrix, with respect to the nearest render
  //        surface, passed down recursively.
  //
  //        M[root] is the full hierarchy, with respect to the root, passed down
  //        recursively.
  //
  //        Tr[origin] is the translation matrix from the parent's origin to
  //        this layer's origin.
  //
  //        Tr[origin2anchor] is the translation from the layer's origin to its
  //        anchor point
  //
  //        Tr[origin2center] is the translation from the layer's origin to its
  //        center
  //
  //        M[layer] is the layer's matrix (applied at the anchor point)
  //
  //        M[sublayer] is the layer's sublayer transform (also applied at the
  //        layer's anchor point)
  //
  //        S[layer2content] is the ratio of a layer's content_bounds() to its
  //        Bounds().
  //
  //    Some composite transforms can help in understanding the sequence of
  //    transforms:
  //        composite_layer_transform = Tr[origin2anchor] * M[layer] *
  //        Tr[origin2anchor].inverse()
  //
  //        composite_sublayer_transform = Tr[origin2anchor] * M[sublayer] *
  //        Tr[origin2anchor].inverse()
  //
  // 4. When a layer (or render surface) is drawn, it is drawn into a "target
  // render surface". Therefore the draw transform does not necessarily
  // transform from screen space to local layer space. Instead, the draw
  // transform is the transform between the "target render surface space" and
  // local layer space. Note that render surfaces, except for the root, also
  // draw themselves into a different target render surface, and so their draw
  // transform and origin transforms are also described with respect to the
  // target.
  //
  // Using these definitions, then:
  //
  // The draw transform for the layer is:
  //        M[draw] = M[parent] * Tr[origin] * composite_layer_transform *
  //            S[layer2content] = M[parent] * Tr[layer->position() + anchor] *
  //            M[layer] * Tr[anchor2origin] * S[layer2content]
  //
  //        Interpreting the math left-to-right, this transforms from the
  //        layer's render surface to the origin of the layer in content space.
  //
  // The screen space transform is:
  //        M[screenspace] = M[root] * Tr[origin] * composite_layer_transform *
  //            S[layer2content]
  //                       = M[root] * Tr[layer->position() + anchor] * M[layer]
  //                           * Tr[anchor2origin] * S[layer2content]
  //
  //        Interpreting the math left-to-right, this transforms from the root
  //        render surface's content space to the origin of the layer in content
  //        space.
  //
  // The transform hierarchy that is passed on to children (i.e. the child's
  // parent_matrix) is:
  //        M[parent]_for_child = M[parent] * Tr[origin] *
  //            composite_layer_transform * composite_sublayer_transform
  //                            = M[parent] * Tr[layer->position() + anchor] *
  //                              M[layer] * Tr[anchor2origin] *
  //                              composite_sublayer_transform
  //
  //        and a similar matrix for the full hierarchy with respect to the
  //        root.
  //
  // Finally, note that the final matrix used by the shader for the layer is P *
  // M[draw] * S . This final product is computed in drawTexturedQuad(), where:
  //        P is the projection matrix
  //        S is the scale adjustment (to scale up a canonical quad to the
  //            layer's size)
  //
  // When a render surface has a replica layer, that layer's transform is used
  // to draw a second copy of the surface.  gfx::Transforms named here are
  // relative to the surface, unless they specify they are relative to the
  // replica layer.
  //
  // We will denote a scale by device scale S[deviceScale]
  //
  // The render surface draw transform to its target surface origin is:
  //        M[surfaceDraw] = M[owningLayer->Draw]
  //
  // The render surface origin transform to its the root (screen space) origin
  // is:
  //        M[surface2root] =  M[owningLayer->screenspace] *
  //            S[deviceScale].inverse()
  //
  // The replica draw transform to its target surface origin is:
  //        M[replicaDraw] = S[deviceScale] * M[surfaceDraw] *
  //            Tr[replica->position() + replica->anchor()] * Tr[replica] *
  //            Tr[origin2anchor].inverse() * S[contents_scale].inverse()
  //
  // The replica draw transform to the root (screen space) origin is:
  //        M[replica2root] = M[surface2root] * Tr[replica->position()] *
  //            Tr[replica] * Tr[origin2anchor].inverse()
  //

  // If we early-exit anywhere in this function, the drawable_content_rect of
  // this subtree should be considered empty.
  *drawable_content_rect_of_subtree = gfx::Rect();

  // The root layer cannot skip CalcDrawProperties.
  if (!IsRootLayer(layer) && SubtreeShouldBeSkipped(layer))
    return;

  // As this function proceeds, these are the properties for the current
  // layer that actually get computed. To avoid unnecessary copies
  // (particularly for matrices), we do computations directly on these values
  // when possible.
  DrawProperties<LayerType, RenderSurfaceType>& layer_draw_properties =
      layer->draw_properties();

  gfx::Rect clip_rect_for_subtree;
  bool subtree_should_be_clipped = false;

  // This value is cached on the stack so that we don't have to inverse-project
  // the surface's clip rect redundantly for every layer. This value is the
  // same as the surface's clip rect, except that instead of being described
  // in the target surface space (i.e. the ancestor surface space), it is
  // described in the current surface space.
  gfx::Rect clip_rect_for_subtree_in_descendant_space;

  float accumulated_draw_opacity = layer->opacity();
  bool animating_opacity_to_target = layer->OpacityIsAnimating();
  bool animating_opacity_to_screen = animating_opacity_to_target;
  if (layer->parent()) {
    accumulated_draw_opacity *= layer->parent()->draw_opacity();
    animating_opacity_to_target |= layer->parent()->draw_opacity_is_animating();
    animating_opacity_to_screen |=
        layer->parent()->screen_space_opacity_is_animating();
  }

  bool animating_transform_to_target = layer->TransformIsAnimating();
  bool animating_transform_to_screen = animating_transform_to_target;
  if (layer->parent()) {
    animating_transform_to_target |=
        layer->parent()->draw_transform_is_animating();
    animating_transform_to_screen |=
        layer->parent()->screen_space_transform_is_animating();
  }

  gfx::Size bounds = layer->bounds();
  gfx::PointF anchor_point = layer->anchor_point();
  gfx::PointF position = layer->position() - layer->scroll_delta();

  gfx::Transform combined_transform = parent_matrix;
  if (!layer->transform().IsIdentity()) {
    // LT = Tr[origin] * Tr[origin2anchor]
    combined_transform.Translate3d(
        position.x() + anchor_point.x() * bounds.width(),
        position.y() + anchor_point.y() * bounds.height(),
        layer->anchor_point_z());
    // LT = Tr[origin] * Tr[origin2anchor] * M[layer]
    combined_transform.PreconcatTransform(layer->transform());
    // LT = Tr[origin] * Tr[origin2anchor] * M[layer] * Tr[anchor2origin]
    combined_transform.Translate3d(-anchor_point.x() * bounds.width(),
                                   -anchor_point.y() * bounds.height(),
                                   -layer->anchor_point_z());
  } else {
    combined_transform.Translate(position.x(), position.y());
  }

  // The layer's contents_scale is determined from the combined_transform, which
  // then informs the layer's draw_transform.
  UpdateLayerContentsScale(layer,
                           combined_transform,
                           device_scale_factor,
                           page_scale_factor,
                           animating_transform_to_screen);

  // If there is a transformation from the impl thread then it should be at
  // the start of the combined_transform, but we don't want it to affect the
  // computation of contents_scale above.
  // Note carefully: this is Concat, not Preconcat (impl_transform *
  // combined_transform).
  combined_transform.ConcatTransform(layer->impl_transform());

  if (!animating_transform_to_target && layer->scrollable() &&
      combined_transform.IsScaleOrTranslation()) {
    // Align the scrollable layer's position to screen space pixels to avoid
    // blurriness.  To avoid side-effects, do this only if the transform is
    // simple.
    RoundTranslationComponents(&combined_transform);
  }

  if (layer->fixed_to_container_layer()) {
    // Special case: this layer is a composited fixed-position layer; we need to
    // explicitly compensate for all ancestors' nonzero scroll_deltas to keep
    // this layer fixed correctly.
    // Note carefully: this is Concat, not Preconcat
    // (current_scroll_compensation * combined_transform).
    combined_transform.ConcatTransform(current_scroll_compensation_matrix);
  }

  // The draw_transform that gets computed below is effectively the layer's
  // draw_transform, unless the layer itself creates a render_surface. In that
  // case, the render_surface re-parents the transforms.
  layer_draw_properties.target_space_transform = combined_transform;
  // M[draw] = M[parent] * LT * S[layer2content]
  layer_draw_properties.target_space_transform.Scale
      (1.f / layer->contents_scale_x(), 1.f / layer->contents_scale_y());

  // The layer's screen_space_transform represents the transform between root
  // layer's "screen space" and local content space.
  layer_draw_properties.screen_space_transform = full_hierarchy_matrix;
  if (!layer->preserves_3d())
    layer_draw_properties.screen_space_transform.FlattenTo2d();
  layer_draw_properties.screen_space_transform.PreconcatTransform
      (layer_draw_properties.target_space_transform);

  // Adjusting text AA method during animation may cause repaints, which in-turn
  // causes jank.
  bool adjust_text_aa =
      !animating_opacity_to_screen && !animating_transform_to_screen;
  // To avoid color fringing, LCD text should only be used on opaque layers with
  // just integral translation.
  bool layer_can_use_lcd_text =
      subtree_can_use_lcd_text && (accumulated_draw_opacity == 1.f) &&
      layer_draw_properties.target_space_transform.
          IsIdentityOrIntegerTranslation();

  gfx::RectF content_rect(layer->content_bounds());

  // full_hierarchy_matrix is the matrix that transforms objects between screen
  // space (except projection matrix) and the most recent RenderSurfaceImpl's
  // space.  next_hierarchy_matrix will only change if this layer uses a new
  // RenderSurfaceImpl, otherwise remains the same.
  gfx::Transform next_hierarchy_matrix = full_hierarchy_matrix;
  gfx::Transform sublayer_matrix;

  gfx::Vector2dF render_surface_sublayer_scale =
      MathUtil::ComputeTransform2dScaleComponents(
          combined_transform, device_scale_factor * page_scale_factor);

  if (SubtreeShouldRenderToSeparateSurface(
          layer, combined_transform.IsScaleOrTranslation())) {
    // Check back-face visibility before continuing with this surface and its
    // subtree
    if (!layer->double_sided() && TransformToParentIsKnown(layer) &&
        IsSurfaceBackFaceVisible(layer, combined_transform))
      return;

    if (!layer->render_surface())
      layer->CreateRenderSurface();

    RenderSurfaceType* render_surface = layer->render_surface();
    render_surface->ClearLayerLists();

    // The owning layer's draw transform has a scale from content to layer
    // space which we do not want; so here we use the combined_transform
    // instead of the draw_transform. However, we do need to add a different
    // scale factor that accounts for the surface's pixel dimensions.
    combined_transform.Scale(1.0 / render_surface_sublayer_scale.x(),
                             1.0 / render_surface_sublayer_scale.y());
    render_surface->SetDrawTransform(combined_transform);

    // The owning layer's transform was re-parented by the surface, so the
    // layer's new draw_transform only needs to scale the layer to surface
    // space.
    layer_draw_properties.target_space_transform.MakeIdentity();
    layer_draw_properties.target_space_transform.
        Scale(render_surface_sublayer_scale.x() / layer->contents_scale_x(),
              render_surface_sublayer_scale.y() / layer->contents_scale_y());

    // Inside the surface's subtree, we scale everything to the owning layer's
    // scale.  The sublayer matrix transforms layer rects into target surface
    // content space.
    DCHECK(sublayer_matrix.IsIdentity());
    sublayer_matrix.Scale(render_surface_sublayer_scale.x(),
                          render_surface_sublayer_scale.y());

    // The opacity value is moved from the layer to its surface, so that the
    // entire subtree properly inherits opacity.
    render_surface->SetDrawOpacity(accumulated_draw_opacity);
    render_surface->SetDrawOpacityIsAnimating(animating_opacity_to_target);
    animating_opacity_to_target = false;
    layer_draw_properties.opacity = 1.f;
    layer_draw_properties.opacity_is_animating = animating_opacity_to_target;
    layer_draw_properties.screen_space_opacity_is_animating =
        animating_opacity_to_screen;

    render_surface->SetTargetSurfaceTransformsAreAnimating(
        animating_transform_to_target);
    render_surface->SetScreenSpaceTransformsAreAnimating(
        animating_transform_to_screen);
    animating_transform_to_target = false;
    layer_draw_properties.target_space_transform_is_animating =
        animating_transform_to_target;
    layer_draw_properties.screen_space_transform_is_animating =
        animating_transform_to_screen;

    // Update the aggregate hierarchy matrix to include the transform of the
    // newly created RenderSurfaceImpl.
    next_hierarchy_matrix.PreconcatTransform(render_surface->draw_transform());

    // The new render_surface here will correctly clip the entire subtree. So,
    // we do not need to continue propagating the clipping state further down
    // the tree. This way, we can avoid transforming clip rects from ancestor
    // target surface space to current target surface space that could cause
    // more w < 0 headaches.
    subtree_should_be_clipped = false;

    if (layer->mask_layer()) {
      DrawProperties<LayerType, RenderSurfaceType>& mask_layer_draw_properties =
          layer->mask_layer()->draw_properties();
      mask_layer_draw_properties.render_target = layer;
      mask_layer_draw_properties.visible_content_rect =
          gfx::Rect(layer->content_bounds());
    }

    if (layer->replica_layer() && layer->replica_layer()->mask_layer()) {
      DrawProperties<LayerType, RenderSurfaceType>&
      replica_mask_draw_properties =
          layer->replica_layer()->mask_layer()->draw_properties();
      replica_mask_draw_properties.render_target = layer;
      replica_mask_draw_properties.visible_content_rect =
          gfx::Rect(layer->content_bounds());
    }

    // TODO(senorblanco): make this smarter for the SkImageFilter case (check
    // for pixel-moving filters)
    if (layer->filters().hasFilterThatMovesPixels() || layer->filter())
      nearest_ancestor_that_moves_pixels = render_surface;

    // The render surface clip rect is expressed in the space where this surface
    // draws, i.e. the same space as clip_rect_from_ancestor.
    render_surface->SetIsClipped(ancestor_clips_subtree);
    if (ancestor_clips_subtree) {
      render_surface->SetClipRect(clip_rect_from_ancestor);

      gfx::Transform inverse_surface_draw_transform(
          gfx::Transform::kSkipInitialization);
      if (!render_surface->draw_transform().GetInverse(
              &inverse_surface_draw_transform)) {
        // TODO(shawnsingh): Either we need to handle uninvertible transforms
        // here, or DCHECK that the transform is invertible.
      }
      clip_rect_for_subtree_in_descendant_space =
          gfx::ToEnclosingRect(MathUtil::ProjectClippedRect(
              inverse_surface_draw_transform, render_surface->clip_rect()));
    } else {
      render_surface->SetClipRect(gfx::Rect());
      clip_rect_for_subtree_in_descendant_space =
          clip_rect_from_ancestor_in_descendant_space;
    }

    render_surface->SetNearestAncestorThatMovesPixels(
        nearest_ancestor_that_moves_pixels);

    // If the new render surface is drawn translucent or with a non-integral
    // translation then the subtree that gets drawn on this render surface
    // cannot use LCD text.
    subtree_can_use_lcd_text = layer_can_use_lcd_text;

    render_surface_layer_list->push_back(layer);
  } else {
    DCHECK(layer->parent());

    // Note: layer_draw_properties.target_space_transform is computed above,
    // before this if-else statement.
    layer_draw_properties.target_space_transform_is_animating =
        animating_transform_to_target;
    layer_draw_properties.screen_space_transform_is_animating =
        animating_transform_to_screen;
    layer_draw_properties.opacity = accumulated_draw_opacity;
    layer_draw_properties.opacity_is_animating = animating_opacity_to_target;
    layer_draw_properties.screen_space_opacity_is_animating =
        animating_opacity_to_screen;
    sublayer_matrix = combined_transform;

    layer->ClearRenderSurface();

    // Layers without render_surfaces directly inherit the ancestor's clip
    // status.
    subtree_should_be_clipped = ancestor_clips_subtree;
    if (ancestor_clips_subtree)
      clip_rect_for_subtree = clip_rect_from_ancestor;

    // The surface's cached clip rect value propagates regardless of what
    // clipping goes on between layers here.
    clip_rect_for_subtree_in_descendant_space =
        clip_rect_from_ancestor_in_descendant_space;

    // Layers that are not their own render_target will render into the target
    // of their nearest ancestor.
    layer_draw_properties.render_target = layer->parent()->render_target();
  }

  if (adjust_text_aa)
    layer_draw_properties.can_use_lcd_text = layer_can_use_lcd_text;

  gfx::Rect rect_in_target_space = ToEnclosingRect(
      MathUtil::MapClippedRect(layer->draw_transform(), content_rect));

  if (LayerClipsSubtree(layer)) {
    subtree_should_be_clipped = true;
    if (ancestor_clips_subtree && !layer->render_surface()) {
      clip_rect_for_subtree = clip_rect_from_ancestor;
      clip_rect_for_subtree.Intersect(rect_in_target_space);
    } else {
      clip_rect_for_subtree = rect_in_target_space;
    }
  }

  // Flatten to 2D if the layer doesn't preserve 3D.
  if (!layer->preserves_3d())
    sublayer_matrix.FlattenTo2d();

  // Apply the sublayer transform at the anchor point of the layer.
  if (!layer->sublayer_transform().IsIdentity()) {
    sublayer_matrix.Translate(layer->anchor_point().x() * bounds.width(),
                              layer->anchor_point().y() * bounds.height());
    sublayer_matrix.PreconcatTransform(layer->sublayer_transform());
    sublayer_matrix.Translate(-layer->anchor_point().x() * bounds.width(),
                              -layer->anchor_point().y() * bounds.height());
  }

  LayerList& descendants =
      (layer->render_surface() ? layer->render_surface()->layer_list()
                               : *layer_list);

  // Any layers that are appended after this point are in the layer's subtree
  // and should be included in the sorting process.
  size_t sorting_start_index = descendants.size();

  if (!LayerShouldBeSkipped(layer))
    descendants.push_back(layer);

  gfx::Transform next_scroll_compensation_matrix =
      ComputeScrollCompensationMatrixForChildren(
          layer, parent_matrix, current_scroll_compensation_matrix);

  gfx::Rect accumulated_drawable_content_rect_of_children;
  for (size_t i = 0; i < layer->children().size(); ++i) {
    LayerType* child =
        LayerTreeHostCommon::get_child_as_raw_ptr(layer->children(), i);
    gfx::Rect drawable_content_rect_of_child_subtree;
    CalculateDrawPropertiesInternal<LayerType, LayerList, RenderSurfaceType>(
        child,
        sublayer_matrix,
        next_hierarchy_matrix,
        next_scroll_compensation_matrix,
        clip_rect_for_subtree,
        clip_rect_for_subtree_in_descendant_space,
        subtree_should_be_clipped,
        nearest_ancestor_that_moves_pixels,
        render_surface_layer_list,
        &descendants,
        layer_sorter,
        max_texture_size,
        device_scale_factor,
        page_scale_factor,
        subtree_can_use_lcd_text,
        &drawable_content_rect_of_child_subtree,
        update_tile_priorities);
    if (!drawable_content_rect_of_child_subtree.IsEmpty()) {
      accumulated_drawable_content_rect_of_children.Union(
          drawable_content_rect_of_child_subtree);
      if (child->render_surface())
        descendants.push_back(child);
    }
  }

  if (layer->render_surface() && !IsRootLayer(layer) &&
      layer->render_surface()->layer_list().empty()) {
    RemoveSurfaceForEarlyExit(layer, render_surface_layer_list);
    return;
  }

  // Compute the total drawable_content_rect for this subtree (the rect is in
  // target surface space).
  gfx::Rect local_drawable_content_rect_of_subtree =
      accumulated_drawable_content_rect_of_children;
  if (layer->DrawsContent())
    local_drawable_content_rect_of_subtree.Union(rect_in_target_space);
  if (subtree_should_be_clipped)
    local_drawable_content_rect_of_subtree.Intersect(clip_rect_for_subtree);

  // Compute the layer's drawable content rect (the rect is in target surface
  // space).
  layer_draw_properties.drawable_content_rect = rect_in_target_space;
  if (subtree_should_be_clipped) {
    layer_draw_properties.drawable_content_rect.
        Intersect(clip_rect_for_subtree);
  }

  // Tell the layer the rect that is clipped by. In theory we could use a
  // tighter clip rect here (drawable_content_rect), but that actually does not
  // reduce how much would be drawn, and instead it would create unnecessary
  // changes to scissor state affecting GPU performance.
  layer_draw_properties.is_clipped = subtree_should_be_clipped;
  if (subtree_should_be_clipped) {
    layer_draw_properties.clip_rect = clip_rect_for_subtree;
  } else {
    // Initialize the clip rect to a safe value that will not clip the
    // layer, just in case clipping is still accidentally used.
    layer_draw_properties.clip_rect = rect_in_target_space;
  }

  // Compute the layer's visible content rect (the rect is in content space)
  layer_draw_properties.visible_content_rect = CalculateVisibleContentRect(
      layer, clip_rect_for_subtree_in_descendant_space, rect_in_target_space);

  // Compute the remaining properties for the render surface, if the layer has
  // one.
  if (IsRootLayer(layer)) {
    // The root layer's surface's content_rect is always the entire viewport.
    DCHECK(layer->render_surface());
    layer->render_surface()->SetContentRect(clip_rect_from_ancestor);
  } else if (layer->render_surface() && !IsRootLayer(layer)) {
    RenderSurfaceType* render_surface = layer->render_surface();
    gfx::Rect clipped_content_rect = local_drawable_content_rect_of_subtree;

    // Don't clip if the layer is reflected as the reflection shouldn't be
    // clipped. If the layer is animating, then the surface's transform to
    // its target is not known on the main thread, and we should not use it
    // to clip.
    if (!layer->replica_layer() && TransformToParentIsKnown(layer)) {
      // Note, it is correct to use ancestor_clips_subtree here, because we are
      // looking at this layer's render_surface, not the layer itself.
      if (ancestor_clips_subtree && !clipped_content_rect.IsEmpty()) {
        gfx::Rect surface_clip_rect = LayerTreeHostCommon::CalculateVisibleRect(
            render_surface->clip_rect(),
            clipped_content_rect,
            render_surface->draw_transform());
        clipped_content_rect.Intersect(surface_clip_rect);
      }
    }

    // The RenderSurfaceImpl backing texture cannot exceed the maximum supported
    // texture size.
    clipped_content_rect.set_width(
        std::min(clipped_content_rect.width(), max_texture_size));
    clipped_content_rect.set_height(
        std::min(clipped_content_rect.height(), max_texture_size));

    if (clipped_content_rect.IsEmpty()) {
      render_surface->ClearLayerLists();
      RemoveSurfaceForEarlyExit(layer, render_surface_layer_list);
      return;
    }

    render_surface->SetContentRect(clipped_content_rect);

    // The owning layer's screen_space_transform has a scale from content to
    // layer space which we need to undo and replace with a scale from the
    // surface's subtree into layer space.
    gfx::Transform screen_space_transform = layer->screen_space_transform();
    screen_space_transform.Scale(
        layer->contents_scale_x() / render_surface_sublayer_scale.x(),
        layer->contents_scale_y() / render_surface_sublayer_scale.y());
    render_surface->SetScreenSpaceTransform(screen_space_transform);

    if (layer->replica_layer()) {
      gfx::Transform surface_origin_to_replica_origin_transform;
      surface_origin_to_replica_origin_transform.Scale(
          render_surface_sublayer_scale.x(), render_surface_sublayer_scale.y());
      surface_origin_to_replica_origin_transform.Translate(
          layer->replica_layer()->position().x() +
          layer->replica_layer()->anchor_point().x() * bounds.width(),
          layer->replica_layer()->position().y() +
          layer->replica_layer()->anchor_point().y() * bounds.height());
      surface_origin_to_replica_origin_transform.PreconcatTransform(
          layer->replica_layer()->transform());
      surface_origin_to_replica_origin_transform.Translate(
          -layer->replica_layer()->anchor_point().x() * bounds.width(),
          -layer->replica_layer()->anchor_point().y() * bounds.height());
      surface_origin_to_replica_origin_transform.Scale(
          1.0 / render_surface_sublayer_scale.x(),
          1.0 / render_surface_sublayer_scale.y());

      // Compute the replica's "originTransform" that maps from the replica's
      // origin space to the target surface origin space.
      gfx::Transform replica_origin_transform =
          layer->render_surface()->draw_transform() *
          surface_origin_to_replica_origin_transform;
      render_surface->SetReplicaDrawTransform(replica_origin_transform);

      // Compute the replica's "screen_space_transform" that maps from the
      // replica's origin space to the screen's origin space.
      gfx::Transform replica_screen_space_transform =
          layer->render_surface()->screen_space_transform() *
          surface_origin_to_replica_origin_transform;
      render_surface->SetReplicaScreenSpaceTransform(
          replica_screen_space_transform);
    }
  }

  if (update_tile_priorities)
    UpdateTilePrioritiesForLayer(layer);

  // If neither this layer nor any of its children were added, early out.
  if (sorting_start_index == descendants.size())
    return;

  // If preserves-3d then sort all the descendants in 3D so that they can be
  // drawn from back to front. If the preserves-3d property is also set on the
  // parent then skip the sorting as the parent will sort all the descendants
  // anyway.
  if (layer_sorter && descendants.size() && layer->preserves_3d() &&
      (!layer->parent() || !layer->parent()->preserves_3d())) {
    SortLayers(descendants.begin() + sorting_start_index,
               descendants.end(),
               layer_sorter);
  }

  if (layer->render_surface()) {
    *drawable_content_rect_of_subtree =
        gfx::ToEnclosingRect(layer->render_surface()->DrawableContentRect());
  } else {
    *drawable_content_rect_of_subtree = local_drawable_content_rect_of_subtree;
  }

  if (layer->HasContributingDelegatedRenderPasses()) {
    layer->render_target()->render_surface()->
        AddContributingDelegatedRenderPassLayer(layer);
  }
}

void LayerTreeHostCommon::CalculateDrawProperties(
    Layer* root_layer,
    gfx::Size device_viewport_size,
    float device_scale_factor,
    float page_scale_factor,
    int max_texture_size,
    bool can_use_lcd_text,
    LayerList* render_surface_layer_list) {
  gfx::Rect total_drawable_content_rect;
  gfx::Transform identity_matrix;
  gfx::Transform device_scale_transform;
  device_scale_transform.Scale(device_scale_factor, device_scale_factor);
  LayerList dummy_layer_list;

  // The root layer's render_surface should receive the device viewport as the
  // initial clip rect.
  bool subtree_should_be_clipped = true;
  gfx::Rect device_viewport_rect(device_viewport_size);
  bool update_tile_priorities = false;

  // This function should have received a root layer.
  DCHECK(IsRootLayer(root_layer));

  PreCalculateMetaInformation<Layer>(root_layer);
  CalculateDrawPropertiesInternal<Layer,
                                  LayerList,
                                  RenderSurface>(root_layer,
                                                 device_scale_transform,
                                                 identity_matrix,
                                                 identity_matrix,
                                                 device_viewport_rect,
                                                 device_viewport_rect,
                                                 subtree_should_be_clipped,
                                                 NULL,
                                                 render_surface_layer_list,
                                                 &dummy_layer_list,
                                                 NULL,
                                                 max_texture_size,
                                                 device_scale_factor,
                                                 page_scale_factor,
                                                 can_use_lcd_text,
                                                 &total_drawable_content_rect,
                                                 update_tile_priorities);

  // The dummy layer list should not have been used.
  DCHECK_EQ(0u, dummy_layer_list.size());
  // A root layer render_surface should always exist after
  // CalculateDrawProperties.
  DCHECK(root_layer->render_surface());
}

void LayerTreeHostCommon::CalculateDrawProperties(
    LayerImpl* root_layer,
    gfx::Size device_viewport_size,
    float device_scale_factor,
    float page_scale_factor,
    int max_texture_size,
    bool can_use_lcd_text,
    LayerImplList* render_surface_layer_list,
    bool update_tile_priorities) {
  gfx::Rect total_drawable_content_rect;
  gfx::Transform identity_matrix;
  gfx::Transform device_scale_transform;
  device_scale_transform.Scale(device_scale_factor, device_scale_factor);
  LayerImplList dummy_layer_list;
  LayerSorter layer_sorter;

  // The root layer's render_surface should receive the device viewport as the
  // initial clip rect.
  bool subtree_should_be_clipped = true;
  gfx::Rect device_viewport_rect(device_viewport_size);

  // This function should have received a root layer.
  DCHECK(IsRootLayer(root_layer));

  PreCalculateMetaInformation<LayerImpl>(root_layer);
  CalculateDrawPropertiesInternal<LayerImpl,
                                  LayerImplList,
                                  RenderSurfaceImpl>(
      root_layer,
      device_scale_transform,
      identity_matrix,
      identity_matrix,
      device_viewport_rect,
      device_viewport_rect,
      subtree_should_be_clipped,
      NULL,
      render_surface_layer_list,
      &dummy_layer_list,
      &layer_sorter,
      max_texture_size,
      device_scale_factor,
      page_scale_factor,
      can_use_lcd_text,
      &total_drawable_content_rect,
      update_tile_priorities);

  // The dummy layer list should not have been used.
  DCHECK_EQ(0u, dummy_layer_list.size());
  // A root layer render_surface should always exist after
  // CalculateDrawProperties.
  DCHECK(root_layer->render_surface());
}

static bool PointHitsRect(
    gfx::PointF screen_space_point,
    const gfx::Transform& local_space_to_screen_space_transform,
    gfx::RectF local_space_rect) {
  // If the transform is not invertible, then assume that this point doesn't hit
  // this rect.
  gfx::Transform inverse_local_space_to_screen_space(
      gfx::Transform::kSkipInitialization);
  if (!local_space_to_screen_space_transform.GetInverse(
          &inverse_local_space_to_screen_space))
    return false;

  // Transform the hit test point from screen space to the local space of the
  // given rect.
  bool clipped = false;
  gfx::PointF hit_test_point_in_local_space = MathUtil::ProjectPoint(
      inverse_local_space_to_screen_space, screen_space_point, &clipped);

  // If ProjectPoint could not project to a valid value, then we assume that
  // this point doesn't hit this rect.
  if (clipped)
    return false;

  return local_space_rect.Contains(hit_test_point_in_local_space);
}

static bool PointHitsRegion(gfx::PointF screen_space_point,
                            const gfx::Transform& screen_space_transform,
                            const Region& layer_space_region,
                            float layer_content_scale_x,
                            float layer_content_scale_y) {
  // If the transform is not invertible, then assume that this point doesn't hit
  // this region.
  gfx::Transform inverse_screen_space_transform(
      gfx::Transform::kSkipInitialization);
  if (!screen_space_transform.GetInverse(&inverse_screen_space_transform))
    return false;

  // Transform the hit test point from screen space to the local space of the
  // given region.
  bool clipped = false;
  gfx::PointF hit_test_point_in_content_space = MathUtil::ProjectPoint(
      inverse_screen_space_transform, screen_space_point, &clipped);
  gfx::PointF hit_test_point_in_layer_space =
      gfx::ScalePoint(hit_test_point_in_content_space,
                      1.f / layer_content_scale_x,
                      1.f / layer_content_scale_y);

  // If ProjectPoint could not project to a valid value, then we assume that
  // this point doesn't hit this region.
  if (clipped)
    return false;

  return layer_space_region.Contains(
      gfx::ToRoundedPoint(hit_test_point_in_layer_space));
}

static bool PointIsClippedBySurfaceOrClipRect(gfx::PointF screen_space_point,
                                              LayerImpl* layer) {
  LayerImpl* current_layer = layer;

  // Walk up the layer tree and hit-test any render_surfaces and any layer
  // clip rects that are active.
  while (current_layer) {
    if (current_layer->render_surface() &&
        !PointHitsRect(
            screen_space_point,
            current_layer->render_surface()->screen_space_transform(),
            current_layer->render_surface()->content_rect()))
      return true;

    // Note that drawable content rects are actually in target surface space, so
    // the transform we have to provide is the target surface's
    // screen_space_transform.
    LayerImpl* render_target = current_layer->render_target();
    if (LayerClipsSubtree(current_layer) &&
        !PointHitsRect(
            screen_space_point,
            render_target->render_surface()->screen_space_transform(),
            current_layer->drawable_content_rect()))
      return true;

    current_layer = current_layer->parent();
  }

  // If we have finished walking all ancestors without having already exited,
  // then the point is not clipped by any ancestors.
  return false;
}

LayerImpl* LayerTreeHostCommon::FindLayerThatIsHitByPoint(
    gfx::PointF screen_space_point,
    const LayerImplList& render_surface_layer_list) {
  LayerImpl* found_layer = NULL;

  typedef LayerIterator<LayerImpl,
                        LayerImplList,
                        RenderSurfaceImpl,
                        LayerIteratorActions::FrontToBack> LayerIteratorType;
  LayerIteratorType end = LayerIteratorType::End(&render_surface_layer_list);

  for (LayerIteratorType
           it = LayerIteratorType::Begin(&render_surface_layer_list);
       it != end;
       ++it) {
    // We don't want to consider render_surfaces for hit testing.
    if (!it.represents_itself())
      continue;

    LayerImpl* current_layer = (*it);

    gfx::RectF content_rect(current_layer->content_bounds());
    if (!PointHitsRect(screen_space_point,
                       current_layer->screen_space_transform(),
                       content_rect))
      continue;

    // At this point, we think the point does hit the layer, but we need to walk
    // up the parents to ensure that the layer was not clipped in such a way
    // that the hit point actually should not hit the layer.
    if (PointIsClippedBySurfaceOrClipRect(screen_space_point, current_layer))
      continue;

    // Skip the HUD layer.
    if (current_layer == current_layer->layer_tree_impl()->hud_layer())
      continue;

    found_layer = current_layer;
    break;
  }

  // This can potentially return NULL, which means the screen_space_point did
  // not successfully hit test any layers, not even the root layer.
  return found_layer;
}

LayerImpl* LayerTreeHostCommon::FindLayerThatIsHitByPointInTouchHandlerRegion(
    gfx::PointF screen_space_point,
    const LayerImplList& render_surface_layer_list) {
  LayerImpl* found_layer = NULL;

  typedef LayerIterator<LayerImpl,
                        LayerImplList,
                        RenderSurfaceImpl,
                        LayerIteratorActions::FrontToBack> LayerIteratorType;
  LayerIteratorType end = LayerIteratorType::End(&render_surface_layer_list);

  for (LayerIteratorType
           it = LayerIteratorType::Begin(&render_surface_layer_list);
       it != end;
       ++it) {
    // We don't want to consider render_surfaces for hit testing.
    if (!it.represents_itself())
      continue;

    LayerImpl* current_layer = (*it);

    if (!LayerHasTouchEventHandlersAt(screen_space_point, current_layer))
      continue;

    found_layer = current_layer;
    break;
  }

  // This can potentially return NULL, which means the screen_space_point did
  // not successfully hit test any layers, not even the root layer.
  return found_layer;
}

bool LayerTreeHostCommon::LayerHasTouchEventHandlersAt(
    gfx::PointF screen_space_point,
    LayerImpl* layer_impl) {
  if (layer_impl->touch_event_handler_region().IsEmpty())
    return false;

  if (!PointHitsRegion(screen_space_point,
                       layer_impl->screen_space_transform(),
                       layer_impl->touch_event_handler_region(),
                       layer_impl->contents_scale_x(),
                       layer_impl->contents_scale_y()))
    return false;

  // At this point, we think the point does hit the touch event handler region
  // on the layer, but we need to walk up the parents to ensure that the layer
  // was not clipped in such a way that the hit point actually should not hit
  // the layer.
  if (PointIsClippedBySurfaceOrClipRect(screen_space_point, layer_impl))
    return false;

  return true;
}
}  // namespace cc
