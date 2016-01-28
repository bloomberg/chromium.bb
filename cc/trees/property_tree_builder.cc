// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/property_tree_builder.h"

#include <stddef.h>

#include <map>
#include <set>

#include "cc/base/math_util.h"
#include "cc/layers/layer.h"
#include "cc/layers/layer_impl.h"
#include "cc/trees/draw_property_utils.h"
#include "cc/trees/layer_tree_host.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/vector2d_conversions.h"

namespace cc {

class LayerTreeHost;

namespace {

static const int kInvalidPropertyTreeNodeId = -1;
static const int kRootPropertyTreeNodeId = 0;

template <typename LayerType>
struct DataForRecursion {
  TransformTree* transform_tree;
  ClipTree* clip_tree;
  EffectTree* effect_tree;
  ScrollTree* scroll_tree;
  LayerType* transform_tree_parent;
  LayerType* transform_fixed_parent;
  int render_target;
  int clip_tree_parent;
  int effect_tree_parent;
  int scroll_tree_parent;
  const LayerType* page_scale_layer;
  const LayerType* inner_viewport_scroll_layer;
  const LayerType* outer_viewport_scroll_layer;
  const LayerType* overscroll_elasticity_layer;
  gfx::Vector2dF elastic_overscroll;
  float page_scale_factor;
  bool in_subtree_of_page_scale_layer;
  bool affected_by_inner_viewport_bounds_delta;
  bool affected_by_outer_viewport_bounds_delta;
  bool should_flatten;
  bool target_is_clipped;
  bool is_hidden;
  bool scroll_tree_parent_created_by_uninheritable_criteria;
  const gfx::Transform* device_transform;
  gfx::Vector2dF scroll_compensation_adjustment;
  gfx::Transform compound_transform_since_render_target;
  bool axis_align_since_render_target;
  int sequence_number;
};

template <typename LayerType>
struct DataForRecursionFromChild {
  int num_copy_requests_in_subtree;

  DataForRecursionFromChild() : num_copy_requests_in_subtree(0) {}

  void Merge(const DataForRecursionFromChild& data) {
    num_copy_requests_in_subtree += data.num_copy_requests_in_subtree;
  }
};

template <typename LayerType>
static LayerType* GetTransformParent(const DataForRecursion<LayerType>& data,
                                     LayerType* layer) {
  return layer->position_constraint().is_fixed_position()
             ? data.transform_fixed_parent
             : data.transform_tree_parent;
}

template <typename LayerType>
static ClipNode* GetClipParent(const DataForRecursion<LayerType>& data,
                               LayerType* layer) {
  const bool inherits_clip = !layer->clip_parent();
  const int id = inherits_clip ? data.clip_tree_parent
                               : layer->clip_parent()->clip_tree_index();
  return data.clip_tree->Node(id);
}

template <typename LayerType>
static bool LayerClipsSubtree(LayerType* layer) {
  return layer->masks_to_bounds() || layer->mask_layer();
}

template <typename LayerType>
static int GetScrollParentId(const DataForRecursion<LayerType>& data,
                             LayerType* layer) {
  const bool inherits_scroll = !layer->scroll_parent();
  const int id = inherits_scroll ? data.scroll_tree_parent
                                 : layer->scroll_parent()->scroll_tree_index();
  return id;
}

template <typename LayerType>
void AddClipNodeIfNeeded(const DataForRecursion<LayerType>& data_from_ancestor,
                         LayerType* layer,
                         bool created_render_surface,
                         bool created_transform_node,
                         DataForRecursion<LayerType>* data_for_children) {
  ClipNode* parent = GetClipParent(data_from_ancestor, layer);
  int parent_id = parent->id;

  bool is_root = !layer->parent();

  // Whether we have an ancestor clip that we might need to apply.
  bool ancestor_clips_subtree = is_root || parent->data.layers_are_clipped;

  bool layers_are_clipped = false;
  bool has_unclipped_surface = false;

  if (created_render_surface) {
    // Clips can usually be applied to a surface's descendants simply by
    // clipping the surface (or applied implicitly by the surface's bounds).
    // However, if the surface has unclipped descendants (layers that aren't
    // affected by the ancestor clip), we cannot clip the surface itself, and
    // must instead apply clips to the clipped descendants.
    if (ancestor_clips_subtree && layer->num_unclipped_descendants() > 0) {
      layers_are_clipped = true;
    } else if (!ancestor_clips_subtree) {
      // When there are no ancestor clips that need to be applied to a render
      // surface, we reset clipping state. The surface might contribute a clip
      // of its own, but clips from ancestor nodes don't need to be considered
      // when computing clip rects or visibility.
      has_unclipped_surface = true;
      DCHECK(!parent->data.applies_local_clip);
    }
    // A surface with unclipped descendants cannot be clipped by its ancestor
    // clip at draw time since the unclipped descendants aren't affected by the
    // ancestor clip.
    data_for_children->target_is_clipped =
        ancestor_clips_subtree && !layer->num_unclipped_descendants();
  } else {
    // Without a new render surface, layer clipping state from ancestors needs
    // to continue to propagate.
    data_for_children->target_is_clipped = data_from_ancestor.target_is_clipped;
    layers_are_clipped = ancestor_clips_subtree;
  }

  bool layer_clips_subtree = LayerClipsSubtree(layer);
  if (layer_clips_subtree)
    layers_are_clipped = true;

  // Without surfaces, all non-viewport clips have to be applied using layer
  // clipping.
  bool layers_are_clipped_when_surfaces_disabled =
      layer_clips_subtree ||
      parent->data.layers_are_clipped_when_surfaces_disabled;

  // Render surface's clip is needed during hit testing. So, we need to create
  // a clip node for every render surface.
  bool requires_node = layer_clips_subtree || created_render_surface;

  if (!requires_node) {
    data_for_children->clip_tree_parent = parent_id;
    DCHECK_EQ(layers_are_clipped, parent->data.layers_are_clipped);
    DCHECK_EQ(layers_are_clipped_when_surfaces_disabled,
              parent->data.layers_are_clipped_when_surfaces_disabled);
  } else {
    LayerType* transform_parent = data_for_children->transform_tree_parent;
    if (layer->position_constraint().is_fixed_position() &&
        !created_transform_node) {
      transform_parent = data_for_children->transform_fixed_parent;
    }
    ClipNode node;
    node.data.clip =
        gfx::RectF(gfx::PointF() + layer->offset_to_transform_parent(),
                   gfx::SizeF(layer->bounds()));
    node.data.transform_id = transform_parent->transform_tree_index();
    node.data.target_id =
        data_for_children->effect_tree->Node(data_for_children->render_target)
            ->data.transform_id;
    node.owner_id = layer->id();

    if (ancestor_clips_subtree || layer_clips_subtree) {
      // Surfaces reset the rect used for layer clipping. At other nodes, layer
      // clipping state from ancestors must continue to get propagated.
      node.data.layer_clipping_uses_only_local_clip =
          created_render_surface || !ancestor_clips_subtree;
    } else {
      // Otherwise, we're either unclipped, or exist only in order to apply our
      // parent's clips in our space.
      node.data.layer_clipping_uses_only_local_clip = false;
    }

    node.data.applies_local_clip = layer_clips_subtree;
    node.data.resets_clip = has_unclipped_surface;
    node.data.target_is_clipped = data_for_children->target_is_clipped;
    node.data.layers_are_clipped = layers_are_clipped;
    node.data.layers_are_clipped_when_surfaces_disabled =
        layers_are_clipped_when_surfaces_disabled;

    data_for_children->clip_tree_parent =
        data_for_children->clip_tree->Insert(node, parent_id);
  }

  layer->SetClipTreeIndex(data_for_children->clip_tree_parent);
  // TODO(awoloszyn): Right now when we hit a node with a replica, we reset the
  // clip for all children since we may need to draw. We need to figure out a
  // better way, since we will need both the clipped and unclipped versions.
}

template <typename LayerType>
bool AddTransformNodeIfNeeded(
    const DataForRecursion<LayerType>& data_from_ancestor,
    LayerType* layer,
    bool created_render_surface,
    DataForRecursion<LayerType>* data_for_children) {
  const bool is_root = !layer->parent();
  const bool is_page_scale_layer = layer == data_from_ancestor.page_scale_layer;
  const bool is_overscroll_elasticity_layer =
      layer == data_from_ancestor.overscroll_elasticity_layer;
  const bool is_scrollable = layer->scrollable();
  const bool is_fixed = layer->position_constraint().is_fixed_position();

  const bool has_significant_transform =
      !layer->transform().IsIdentityOr2DTranslation();

  const bool has_potentially_animated_transform =
      layer->HasPotentiallyRunningTransformAnimation();

  // A transform node is needed even for a finished animation, since differences
  // in the timing of animation state updates can mean that an animation that's
  // in the Finished state at tree-building time on the main thread is still in
  // the Running state right after commit on the compositor thread.
  const bool has_any_transform_animation =
      layer->HasAnyAnimationTargetingProperty(Animation::TRANSFORM);

  const bool has_surface = created_render_surface;

  // A transform node is needed to change the render target for subtree when
  // a scroll child's render target is different from the scroll parent's render
  // target.
  const bool scroll_child_has_different_target =
      layer->scroll_parent() &&
      layer->parent()->effect_tree_index() !=
          layer->scroll_parent()->effect_tree_index();

  bool requires_node = is_root || is_scrollable || has_significant_transform ||
                       has_any_transform_animation || has_surface || is_fixed ||
                       is_page_scale_layer || is_overscroll_elasticity_layer ||
                       scroll_child_has_different_target;

  LayerType* transform_parent = GetTransformParent(data_from_ancestor, layer);
  DCHECK(is_root || transform_parent);

  int parent_index = kRootPropertyTreeNodeId;
  if (transform_parent)
    parent_index = transform_parent->transform_tree_index();

  int source_index = parent_index;

  gfx::Vector2dF source_offset;
  if (transform_parent) {
    if (layer->scroll_parent()) {
      LayerType* source = layer->parent();
      source_offset += source->offset_to_transform_parent();
      source_index = source->transform_tree_index();
    } else if (!is_fixed) {
      source_offset = transform_parent->offset_to_transform_parent();
    } else {
      source_offset = data_from_ancestor.transform_tree_parent
                          ->offset_to_transform_parent();
      source_index =
          data_from_ancestor.transform_tree_parent->transform_tree_index();
      source_offset += data_from_ancestor.scroll_compensation_adjustment;
    }
  }

  if (layer->IsContainerForFixedPositionLayers() || is_root) {
    data_for_children->affected_by_inner_viewport_bounds_delta =
        layer == data_from_ancestor.inner_viewport_scroll_layer;
    data_for_children->affected_by_outer_viewport_bounds_delta =
        layer == data_from_ancestor.outer_viewport_scroll_layer;
    if (is_scrollable) {
      DCHECK(!is_root);
      DCHECK(layer->transform().IsIdentity());
      data_for_children->transform_fixed_parent = layer->parent();
    } else {
      data_for_children->transform_fixed_parent = layer;
    }
  }
  data_for_children->transform_tree_parent = layer;

  if (layer->IsContainerForFixedPositionLayers() || is_fixed)
    data_for_children->scroll_compensation_adjustment = gfx::Vector2dF();

  if (!requires_node) {
    data_for_children->should_flatten |= layer->should_flatten_transform();
    gfx::Vector2dF local_offset = layer->position().OffsetFromOrigin() +
                                  layer->transform().To2dTranslation();
    gfx::Vector2dF source_to_parent;
    if (source_index != parent_index) {
      gfx::Transform to_parent;
      data_from_ancestor.transform_tree->ComputeTransform(
          source_index, parent_index, &to_parent);
      source_to_parent = to_parent.To2dTranslation();
    }
    layer->set_offset_to_transform_parent(source_offset + source_to_parent +
                                          local_offset);
    layer->set_should_flatten_transform_from_property_tree(
        data_from_ancestor.should_flatten);
    layer->SetTransformTreeIndex(parent_index);
    if (layer->mask_layer())
      layer->mask_layer()->SetTransformTreeIndex(parent_index);
    return false;
  }

  data_for_children->transform_tree->Insert(TransformNode(), parent_index);

  TransformNode* node = data_for_children->transform_tree->back();
  layer->SetTransformTreeIndex(node->id);
  if (layer->mask_layer())
    layer->mask_layer()->SetTransformTreeIndex(node->id);

  node->data.scrolls = is_scrollable;
  node->data.flattens_inherited_transform = data_for_children->should_flatten;

  if (layer == data_from_ancestor.page_scale_layer)
    data_for_children->in_subtree_of_page_scale_layer = true;
  node->data.in_subtree_of_page_scale_layer =
      data_for_children->in_subtree_of_page_scale_layer;

  // Surfaces inherently flatten transforms.
  data_for_children->should_flatten =
      layer->should_flatten_transform() || has_surface;
  DCHECK_GT(data_from_ancestor.effect_tree->size(), 0u);

  node->data.target_id =
      data_for_children->effect_tree->Node(data_from_ancestor.render_target)
          ->data.transform_id;
  node->data.content_target_id =
      data_for_children->effect_tree->Node(data_for_children->render_target)
          ->data.transform_id;
  DCHECK_NE(node->data.target_id, kInvalidPropertyTreeNodeId);

  node->data.is_animated = has_potentially_animated_transform;
  if (has_potentially_animated_transform) {
    float maximum_animation_target_scale = 0.f;
    if (layer->MaximumTargetScale(&maximum_animation_target_scale)) {
      node->data.local_maximum_animation_target_scale =
          maximum_animation_target_scale;
    }

    float starting_animation_scale = 0.f;
    if (layer->AnimationStartScale(&starting_animation_scale)) {
      node->data.local_starting_animation_scale = starting_animation_scale;
    }

    node->data.has_only_translation_animations =
        layer->HasOnlyTranslationTransforms();
  }

  float post_local_scale_factor = 1.0f;
  if (is_root)
    post_local_scale_factor =
        data_for_children->transform_tree->device_scale_factor();

  if (is_page_scale_layer) {
    post_local_scale_factor *= data_from_ancestor.page_scale_factor;
    data_for_children->transform_tree->set_page_scale_factor(
        data_from_ancestor.page_scale_factor);
  }

  if (has_surface && !is_root)
    node->data.needs_sublayer_scale = true;

  node->data.source_node_id = source_index;
  node->data.post_local_scale_factor = post_local_scale_factor;
  if (is_root) {
    data_for_children->transform_tree->SetDeviceTransform(
        *data_from_ancestor.device_transform, layer->position());
    data_for_children->transform_tree->SetDeviceTransformScaleFactor(
        *data_from_ancestor.device_transform);
  } else {
    node->data.source_offset = source_offset;
    node->data.update_post_local_transform(layer->position(),
                                           layer->transform_origin());
  }

  if (is_overscroll_elasticity_layer) {
    DCHECK(!is_scrollable);
    node->data.scroll_offset =
        gfx::ScrollOffset(data_from_ancestor.elastic_overscroll);
  } else if (!layer->scroll_parent()) {
    node->data.scroll_offset = layer->CurrentScrollOffset();
  }

  if (is_fixed) {
    if (data_from_ancestor.affected_by_inner_viewport_bounds_delta) {
      node->data.affected_by_inner_viewport_bounds_delta_x =
          layer->position_constraint().is_fixed_to_right_edge();
      node->data.affected_by_inner_viewport_bounds_delta_y =
          layer->position_constraint().is_fixed_to_bottom_edge();
      if (node->data.affected_by_inner_viewport_bounds_delta_x ||
          node->data.affected_by_inner_viewport_bounds_delta_y) {
        data_for_children->transform_tree
            ->AddNodeAffectedByInnerViewportBoundsDelta(node->id);
      }
    } else if (data_from_ancestor.affected_by_outer_viewport_bounds_delta) {
      node->data.affected_by_outer_viewport_bounds_delta_x =
          layer->position_constraint().is_fixed_to_right_edge();
      node->data.affected_by_outer_viewport_bounds_delta_y =
          layer->position_constraint().is_fixed_to_bottom_edge();
      if (node->data.affected_by_outer_viewport_bounds_delta_x ||
          node->data.affected_by_outer_viewport_bounds_delta_y) {
        data_for_children->transform_tree
            ->AddNodeAffectedByOuterViewportBoundsDelta(node->id);
      }
    }
  }

  node->data.local = layer->transform();
  node->data.update_pre_local_transform(layer->transform_origin());

  node->data.needs_local_transform_update = true;
  data_from_ancestor.transform_tree->UpdateTransforms(node->id);

  layer->set_offset_to_transform_parent(gfx::Vector2dF());

  // Flattening (if needed) will be handled by |node|.
  layer->set_should_flatten_transform_from_property_tree(false);

  data_for_children->scroll_compensation_adjustment +=
      layer->ScrollDelta() + layer->ScrollCompensationAdjustment() -
      node->data.scroll_snap;

  node->owner_id = layer->id();

  return true;
}

bool IsAnimatingOpacity(Layer* layer) {
  return layer->HasPotentiallyRunningOpacityAnimation() ||
         layer->OpacityCanAnimateOnImplThread();
}

bool IsAnimatingOpacity(LayerImpl* layer) {
  return layer->HasPotentiallyRunningOpacityAnimation();
}

template <typename LayerType>
static inline bool LayerIsInExisting3DRenderingContext(LayerType* layer) {
  return layer->Is3dSorted() && layer->parent() &&
         layer->parent()->Is3dSorted() &&
         (layer->parent()->sorting_context_id() == layer->sorting_context_id());
}

template <typename LayerType>
bool ShouldCreateRenderSurface(LayerType* layer,
                               gfx::Transform current_transform,
                               bool axis_aligned) {
  const bool preserves_2d_axis_alignment =
      (current_transform * layer->transform()).Preserves2dAxisAlignment() &&
      axis_aligned && layer->AnimationsPreserveAxisAlignment();
  const bool is_root = !layer->parent();
  if (is_root)
    return true;

  // If the layer uses a mask and the layer is not a replica layer.
  // TODO(weiliangc): After slimming paint there won't be replica layers.
  if (layer->mask_layer() && layer->parent()->replica_layer() != layer) {
    return true;
  }

  // If the layer has a reflection.
  if (layer->replica_layer()) {
    return true;
  }

  // If the layer uses a CSS filter.
  if (!layer->filters().IsEmpty() || !layer->background_filters().IsEmpty()) {
    return true;
  }

  // If the layer will use a CSS filter.  In this case, the animation
  // will start and add a filter to this layer, so it needs a surface.
  if (layer->HasPotentiallyRunningFilterAnimation()) {
    return true;
  }

  int num_descendants_that_draw_content =
      layer->NumDescendantsThatDrawContent();

  // If the layer flattens its subtree, but it is treated as a 3D object by its
  // parent (i.e. parent participates in a 3D rendering context).
  if (LayerIsInExisting3DRenderingContext(layer) &&
      layer->should_flatten_transform() &&
      num_descendants_that_draw_content > 0) {
    TRACE_EVENT_INSTANT0(
        "cc", "PropertyTreeBuilder::ShouldCreateRenderSurface flattening",
        TRACE_EVENT_SCOPE_THREAD);
    return true;
  }

  // If the layer has blending.
  // TODO(rosca): this is temporary, until blending is implemented for other
  // types of quads than RenderPassDrawQuad. Layers having descendants that draw
  // content will still create a separate rendering surface.
  if (!layer->uses_default_blend_mode()) {
    TRACE_EVENT_INSTANT0(
        "cc", "PropertyTreeBuilder::ShouldCreateRenderSurface blending",
        TRACE_EVENT_SCOPE_THREAD);
    return true;
  }
  // If the layer clips its descendants but it is not axis-aligned with respect
  // to its parent.
  bool layer_clips_external_content =
      LayerClipsSubtree(layer) || layer->HasDelegatedContent();
  if (layer_clips_external_content && !preserves_2d_axis_alignment &&
      num_descendants_that_draw_content > 0) {
    TRACE_EVENT_INSTANT0(
        "cc", "PropertyTreeBuilder::ShouldCreateRenderSurface clipping",
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

  if (layer->EffectiveOpacity() != 1.f && layer->should_flatten_transform() &&
      at_least_two_layers_in_subtree_draw_content) {
    TRACE_EVENT_INSTANT0(
        "cc", "PropertyTreeBuilder::ShouldCreateRenderSurface opacity",
        TRACE_EVENT_SCOPE_THREAD);
    DCHECK(!is_root);
    return true;
  }
  // If the layer has isolation.
  // TODO(rosca): to be optimized - create separate rendering surface only when
  // the blending descendants might have access to the content behind this layer
  // (layer has transparent background or descendants overflow).
  // https://code.google.com/p/chromium/issues/detail?id=301738
  if (layer->is_root_for_isolated_group()) {
    TRACE_EVENT_INSTANT0(
        "cc", "PropertyTreeBuilder::ShouldCreateRenderSurface isolation",
        TRACE_EVENT_SCOPE_THREAD);
    return true;
  }

  // If we force it.
  if (layer->force_render_surface())
    return true;

  // If we'll make a copy of the layer's contents.
  if (layer->HasCopyRequest())
    return true;

  return false;
}

template <typename LayerType>
bool AddEffectNodeIfNeeded(
    const DataForRecursion<LayerType>& data_from_ancestor,
    LayerType* layer,
    DataForRecursion<LayerType>* data_for_children) {
  const bool is_root = !layer->parent();
  const bool has_transparency = layer->EffectiveOpacity() != 1.f;
  const bool has_animated_opacity = IsAnimatingOpacity(layer);
  const bool should_create_render_surface = ShouldCreateRenderSurface(
      layer, data_from_ancestor.compound_transform_since_render_target,
      data_from_ancestor.axis_align_since_render_target);
  data_for_children->axis_align_since_render_target &=
      layer->AnimationsPreserveAxisAlignment();

  bool requires_node = is_root || has_transparency || has_animated_opacity ||
                       should_create_render_surface;

  int parent_id = data_from_ancestor.effect_tree_parent;

  if (!requires_node) {
    layer->SetEffectTreeIndex(parent_id);
    data_for_children->effect_tree_parent = parent_id;
    data_for_children->compound_transform_since_render_target *=
        layer->transform();
    return false;
  }

  EffectNode node;
  node.owner_id = layer->id();
  node.data.opacity = layer->EffectiveOpacity();
  node.data.has_render_surface = should_create_render_surface;
  node.data.has_copy_request = layer->HasCopyRequest();
  node.data.has_background_filters = !layer->background_filters().IsEmpty();

  if (!is_root) {
    // The effect node's transform id is used only when we create a render
    // surface. So, we can leave the default value when we don't create a render
    // surface.
    if (should_create_render_surface) {
      // In this case, we will create a transform node, so it's safe to use the
      // next available id from the transform tree as this effect node's
      // transform id.
      node.data.transform_id =
          data_from_ancestor.transform_tree->next_available_id();
    }
    node.data.clip_id = data_from_ancestor.clip_tree_parent;

    EffectNode* parent = data_from_ancestor.effect_tree->Node(parent_id);
    node.data.screen_space_opacity_is_animating =
        parent->data.screen_space_opacity_is_animating || has_animated_opacity;
  } else {
    // Root render surface acts the unbounded and untransformed to draw content
    // into. Transform node created from root layer (includes device scale
    // factor) and clip node created from root layer (include viewports) applies
    // to root render surface's content, but not root render surface itself.
    node.data.transform_id = kRootPropertyTreeNodeId;
    node.data.clip_id = kRootPropertyTreeNodeId;
    node.data.screen_space_opacity_is_animating = has_animated_opacity;
  }
  data_for_children->effect_tree_parent =
      data_for_children->effect_tree->Insert(node, parent_id);
  layer->SetEffectTreeIndex(data_for_children->effect_tree_parent);
  if (should_create_render_surface) {
    data_for_children->compound_transform_since_render_target =
        gfx::Transform();
    data_for_children->axis_align_since_render_target = true;
  }
  return should_create_render_surface;
}

template <typename LayerType>
void AddScrollNodeIfNeeded(
    const DataForRecursion<LayerType>& data_from_ancestor,
    LayerType* layer,
    DataForRecursion<LayerType>* data_for_children) {
  int parent_id = GetScrollParentId(data_from_ancestor, layer);

  bool is_root = !layer->parent();
  bool scrollable = layer->scrollable();
  bool contains_non_fast_scrollable_region =
      !layer->non_fast_scrollable_region().IsEmpty();
  bool should_scroll_on_main_thread = layer->should_scroll_on_main_thread();

  bool scroll_node_uninheritable_criteria =
      is_root || scrollable || contains_non_fast_scrollable_region;
  bool requires_node =
      scroll_node_uninheritable_criteria ||
      (should_scroll_on_main_thread &&
       data_from_ancestor.scroll_tree_parent_created_by_uninheritable_criteria);

  if (!requires_node) {
    data_for_children->scroll_tree_parent = parent_id;
  } else {
    ScrollNode node;
    node.owner_id = layer->id();
    node.data.scrollable = scrollable;
    node.data.should_scroll_on_main_thread = should_scroll_on_main_thread;
    node.data.contains_non_fast_scrollable_region =
        contains_non_fast_scrollable_region;
    node.data.transform_id =
        data_for_children->transform_tree_parent->transform_tree_index();
    data_for_children->scroll_tree_parent =
        data_for_children->scroll_tree->Insert(node, parent_id);
    data_for_children->scroll_tree_parent_created_by_uninheritable_criteria =
        scroll_node_uninheritable_criteria;
  }

  layer->SetScrollTreeIndex(data_for_children->scroll_tree_parent);
}

template <typename LayerType>
void BuildPropertyTreesInternal(
    LayerType* layer,
    const DataForRecursion<LayerType>& data_from_parent,
    DataForRecursionFromChild<LayerType>* data_to_parent) {
  layer->set_property_tree_sequence_number(data_from_parent.sequence_number);
  if (layer->mask_layer())
    layer->mask_layer()->set_property_tree_sequence_number(
        data_from_parent.sequence_number);

  DataForRecursion<LayerType> data_for_children(data_from_parent);

  bool created_render_surface =
      AddEffectNodeIfNeeded(data_from_parent, layer, &data_for_children);

  if (created_render_surface) {
    data_for_children.render_target = data_for_children.effect_tree_parent;
    layer->set_draw_blend_mode(SkXfermode::kSrcOver_Mode);
  } else {
    layer->set_draw_blend_mode(layer->blend_mode());
  }

  bool created_transform_node = AddTransformNodeIfNeeded(
      data_from_parent, layer, created_render_surface, &data_for_children);
  AddClipNodeIfNeeded(data_from_parent, layer, created_render_surface,
                      created_transform_node, &data_for_children);

  AddScrollNodeIfNeeded(data_from_parent, layer, &data_for_children);

  for (size_t i = 0; i < layer->children().size(); ++i) {
    if (!layer->child_at(i)->scroll_parent()) {
      DataForRecursionFromChild<LayerType> data_from_child;
      BuildPropertyTreesInternal(layer->child_at(i), data_for_children,
                                 &data_from_child);
      data_to_parent->Merge(data_from_child);
    } else {
      // The child should be included in its scroll parent's list of scroll
      // children.
      DCHECK(layer->child_at(i)->scroll_parent()->scroll_children()->count(
          layer->child_at(i)));
    }
  }

  if (layer->scroll_children()) {
    for (LayerType* scroll_child : *layer->scroll_children()) {
      DCHECK_EQ(scroll_child->scroll_parent(), layer);
      DataForRecursionFromChild<LayerType> data_from_child;
      DCHECK(scroll_child->parent());
      data_for_children.render_target =
          scroll_child->parent()->effect_tree_index();
      BuildPropertyTreesInternal(scroll_child, data_for_children,
                                 &data_from_child);
      data_to_parent->Merge(data_from_child);
    }
  }

  if (layer->has_replica()) {
    DataForRecursionFromChild<LayerType> data_from_child;
    BuildPropertyTreesInternal(layer->replica_layer(), data_for_children,
                               &data_from_child);
    data_to_parent->Merge(data_from_child);
  }

  if (layer->HasCopyRequest())
    data_to_parent->num_copy_requests_in_subtree++;

  if (data_for_children.effect_tree->Node(data_for_children.effect_tree_parent)
          ->owner_id == layer->id())
    data_for_children.effect_tree->Node(data_for_children.effect_tree_parent)
        ->data.num_copy_requests_in_subtree =
        data_to_parent->num_copy_requests_in_subtree;
}

}  // namespace

template <typename LayerType>
void BuildPropertyTreesTopLevelInternal(
    LayerType* root_layer,
    const LayerType* page_scale_layer,
    const LayerType* inner_viewport_scroll_layer,
    const LayerType* outer_viewport_scroll_layer,
    const LayerType* overscroll_elasticity_layer,
    const gfx::Vector2dF& elastic_overscroll,
    float page_scale_factor,
    float device_scale_factor,
    const gfx::Rect& viewport,
    const gfx::Transform& device_transform,
    PropertyTrees* property_trees) {
  if (!property_trees->needs_rebuild) {
    UpdatePageScaleFactorInPropertyTrees(property_trees, page_scale_layer,
                                         page_scale_factor, device_scale_factor,
                                         device_transform);
    UpdateElasticOverscrollInPropertyTrees(
        property_trees, overscroll_elasticity_layer, elastic_overscroll);
    property_trees->clip_tree.SetViewportClip(gfx::RectF(viewport));
    property_trees->transform_tree.SetDeviceTransform(device_transform,
                                                      root_layer->position());
    return;
  }

  property_trees->sequence_number++;

  DataForRecursion<LayerType> data_for_recursion;
  data_for_recursion.transform_tree = &property_trees->transform_tree;
  data_for_recursion.clip_tree = &property_trees->clip_tree;
  data_for_recursion.effect_tree = &property_trees->effect_tree;
  data_for_recursion.scroll_tree = &property_trees->scroll_tree;
  data_for_recursion.transform_tree_parent = nullptr;
  data_for_recursion.transform_fixed_parent = nullptr;
  data_for_recursion.render_target = kRootPropertyTreeNodeId;
  data_for_recursion.clip_tree_parent = kRootPropertyTreeNodeId;
  data_for_recursion.effect_tree_parent = kInvalidPropertyTreeNodeId;
  data_for_recursion.scroll_tree_parent = kRootPropertyTreeNodeId;
  data_for_recursion.page_scale_layer = page_scale_layer;
  data_for_recursion.inner_viewport_scroll_layer = inner_viewport_scroll_layer;
  data_for_recursion.outer_viewport_scroll_layer = outer_viewport_scroll_layer;
  data_for_recursion.overscroll_elasticity_layer = overscroll_elasticity_layer;
  data_for_recursion.elastic_overscroll = elastic_overscroll;
  data_for_recursion.page_scale_factor = page_scale_factor;
  data_for_recursion.in_subtree_of_page_scale_layer = false;
  data_for_recursion.affected_by_inner_viewport_bounds_delta = false;
  data_for_recursion.affected_by_outer_viewport_bounds_delta = false;
  data_for_recursion.should_flatten = false;
  data_for_recursion.target_is_clipped = false;
  data_for_recursion.is_hidden = false;
  data_for_recursion.scroll_tree_parent_created_by_uninheritable_criteria =
      true;
  data_for_recursion.device_transform = &device_transform;

  data_for_recursion.transform_tree->clear();
  data_for_recursion.clip_tree->clear();
  data_for_recursion.effect_tree->clear();
  data_for_recursion.scroll_tree->clear();
  data_for_recursion.compound_transform_since_render_target = gfx::Transform();
  data_for_recursion.axis_align_since_render_target = true;
  data_for_recursion.sequence_number = property_trees->sequence_number;
  data_for_recursion.transform_tree->set_device_scale_factor(
      device_scale_factor);

  ClipNode root_clip;
  root_clip.data.resets_clip = true;
  root_clip.data.applies_local_clip = true;
  root_clip.data.clip = gfx::RectF(viewport);
  root_clip.data.transform_id = kRootPropertyTreeNodeId;
  data_for_recursion.clip_tree_parent =
      data_for_recursion.clip_tree->Insert(root_clip, kRootPropertyTreeNodeId);

  DataForRecursionFromChild<LayerType> data_from_child;
  BuildPropertyTreesInternal(root_layer, data_for_recursion, &data_from_child);
  property_trees->needs_rebuild = false;

  // The transform tree is kept up-to-date as it is built, but the
  // combined_clips stored in the clip tree and the screen_space_opacity and
  // is_drawn in the effect tree aren't computed during tree building.
  property_trees->transform_tree.set_needs_update(false);
  property_trees->clip_tree.set_needs_update(true);
  property_trees->effect_tree.set_needs_update(true);
  property_trees->scroll_tree.set_needs_update(false);
}

void PropertyTreeBuilder::BuildPropertyTrees(
    Layer* root_layer,
    const Layer* page_scale_layer,
    const Layer* inner_viewport_scroll_layer,
    const Layer* outer_viewport_scroll_layer,
    const Layer* overscroll_elasticity_layer,
    const gfx::Vector2dF& elastic_overscroll,
    float page_scale_factor,
    float device_scale_factor,
    const gfx::Rect& viewport,
    const gfx::Transform& device_transform,
    PropertyTrees* property_trees) {
  BuildPropertyTreesTopLevelInternal(
      root_layer, page_scale_layer, inner_viewport_scroll_layer,
      outer_viewport_scroll_layer, overscroll_elasticity_layer,
      elastic_overscroll, page_scale_factor, device_scale_factor, viewport,
      device_transform, property_trees);
}

void PropertyTreeBuilder::BuildPropertyTrees(
    LayerImpl* root_layer,
    const LayerImpl* page_scale_layer,
    const LayerImpl* inner_viewport_scroll_layer,
    const LayerImpl* outer_viewport_scroll_layer,
    const LayerImpl* overscroll_elasticity_layer,
    const gfx::Vector2dF& elastic_overscroll,
    float page_scale_factor,
    float device_scale_factor,
    const gfx::Rect& viewport,
    const gfx::Transform& device_transform,
    PropertyTrees* property_trees) {
  BuildPropertyTreesTopLevelInternal(
      root_layer, page_scale_layer, inner_viewport_scroll_layer,
      outer_viewport_scroll_layer, overscroll_elasticity_layer,
      elastic_overscroll, page_scale_factor, device_scale_factor, viewport,
      device_transform, property_trees);
}

}  // namespace cc
