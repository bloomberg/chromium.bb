// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/property_tree_builder.h"

#include <stddef.h>

#include <map>
#include <set>

#include "base/memory/ptr_util.h"
#include "cc/base/math_util.h"
#include "cc/layers/layer.h"
#include "cc/layers/layer_impl.h"
#include "cc/output/copy_output_request.h"
#include "cc/trees/clip_node.h"
#include "cc/trees/draw_property_utils.h"
#include "cc/trees/effect_node.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/layer_tree_settings.h"
#include "cc/trees/mutable_properties.h"
#include "cc/trees/mutator_host.h"
#include "cc/trees/scroll_node.h"
#include "cc/trees/transform_node.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/vector2d_conversions.h"

namespace cc {

namespace {

template <typename LayerType>
struct DataForRecursion {
  PropertyTrees* property_trees;
  LayerType* transform_tree_parent;
  LayerType* transform_fixed_parent;
  int clip_tree_parent;
  int effect_tree_parent;
  int scroll_tree_parent;
  int closest_ancestor_with_copy_request;
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
  bool is_hidden;
  uint32_t main_thread_scrolling_reasons;
  bool scroll_tree_parent_created_by_uninheritable_criteria;
  const gfx::Transform* device_transform;
  gfx::Transform compound_transform_since_render_target;
  bool animation_axis_aligned_since_render_target;
  bool not_axis_aligned_since_last_clip;
  SkColor safe_opaque_background_color;
};

static LayerPositionConstraint PositionConstraint(Layer* layer) {
  return layer->position_constraint();
}

static LayerPositionConstraint PositionConstraint(LayerImpl* layer) {
  return layer->test_properties()->position_constraint;
}

static LayerStickyPositionConstraint StickyPositionConstraint(Layer* layer) {
  return layer->sticky_position_constraint();
}

static LayerStickyPositionConstraint StickyPositionConstraint(
    LayerImpl* layer) {
  return layer->test_properties()->sticky_position_constraint;
}

static LayerImplList& Children(LayerImpl* layer) {
  return layer->test_properties()->children;
}

static const LayerList& Children(Layer* layer) {
  return layer->children();
}

static LayerImpl* ChildAt(LayerImpl* layer, int index) {
  return layer->test_properties()->children[index];
}

static Layer* ChildAt(Layer* layer, int index) {
  return layer->child_at(index);
}

static Layer* ScrollParent(Layer* layer) {
  return layer->scroll_parent();
}

static LayerImpl* ScrollParent(LayerImpl* layer) {
  return layer->test_properties()->scroll_parent;
}

static std::set<Layer*>* ScrollChildren(Layer* layer) {
  return layer->scroll_children();
}

static std::set<LayerImpl*>* ScrollChildren(LayerImpl* layer) {
  return layer->test_properties()->scroll_children.get();
}

static Layer* ClipParent(Layer* layer) {
  return layer->clip_parent();
}

static LayerImpl* ClipParent(LayerImpl* layer) {
  return layer->test_properties()->clip_parent;
}

static inline const FilterOperations& Filters(Layer* layer) {
  return layer->filters();
}

static inline const FilterOperations& Filters(LayerImpl* layer) {
  return layer->test_properties()->filters;
}

static Layer* MaskLayer(Layer* layer) {
  return layer->mask_layer();
}

static LayerImpl* MaskLayer(LayerImpl* layer) {
  return layer->test_properties()->mask_layer;
}

static const gfx::Transform& Transform(Layer* layer) {
  return layer->transform();
}

static const gfx::Transform& Transform(LayerImpl* layer) {
  return layer->test_properties()->transform;
}

static void SetIsScrollClipLayer(Layer* layer) {
  layer->set_is_scroll_clip_layer();
}

static void SetIsScrollClipLayer(LayerImpl* layer) {}

// Methods to query state from the AnimationHost ----------------------
template <typename LayerType>
bool OpacityIsAnimating(LayerType* layer) {
  return layer->GetMutatorHost()->IsAnimatingOpacityProperty(
      layer->element_id(), layer->GetElementTypeForAnimation());
}

template <typename LayerType>
bool HasPotentiallyRunningOpacityAnimation(LayerType* layer) {
  return layer->GetMutatorHost()->HasPotentiallyRunningOpacityAnimation(
      layer->element_id(), layer->GetElementTypeForAnimation());
}

template <typename LayerType>
bool FilterIsAnimating(LayerType* layer) {
  return layer->GetMutatorHost()->IsAnimatingFilterProperty(
      layer->element_id(), layer->GetElementTypeForAnimation());
}

template <typename LayerType>
bool HasPotentiallyRunningFilterAnimation(LayerType* layer) {
  return layer->GetMutatorHost()->HasPotentiallyRunningFilterAnimation(
      layer->element_id(), layer->GetElementTypeForAnimation());
}

template <typename LayerType>
bool TransformIsAnimating(LayerType* layer) {
  return layer->GetMutatorHost()->IsAnimatingTransformProperty(
      layer->element_id(), layer->GetElementTypeForAnimation());
}

template <typename LayerType>
bool HasPotentiallyRunningTransformAnimation(LayerType* layer) {
  return layer->GetMutatorHost()->HasPotentiallyRunningTransformAnimation(
      layer->element_id(), layer->GetElementTypeForAnimation());
}

template <typename LayerType>
bool HasOnlyTranslationTransforms(LayerType* layer) {
  return layer->GetMutatorHost()->HasOnlyTranslationTransforms(
      layer->element_id(), layer->GetElementTypeForAnimation());
}

template <typename LayerType>
bool AnimationsPreserveAxisAlignment(LayerType* layer) {
  return layer->GetMutatorHost()->AnimationsPreserveAxisAlignment(
      layer->element_id());
}

template <typename LayerType>
bool HasAnyAnimationTargetingProperty(LayerType* layer,
                                      TargetProperty::Type property) {
  return layer->GetMutatorHost()->HasAnyAnimationTargetingProperty(
      layer->element_id(), property);
}

// -------------------------------------------------------------------

template <typename LayerType>
static LayerType* GetTransformParent(const DataForRecursion<LayerType>& data,
                                     LayerType* layer) {
  return PositionConstraint(layer).is_fixed_position()
             ? data.transform_fixed_parent
             : data.transform_tree_parent;
}

template <typename LayerType>
static bool LayerClipsSubtree(LayerType* layer) {
  return layer->masks_to_bounds() || MaskLayer(layer);
}

template <typename LayerType>
static int GetScrollParentId(const DataForRecursion<LayerType>& data,
                             LayerType* layer) {
  const bool inherits_scroll = !ScrollParent(layer);
  const int id = inherits_scroll ? data.scroll_tree_parent
                                 : ScrollParent(layer)->scroll_tree_index();
  return id;
}

static Layer* Parent(Layer* layer) {
  return layer->parent();
}

static LayerImpl* Parent(LayerImpl* layer) {
  return layer->test_properties()->parent;
}

static inline int SortingContextId(Layer* layer) {
  return layer->sorting_context_id();
}

static inline int SortingContextId(LayerImpl* layer) {
  return layer->test_properties()->sorting_context_id;
}

static inline bool Is3dSorted(Layer* layer) {
  return layer->Is3dSorted();
}

static inline bool Is3dSorted(LayerImpl* layer) {
  return layer->test_properties()->sorting_context_id != 0;
}

template <typename LayerType>
void AddClipNodeIfNeeded(const DataForRecursion<LayerType>& data_from_ancestor,
                         LayerType* layer,
                         bool created_transform_node,
                         DataForRecursion<LayerType>* data_for_children) {
  const bool inherits_clip = !ClipParent(layer);
  const int parent_id = inherits_clip ? data_from_ancestor.clip_tree_parent
                                      : ClipParent(layer)->clip_tree_index();

  bool layer_clips_subtree = LayerClipsSubtree(layer);
  bool requires_node =
      layer_clips_subtree || Filters(layer).HasFilterThatMovesPixels();
  if (!requires_node) {
    data_for_children->clip_tree_parent = parent_id;
  } else {
    LayerType* transform_parent = data_for_children->transform_tree_parent;
    if (PositionConstraint(layer).is_fixed_position() &&
        !created_transform_node) {
      transform_parent = data_for_children->transform_fixed_parent;
    }
    ClipNode node;
    node.clip = gfx::RectF(gfx::PointF() + layer->offset_to_transform_parent(),
                           gfx::SizeF(layer->bounds()));
    node.transform_id = transform_parent->transform_tree_index();
    node.owning_layer_id = layer->id();
    if (layer_clips_subtree) {
      node.clip_type = ClipNode::ClipType::APPLIES_LOCAL_CLIP;
    } else {
      DCHECK(Filters(layer).HasFilterThatMovesPixels());
      node.clip_type = ClipNode::ClipType::EXPANDS_CLIP;
      node.clip_expander =
          base::MakeUnique<ClipExpander>(layer->effect_tree_index());
    }
    data_for_children->clip_tree_parent =
        data_for_children->property_trees->clip_tree.Insert(node, parent_id);
    data_for_children->property_trees->clip_tree.SetOwningLayerIdForNode(
        data_for_children->property_trees->clip_tree.back(), layer->id());
  }

  layer->SetClipTreeIndex(data_for_children->clip_tree_parent);
}

template <typename LayerType>
static inline bool IsAtBoundaryOf3dRenderingContext(LayerType* layer) {
  return Parent(layer)
             ? SortingContextId(Parent(layer)) != SortingContextId(layer)
             : Is3dSorted(layer);
}

static inline gfx::Point3F TransformOrigin(Layer* layer) {
  return layer->transform_origin();
}

static inline gfx::Point3F TransformOrigin(LayerImpl* layer) {
  return layer->test_properties()->transform_origin;
}

static inline bool IsContainerForFixedPositionLayers(Layer* layer) {
  return layer->IsContainerForFixedPositionLayers();
}

static inline bool IsContainerForFixedPositionLayers(LayerImpl* layer) {
  return layer->test_properties()->is_container_for_fixed_position_layers;
}

static inline bool ShouldFlattenTransform(Layer* layer) {
  return layer->should_flatten_transform();
}

static inline bool ShouldFlattenTransform(LayerImpl* layer) {
  return layer->test_properties()->should_flatten_transform;
}

template <typename LayerType>
bool AddTransformNodeIfNeeded(
    const DataForRecursion<LayerType>& data_from_ancestor,
    LayerType* layer,
    bool created_render_surface,
    DataForRecursion<LayerType>* data_for_children) {
  const bool is_root = !Parent(layer);
  const bool is_page_scale_layer = layer == data_from_ancestor.page_scale_layer;
  const bool is_overscroll_elasticity_layer =
      layer == data_from_ancestor.overscroll_elasticity_layer;
  const bool is_scrollable = layer->scrollable();
  const bool is_fixed = PositionConstraint(layer).is_fixed_position();
  const bool is_sticky = StickyPositionConstraint(layer).is_sticky;
  const bool is_snapped = layer->IsSnapped();

  const bool has_significant_transform =
      !Transform(layer).IsIdentityOr2DTranslation();

  const bool has_potentially_animated_transform =
      HasPotentiallyRunningTransformAnimation(layer);

  // A transform node is needed even for a finished animation, since differences
  // in the timing of animation state updates can mean that an animation that's
  // in the Finished state at tree-building time on the main thread is still in
  // the Running state right after commit on the compositor thread.
  const bool has_any_transform_animation =
      HasAnyAnimationTargetingProperty(layer, TargetProperty::TRANSFORM);

  const bool has_proxied_transform_related_property =
      !!(layer->mutable_properties() & MutableProperty::kTransformRelated);

  const bool has_surface = created_render_surface;

  const bool is_at_boundary_of_3d_rendering_context =
      IsAtBoundaryOf3dRenderingContext(layer);

  DCHECK(!is_scrollable || is_snapped);
  bool requires_node = is_root || is_snapped || has_significant_transform ||
                       has_any_transform_animation || has_surface || is_fixed ||
                       is_page_scale_layer || is_overscroll_elasticity_layer ||
                       has_proxied_transform_related_property || is_sticky ||
                       is_at_boundary_of_3d_rendering_context;

  int parent_index = TransformTree::kRootNodeId;
  int source_index = TransformTree::kRootNodeId;
  gfx::Vector2dF source_offset;

  LayerType* transform_parent = GetTransformParent(data_from_ancestor, layer);
  DCHECK_EQ(is_root, !transform_parent);

  if (transform_parent) {
    parent_index = transform_parent->transform_tree_index();
    // Because Blink still provides positions with respect to the parent layer,
    // we track both a parent TransformNode (which is the parent in the
    // TransformTree) and a 'source' TransformNode (which is the TransformNode
    // for the parent in the Layer tree).
    source_index = Parent(layer)->transform_tree_index();
    source_offset = Parent(layer)->offset_to_transform_parent();
  }

  if (IsContainerForFixedPositionLayers(layer) || is_root) {
    data_for_children->affected_by_inner_viewport_bounds_delta =
        layer == data_from_ancestor.inner_viewport_scroll_layer;
    data_for_children->affected_by_outer_viewport_bounds_delta =
        layer == data_from_ancestor.outer_viewport_scroll_layer;
    if (is_scrollable) {
      DCHECK(!is_root);
      DCHECK(Transform(layer).IsIdentity());
      data_for_children->transform_fixed_parent = Parent(layer);
    } else {
      data_for_children->transform_fixed_parent = layer;
    }
  }
  data_for_children->transform_tree_parent = layer;

  if (!requires_node) {
    data_for_children->should_flatten |= ShouldFlattenTransform(layer);
    gfx::Vector2dF local_offset = layer->position().OffsetFromOrigin() +
                                  Transform(layer).To2dTranslation();
    gfx::Vector2dF source_to_parent;
    if (source_index != parent_index) {
      gfx::Transform to_parent;
      data_from_ancestor.property_trees->transform_tree.ComputeTranslation(
          source_index, parent_index, &to_parent);
      source_to_parent = to_parent.To2dTranslation();
    }
    layer->set_offset_to_transform_parent(source_offset + source_to_parent +
                                          local_offset);
    layer->set_should_flatten_transform_from_property_tree(
        data_from_ancestor.should_flatten);
    layer->SetTransformTreeIndex(parent_index);
    return false;
  }

  data_for_children->property_trees->transform_tree.Insert(TransformNode(),
                                                           parent_index);

  TransformNode* node =
      data_for_children->property_trees->transform_tree.back();
  layer->SetTransformTreeIndex(node->id);
  data_for_children->property_trees->transform_tree.SetOwningLayerIdForNode(
      node, layer->id());

  // For animation subsystem purposes, if this layer has a compositor element
  // id, we build a map from that id to this transform node.
  if (layer->element_id()) {
    data_for_children->property_trees
        ->element_id_to_transform_node_index[layer->element_id()] = node->id;
  }

  node->scrolls = is_scrollable;
  node->should_be_snapped = is_snapped;
  node->flattens_inherited_transform = data_for_children->should_flatten;

  node->sorting_context_id = SortingContextId(layer);

  if (layer == data_from_ancestor.page_scale_layer)
    data_for_children->in_subtree_of_page_scale_layer = true;
  node->in_subtree_of_page_scale_layer =
      data_for_children->in_subtree_of_page_scale_layer;

  // Surfaces inherently flatten transforms.
  data_for_children->should_flatten =
      ShouldFlattenTransform(layer) || has_surface;
  DCHECK_GT(data_from_ancestor.property_trees->effect_tree.size(), 0u);

  node->has_potential_animation = has_potentially_animated_transform;
  node->is_currently_animating = TransformIsAnimating(layer);
  if (has_potentially_animated_transform) {
    node->has_only_translation_animations =
        HasOnlyTranslationTransforms(layer);
  }

  float post_local_scale_factor = 1.0f;

  if (is_page_scale_layer) {
    if (!is_root)
      post_local_scale_factor *= data_from_ancestor.page_scale_factor;
    data_for_children->property_trees->transform_tree.set_page_scale_factor(
        data_from_ancestor.page_scale_factor);
  }

  node->source_node_id = source_index;
  node->post_local_scale_factor = post_local_scale_factor;
  if (is_root) {
    float page_scale_factor_for_root =
        is_page_scale_layer ? data_from_ancestor.page_scale_factor : 1.f;
    data_for_children->property_trees->transform_tree
        .SetRootTransformsAndScales(data_for_children->property_trees
                                        ->transform_tree.device_scale_factor(),
                                    page_scale_factor_for_root,
                                    *data_from_ancestor.device_transform,
                                    layer->position());
  } else {
    node->source_offset = source_offset;
    node->update_post_local_transform(layer->position(),
                                      TransformOrigin(layer));
  }

  if (is_overscroll_elasticity_layer) {
    DCHECK(!is_scrollable);
    node->scroll_offset =
        gfx::ScrollOffset(data_from_ancestor.elastic_overscroll);
  } else if (!ScrollParent(layer)) {
    node->scroll_offset = layer->CurrentScrollOffset();
  }

  if (is_fixed) {
    if (data_from_ancestor.affected_by_inner_viewport_bounds_delta) {
      node->moved_by_inner_viewport_bounds_delta_x =
          PositionConstraint(layer).is_fixed_to_right_edge();
      node->moved_by_inner_viewport_bounds_delta_y =
          PositionConstraint(layer).is_fixed_to_bottom_edge();
      if (node->moved_by_inner_viewport_bounds_delta_x ||
          node->moved_by_inner_viewport_bounds_delta_y) {
        data_for_children->property_trees->transform_tree
            .AddNodeAffectedByInnerViewportBoundsDelta(node->id);
      }
    } else if (data_from_ancestor.affected_by_outer_viewport_bounds_delta) {
      node->moved_by_outer_viewport_bounds_delta_x =
          PositionConstraint(layer).is_fixed_to_right_edge();
      node->moved_by_outer_viewport_bounds_delta_y =
          PositionConstraint(layer).is_fixed_to_bottom_edge();
      if (node->moved_by_outer_viewport_bounds_delta_x ||
          node->moved_by_outer_viewport_bounds_delta_y) {
        data_for_children->property_trees->transform_tree
            .AddNodeAffectedByOuterViewportBoundsDelta(node->id);
      }
    }
  }

  node->local = Transform(layer);
  node->update_pre_local_transform(TransformOrigin(layer));

  if (StickyPositionConstraint(layer).is_sticky) {
    StickyPositionNodeData* sticky_data =
        data_for_children->property_trees->transform_tree.StickyPositionData(
            node->id);
    sticky_data->constraints = StickyPositionConstraint(layer);
    sticky_data->scroll_ancestor = GetScrollParentId(data_from_ancestor, layer);
    ScrollNode* scroll_ancestor =
        data_for_children->property_trees->scroll_tree.Node(
            sticky_data->scroll_ancestor);
    if (sticky_data->constraints.is_anchored_right ||
        sticky_data->constraints.is_anchored_bottom) {
      // Sticky nodes whose ancestor scroller is the inner / outer viewport
      // need to have their local transform updated when the inner / outer
      // viewport bounds change, but do not unconditionally move by that delta
      // like fixed position nodes.
      if (scroll_ancestor->scrolls_inner_viewport) {
        data_for_children->property_trees->transform_tree
            .AddNodeAffectedByInnerViewportBoundsDelta(node->id);
      } else if (scroll_ancestor->scrolls_outer_viewport) {
        data_for_children->property_trees->transform_tree
            .AddNodeAffectedByOuterViewportBoundsDelta(node->id);
      }
    }
    // TODO(smcgruer): Pass main thread sticky-shifting offsets of
    // non-promoted ancestors, or promote all ancestor sticky elements.
    // See http://crbug.com/702229
    sticky_data->main_thread_offset =
        layer->position().OffsetFromOrigin() -
        sticky_data->constraints.parent_relative_sticky_box_offset
            .OffsetFromOrigin();

    // Copy the ancestor nodes for later use. These layers are guaranteed to
    // have transform nodes at this point because they are our ancestors (so
    // have already been processed) and are sticky (so have transform nodes).
    int shifting_sticky_box_layer_id =
        sticky_data->constraints.nearest_layer_shifting_sticky_box;
    if (shifting_sticky_box_layer_id != Layer::INVALID_ID) {
      sticky_data->nearest_node_shifting_sticky_box =
          data_for_children->property_trees->transform_tree
              .FindNodeIndexFromOwningLayerId(shifting_sticky_box_layer_id);
      DCHECK(sticky_data->nearest_node_shifting_sticky_box !=
             TransformTree::kInvalidNodeId);
    }
    int shifting_containing_block_layer_id =
        sticky_data->constraints.nearest_layer_shifting_containing_block;
    if (shifting_containing_block_layer_id != Layer::INVALID_ID) {
      sticky_data->nearest_node_shifting_containing_block =
          data_for_children->property_trees->transform_tree
              .FindNodeIndexFromOwningLayerId(
                  shifting_containing_block_layer_id);
      DCHECK(sticky_data->nearest_node_shifting_containing_block !=
             TransformTree::kInvalidNodeId);
    }
  }

  node->needs_local_transform_update = true;
  data_from_ancestor.property_trees->transform_tree.UpdateTransforms(node->id);

  layer->set_offset_to_transform_parent(gfx::Vector2dF());

  // Flattening (if needed) will be handled by |node|.
  layer->set_should_flatten_transform_from_property_tree(false);

  node->owning_layer_id = layer->id();

  return true;
}

static inline bool HasPotentialOpacityAnimation(Layer* layer) {
  return HasPotentiallyRunningOpacityAnimation(layer) ||
         layer->OpacityCanAnimateOnImplThread();
}

static inline bool HasPotentialOpacityAnimation(LayerImpl* layer) {
  return HasPotentiallyRunningOpacityAnimation(layer) ||
         layer->test_properties()->opacity_can_animate;
}

static inline bool DoubleSided(Layer* layer) {
  return layer->double_sided();
}

static inline bool DoubleSided(LayerImpl* layer) {
  return layer->test_properties()->double_sided;
}

static inline bool ForceRenderSurface(Layer* layer) {
  return layer->force_render_surface_for_testing();
}

static inline bool ForceRenderSurface(LayerImpl* layer) {
  return layer->test_properties()->force_render_surface;
}

template <typename LayerType>
static inline bool LayerIsInExisting3DRenderingContext(LayerType* layer) {
  return Is3dSorted(layer) && Parent(layer) && Is3dSorted(Parent(layer)) &&
         (SortingContextId(Parent(layer)) == SortingContextId(layer));
}

static inline bool IsRootForIsolatedGroup(Layer* layer) {
  return layer->is_root_for_isolated_group();
}

static inline bool IsRootForIsolatedGroup(LayerImpl* layer) {
  return false;
}

static inline int NumDescendantsThatDrawContent(Layer* layer) {
  return layer->NumDescendantsThatDrawContent();
}

static inline int NumLayerOrDescendantsThatDrawContentRecursive(
    LayerImpl* layer) {
  int num = layer->DrawsContent() ? 1 : 0;
  for (size_t i = 0; i < layer->test_properties()->children.size(); ++i) {
    LayerImpl* child_layer = layer->test_properties()->children[i];
    num += NumLayerOrDescendantsThatDrawContentRecursive(child_layer);
  }
  return num;
}

static inline int NumDescendantsThatDrawContent(LayerImpl* layer) {
  int num_descendants_that_draw_content = 0;
  for (size_t i = 0; i < layer->test_properties()->children.size(); ++i) {
    LayerImpl* child_layer = layer->test_properties()->children[i];
    num_descendants_that_draw_content +=
        NumLayerOrDescendantsThatDrawContentRecursive(child_layer);
  }
  return num_descendants_that_draw_content;
}

static inline float EffectiveOpacity(Layer* layer) {
  return layer->EffectiveOpacity();
}

static inline float EffectiveOpacity(LayerImpl* layer) {
  return layer->test_properties()->hide_layer_and_subtree
             ? 0.f
             : layer->test_properties()->opacity;
}

static inline float Opacity(Layer* layer) {
  return layer->opacity();
}

static inline float Opacity(LayerImpl* layer) {
  return layer->test_properties()->opacity;
}

static inline SkBlendMode BlendMode(Layer* layer) {
  return layer->blend_mode();
}

static inline SkBlendMode BlendMode(LayerImpl* layer) {
  return layer->test_properties()->blend_mode;
}

static inline const gfx::PointF FiltersOrigin(Layer* layer) {
  return layer->filters_origin();
}

static inline const gfx::PointF FiltersOrigin(LayerImpl* layer) {
  return layer->test_properties()->filters_origin;
}

static inline const FilterOperations& BackgroundFilters(Layer* layer) {
  return layer->background_filters();
}

static inline const FilterOperations& BackgroundFilters(LayerImpl* layer) {
  return layer->test_properties()->background_filters;
}

static inline bool HideLayerAndSubtree(Layer* layer) {
  return layer->hide_layer_and_subtree();
}

static inline bool HideLayerAndSubtree(LayerImpl* layer) {
  return layer->test_properties()->hide_layer_and_subtree;
}

static inline bool HasCopyRequest(Layer* layer) {
  return layer->HasCopyRequest();
}

static inline bool HasCopyRequest(LayerImpl* layer) {
  return !layer->test_properties()->copy_requests.empty();
}

static inline bool PropertyChanged(Layer* layer) {
  return layer->subtree_property_changed();
}

static inline bool PropertyChanged(LayerImpl* layer) {
  return false;
}

template <typename LayerType>
bool ShouldCreateRenderSurface(LayerType* layer,
                               gfx::Transform current_transform,
                               bool animation_axis_aligned) {
  const bool preserves_2d_axis_alignment =
      current_transform.Preserves2dAxisAlignment() && animation_axis_aligned;
  const bool is_root = !Parent(layer);
  if (is_root)
    return true;

  // If the layer uses a mask.
  if (MaskLayer(layer)) {
    return true;
  }

  // If the layer uses a CSS filter.
  if (!Filters(layer).IsEmpty() || !BackgroundFilters(layer).IsEmpty()) {
    return true;
  }

  // If the layer will use a CSS filter.  In this case, the animation
  // will start and add a filter to this layer, so it needs a surface.
  if (HasPotentiallyRunningFilterAnimation(layer)) {
    return true;
  }

  int num_descendants_that_draw_content = NumDescendantsThatDrawContent(layer);

  // If the layer flattens its subtree, but it is treated as a 3D object by its
  // parent (i.e. parent participates in a 3D rendering context).
  if (LayerIsInExisting3DRenderingContext(layer) &&
      ShouldFlattenTransform(layer) && num_descendants_that_draw_content > 0) {
    TRACE_EVENT_INSTANT0(
        "cc", "PropertyTreeBuilder::ShouldCreateRenderSurface flattening",
        TRACE_EVENT_SCOPE_THREAD);
    return true;
  }

  // If the layer has blending.
  // TODO(rosca): this is temporary, until blending is implemented for other
  // types of quads than RenderPassDrawQuad. Layers having descendants that draw
  // content will still create a separate rendering surface.
  if (BlendMode(layer) != SkBlendMode::kSrcOver) {
    TRACE_EVENT_INSTANT0(
        "cc", "PropertyTreeBuilder::ShouldCreateRenderSurface blending",
        TRACE_EVENT_SCOPE_THREAD);
    return true;
  }
  // If the layer clips its descendants but it is not axis-aligned with respect
  // to its parent.
  bool layer_clips_external_content = LayerClipsSubtree(layer);
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

  bool may_have_transparency = EffectiveOpacity(layer) != 1.f ||
                               HasPotentiallyRunningOpacityAnimation(layer);
  if (may_have_transparency && ShouldFlattenTransform(layer) &&
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
  if (IsRootForIsolatedGroup(layer)) {
    TRACE_EVENT_INSTANT0(
        "cc", "PropertyTreeBuilder::ShouldCreateRenderSurface isolation",
        TRACE_EVENT_SCOPE_THREAD);
    return true;
  }

  // If we force it.
  if (ForceRenderSurface(layer))
    return true;

  // If we'll make a copy of the layer's contents.
  if (HasCopyRequest(layer))
    return true;

  return false;
}

static void TakeCopyRequests(
    Layer* layer,
    std::vector<std::unique_ptr<CopyOutputRequest>>* copy_requests) {
  layer->TakeCopyRequests(copy_requests);
}

static void TakeCopyRequests(
    LayerImpl* layer,
    std::vector<std::unique_ptr<CopyOutputRequest>>* copy_requests) {
  for (auto& request : layer->test_properties()->copy_requests)
    copy_requests->push_back(std::move(request));
  layer->test_properties()->copy_requests.clear();
}

static void SetSubtreeHasCopyRequest(Layer* layer,
                                     bool subtree_has_copy_request) {
  layer->SetSubtreeHasCopyRequest(subtree_has_copy_request);
}

static void SetSubtreeHasCopyRequest(LayerImpl* layer,
                                     bool subtree_has_copy_request) {
  layer->test_properties()->subtree_has_copy_request = subtree_has_copy_request;
}

static bool SubtreeHasCopyRequest(Layer* layer) {
  return layer->SubtreeHasCopyRequest();
}

static bool SubtreeHasCopyRequest(LayerImpl* layer) {
  return layer->test_properties()->subtree_has_copy_request;
}

template <typename LayerType>
bool UpdateSubtreeHasCopyRequestRecursive(LayerType* layer) {
  bool subtree_has_copy_request = false;
  if (HasCopyRequest(layer))
    subtree_has_copy_request = true;
  for (size_t i = 0; i < Children(layer).size(); ++i) {
    LayerType* current_child = ChildAt(layer, i);
    subtree_has_copy_request |=
        UpdateSubtreeHasCopyRequestRecursive(current_child);
  }
  SetSubtreeHasCopyRequest(layer, subtree_has_copy_request);
  return subtree_has_copy_request;
}

template <typename LayerType>
bool AddEffectNodeIfNeeded(
    const DataForRecursion<LayerType>& data_from_ancestor,
    LayerType* layer,
    DataForRecursion<LayerType>* data_for_children) {
  const bool is_root = !Parent(layer);
  const bool has_transparency = EffectiveOpacity(layer) != 1.f;
  const bool has_potential_opacity_animation =
      HasPotentialOpacityAnimation(layer);
  const bool has_potential_filter_animation =
      HasPotentiallyRunningFilterAnimation(layer);
  const bool has_proxied_opacity =
      !!(layer->mutable_properties() & MutableProperty::kOpacity);

  data_for_children->animation_axis_aligned_since_render_target &=
      AnimationsPreserveAxisAlignment(layer);
  data_for_children->compound_transform_since_render_target *= Transform(layer);
  const bool should_create_render_surface = ShouldCreateRenderSurface(
      layer, data_for_children->compound_transform_since_render_target,
      data_for_children->animation_axis_aligned_since_render_target);

  bool not_axis_aligned_since_last_clip =
      data_from_ancestor.not_axis_aligned_since_last_clip
          ? true
          : !AnimationsPreserveAxisAlignment(layer) ||
                !Transform(layer).Preserves2dAxisAlignment();
  // A non-axis aligned clip may need a render surface. So, we create an effect
  // node.
  bool has_non_axis_aligned_clip =
      not_axis_aligned_since_last_clip && LayerClipsSubtree(layer);

  bool requires_node = is_root || has_transparency ||
                       has_potential_opacity_animation || has_proxied_opacity ||
                       has_non_axis_aligned_clip ||
                       should_create_render_surface;

  int parent_id = data_from_ancestor.effect_tree_parent;

  if (!requires_node) {
    layer->SetEffectTreeIndex(parent_id);
    data_for_children->effect_tree_parent = parent_id;
    return false;
  }

  EffectTree& effect_tree = data_for_children->property_trees->effect_tree;
  int node_id = effect_tree.Insert(EffectNode(), parent_id);
  EffectNode* node = effect_tree.back();

  node->owning_layer_id = layer->id();
  node->opacity = Opacity(layer);
  node->blend_mode = BlendMode(layer);
  node->unscaled_mask_target_size = layer->bounds();
  node->has_render_surface = should_create_render_surface;
  node->has_copy_request = HasCopyRequest(layer);
  node->filters = Filters(layer);
  node->background_filters = BackgroundFilters(layer);
  node->filters_origin = FiltersOrigin(layer);
  node->has_potential_opacity_animation = has_potential_opacity_animation;
  node->has_potential_filter_animation = has_potential_filter_animation;
  node->double_sided = DoubleSided(layer);
  node->subtree_hidden = HideLayerAndSubtree(layer);
  node->is_currently_animating_opacity = OpacityIsAnimating(layer);
  node->is_currently_animating_filter = FilterIsAnimating(layer);
  node->effect_changed = PropertyChanged(layer);
  node->subtree_has_copy_request = SubtreeHasCopyRequest(layer);
  node->closest_ancestor_with_copy_request_id =
      HasCopyRequest(layer)
          ? node_id
          : data_from_ancestor.closest_ancestor_with_copy_request;

  if (MaskLayer(layer)) {
    node->mask_layer_id = MaskLayer(layer)->id();
    effect_tree.AddMaskLayerId(node->mask_layer_id);
  }

  if (!is_root) {
    // The effect node's transform id is used only when we create a render
    // surface. So, we can leave the default value when we don't create a render
    // surface.
    if (should_create_render_surface) {
      // In this case, we will create a transform node, so it's safe to use the
      // next available id from the transform tree as this effect node's
      // transform id.
      node->transform_id =
          data_from_ancestor.property_trees->transform_tree.next_available_id();
    }
    node->clip_id = data_from_ancestor.clip_tree_parent;
  } else {
    // The root render surface acts as the unbounded and untransformed
    // surface into which content is drawn. The transform node created
    // from the root layer (which includes device scale factor) and
    // the clip node created from the root layer (which includes
    // viewports) apply to the root render surface's content, but not
    // to the root render surface itself.
    node->transform_id = TransformTree::kRootNodeId;
    node->clip_id = ClipTree::kViewportNodeId;
  }

  data_for_children->closest_ancestor_with_copy_request =
      node->closest_ancestor_with_copy_request_id;
  data_for_children->effect_tree_parent = node_id;
  layer->SetEffectTreeIndex(node_id);
  data_for_children->property_trees->effect_tree.SetOwningLayerIdForNode(
      effect_tree.back(), layer->id());

  // For animation subsystem purposes, if this layer has a compositor element
  // id, we build a map from that id to this effect node.
  if (layer->element_id()) {
    data_for_children->property_trees
        ->element_id_to_effect_node_index[layer->element_id()] = node_id;
  }

  std::vector<std::unique_ptr<CopyOutputRequest>> layer_copy_requests;
  TakeCopyRequests(layer, &layer_copy_requests);
  for (auto& it : layer_copy_requests) {
    effect_tree.AddCopyRequest(node_id, std::move(it));
  }
  layer_copy_requests.clear();

  if (should_create_render_surface) {
    data_for_children->compound_transform_since_render_target =
        gfx::Transform();
    data_for_children->animation_axis_aligned_since_render_target = true;
  }
  return should_create_render_surface;
}

template <typename LayerType>
void AddScrollNodeIfNeeded(
    const DataForRecursion<LayerType>& data_from_ancestor,
    LayerType* layer,
    DataForRecursion<LayerType>* data_for_children) {
  int parent_id = GetScrollParentId(data_from_ancestor, layer);

  bool is_root = !Parent(layer);
  bool scrollable = layer->scrollable();
  bool contains_non_fast_scrollable_region =
      !layer->non_fast_scrollable_region().IsEmpty();
  uint32_t main_thread_scrolling_reasons =
      layer->main_thread_scrolling_reasons();

  bool scroll_node_uninheritable_criteria =
      is_root || scrollable || contains_non_fast_scrollable_region;
  bool has_different_main_thread_scrolling_reasons =
      main_thread_scrolling_reasons !=
      data_from_ancestor.main_thread_scrolling_reasons;
  bool requires_node =
      scroll_node_uninheritable_criteria ||
      (main_thread_scrolling_reasons !=
           MainThreadScrollingReason::kNotScrollingOnMain &&
       (has_different_main_thread_scrolling_reasons ||
        data_from_ancestor
            .scroll_tree_parent_created_by_uninheritable_criteria));

  int node_id;
  if (!requires_node) {
    node_id = parent_id;
    data_for_children->scroll_tree_parent = node_id;
  } else {
    ScrollNode node;
    node.owning_layer_id = layer->id();
    node.scrollable = scrollable;
    node.main_thread_scrolling_reasons = main_thread_scrolling_reasons;
    node.non_fast_scrollable_region = layer->non_fast_scrollable_region();
    gfx::Size clip_bounds;
    if (LayerType* scroll_clip_layer = layer->scroll_clip_layer()) {
      SetIsScrollClipLayer(scroll_clip_layer);
      clip_bounds = scroll_clip_layer->bounds();
      DCHECK(scroll_clip_layer->transform_tree_index() !=
             TransformTree::kInvalidNodeId);
      node.max_scroll_offset_affected_by_page_scale =
          !data_from_ancestor.property_trees->transform_tree
               .Node(scroll_clip_layer->transform_tree_index())
               ->in_subtree_of_page_scale_layer &&
          data_from_ancestor.in_subtree_of_page_scale_layer;
    }

    node.scroll_clip_layer_bounds = clip_bounds;
    node.scrolls_inner_viewport =
        layer == data_from_ancestor.inner_viewport_scroll_layer;
    node.scrolls_outer_viewport =
        layer == data_from_ancestor.outer_viewport_scroll_layer;

    node.bounds = layer->bounds();
    node.offset_to_transform_parent = layer->offset_to_transform_parent();
    node.should_flatten = layer->should_flatten_transform_from_property_tree();
    node.user_scrollable_horizontal = layer->user_scrollable_horizontal();
    node.user_scrollable_vertical = layer->user_scrollable_vertical();
    node.element_id = layer->element_id();
    node.transform_id =
        data_for_children->transform_tree_parent->transform_tree_index();

    node_id =
        data_for_children->property_trees->scroll_tree.Insert(node, parent_id);
    data_for_children->scroll_tree_parent = node_id;
    data_for_children->main_thread_scrolling_reasons =
        node.main_thread_scrolling_reasons;
    data_for_children->scroll_tree_parent_created_by_uninheritable_criteria =
        scroll_node_uninheritable_criteria;
    data_for_children->property_trees->scroll_tree.SetOwningLayerIdForNode(
        data_for_children->property_trees->scroll_tree.back(), layer->id());
    // For animation subsystem purposes, if this layer has a compositor element
    // id, we build a map from that id to this scroll node.
    if (layer->element_id()) {
      data_for_children->property_trees
          ->element_id_to_scroll_node_index[layer->element_id()] = node_id;
    }

    if (node.scrollable) {
      data_for_children->property_trees->scroll_tree.SetBaseScrollOffset(
          layer->id(), layer->CurrentScrollOffset());
    }
  }

  layer->SetScrollTreeIndex(node_id);
}

template <typename LayerType>
void SetBackfaceVisibilityTransform(LayerType* layer,
                                    bool created_transform_node) {
  const bool is_at_boundary_of_3d_rendering_context =
      IsAtBoundaryOf3dRenderingContext(layer);
  if (layer->use_parent_backface_visibility()) {
    DCHECK(!is_at_boundary_of_3d_rendering_context);
    DCHECK(Parent(layer));
    DCHECK(!Parent(layer)->use_parent_backface_visibility());
    layer->SetUseLocalTransformForBackfaceVisibility(
        Parent(layer)->use_local_transform_for_backface_visibility());
    layer->SetShouldCheckBackfaceVisibility(
        Parent(layer)->should_check_backface_visibility());
  } else {
    // The current W3C spec on CSS transforms says that backface visibility
    // should be determined differently depending on whether the layer is in a
    // "3d rendering context" or not. For Chromium code, we can determine
    // whether we are in a 3d rendering context by checking if the parent
    // preserves 3d.
    const bool use_local_transform =
        !Is3dSorted(layer) ||
        (Is3dSorted(layer) && is_at_boundary_of_3d_rendering_context);
    layer->SetUseLocalTransformForBackfaceVisibility(use_local_transform);

    // A double-sided layer's backface can been shown when its visible.
    if (DoubleSided(layer))
      layer->SetShouldCheckBackfaceVisibility(false);
    // The backface of a layer that uses local transform for backface visibility
    // is not visible when it does not create a transform node as its local
    // transform is identity or 2d translation and is not animating.
    else if (use_local_transform && !created_transform_node)
      layer->SetShouldCheckBackfaceVisibility(false);
    else
      layer->SetShouldCheckBackfaceVisibility(true);
  }
}

template <typename LayerType>
void SetSafeOpaqueBackgroundColor(
    const DataForRecursion<LayerType>& data_from_ancestor,
    LayerType* layer,
    DataForRecursion<LayerType>* data_for_children) {
  SkColor background_color = layer->background_color();
  data_for_children->safe_opaque_background_color =
      SkColorGetA(background_color) == 255
          ? background_color
          : data_from_ancestor.safe_opaque_background_color;
  layer->SetSafeOpaqueBackgroundColor(
      data_for_children->safe_opaque_background_color);
}

static void SetLayerPropertyChangedForChild(Layer* parent, Layer* child) {
  if (parent->subtree_property_changed())
    child->SetSubtreePropertyChanged();
}

static void SetLayerPropertyChangedForChild(LayerImpl* parent,
                                            LayerImpl* child) {}

template <typename LayerType>
void BuildPropertyTreesInternal(
    LayerType* layer,
    const DataForRecursion<LayerType>& data_from_parent) {
  layer->set_property_tree_sequence_number(
      data_from_parent.property_trees->sequence_number);

  DataForRecursion<LayerType> data_for_children(data_from_parent);

  bool created_render_surface =
      AddEffectNodeIfNeeded(data_from_parent, layer, &data_for_children);


  bool created_transform_node = AddTransformNodeIfNeeded(
      data_from_parent, layer, created_render_surface, &data_for_children);
  AddClipNodeIfNeeded(data_from_parent, layer, created_transform_node,
                      &data_for_children);

  AddScrollNodeIfNeeded(data_from_parent, layer, &data_for_children);

  SetBackfaceVisibilityTransform(layer, created_transform_node);
  SetSafeOpaqueBackgroundColor(data_from_parent, layer, &data_for_children);

  bool not_axis_aligned_since_last_clip =
      data_from_parent.not_axis_aligned_since_last_clip
          ? true
          : !AnimationsPreserveAxisAlignment(layer) ||
                !Transform(layer).Preserves2dAxisAlignment();
  bool has_non_axis_aligned_clip =
      not_axis_aligned_since_last_clip && LayerClipsSubtree(layer);
  data_for_children.not_axis_aligned_since_last_clip =
      !has_non_axis_aligned_clip;

  for (size_t i = 0; i < Children(layer).size(); ++i) {
    LayerType* current_child = ChildAt(layer, i);
    SetLayerPropertyChangedForChild(layer, current_child);
    if (!ScrollParent(current_child)) {
      BuildPropertyTreesInternal(current_child, data_for_children);
    } else {
      // The child should be included in its scroll parent's list of scroll
      // children.
      DCHECK(ScrollChildren(ScrollParent(current_child))->count(current_child));
    }
  }

  if (ScrollChildren(layer)) {
    for (LayerType* scroll_child : *ScrollChildren(layer)) {
      DCHECK_EQ(ScrollParent(scroll_child), layer);
      DCHECK(Parent(scroll_child));
      data_for_children.effect_tree_parent =
          Parent(scroll_child)->effect_tree_index();
      BuildPropertyTreesInternal(scroll_child, data_for_children);
    }
  }

  if (MaskLayer(layer)) {
    MaskLayer(layer)->set_property_tree_sequence_number(
        data_from_parent.property_trees->sequence_number);
    MaskLayer(layer)->set_offset_to_transform_parent(
        layer->offset_to_transform_parent());
    MaskLayer(layer)->SetTransformTreeIndex(layer->transform_tree_index());
    MaskLayer(layer)->SetClipTreeIndex(layer->clip_tree_index());
    MaskLayer(layer)->SetEffectTreeIndex(layer->effect_tree_index());
    MaskLayer(layer)->SetScrollTreeIndex(layer->scroll_tree_index());
  }
}

}  // namespace

Layer* PropertyTreeBuilder::FindFirstScrollableLayer(Layer* layer) {
  if (!layer)
    return nullptr;

  if (layer->scrollable())
    return layer;

  for (size_t i = 0; i < layer->children().size(); ++i) {
    Layer* found = FindFirstScrollableLayer(layer->children()[i].get());
    if (found)
      return found;
  }

  return nullptr;
}

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
    PropertyTrees* property_trees,
    SkColor color) {
  if (!property_trees->needs_rebuild) {
    draw_property_utils::UpdatePageScaleFactor(
        property_trees, page_scale_layer, page_scale_factor,
        device_scale_factor, device_transform);
    draw_property_utils::UpdateElasticOverscroll(
        property_trees, overscroll_elasticity_layer, elastic_overscroll);
    property_trees->clip_tree.SetViewportClip(gfx::RectF(viewport));
    float page_scale_factor_for_root =
        page_scale_layer == root_layer ? page_scale_factor : 1.f;
    property_trees->transform_tree.SetRootTransformsAndScales(
        device_scale_factor, page_scale_factor_for_root, device_transform,
        root_layer->position());
    return;
  }

  DataForRecursion<LayerType> data_for_recursion;
  data_for_recursion.property_trees = property_trees;
  data_for_recursion.transform_tree_parent = nullptr;
  data_for_recursion.transform_fixed_parent = nullptr;
  data_for_recursion.clip_tree_parent = ClipTree::kRootNodeId;
  data_for_recursion.effect_tree_parent = EffectTree::kInvalidNodeId;
  data_for_recursion.scroll_tree_parent = ScrollTree::kRootNodeId;
  data_for_recursion.closest_ancestor_with_copy_request =
      EffectTree::kInvalidNodeId;
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
  data_for_recursion.is_hidden = false;
  data_for_recursion.main_thread_scrolling_reasons =
      MainThreadScrollingReason::kNotScrollingOnMain;
  data_for_recursion.scroll_tree_parent_created_by_uninheritable_criteria =
      true;
  data_for_recursion.device_transform = &device_transform;

  data_for_recursion.property_trees->clear();
  data_for_recursion.compound_transform_since_render_target = gfx::Transform();
  data_for_recursion.animation_axis_aligned_since_render_target = true;
  data_for_recursion.not_axis_aligned_since_last_clip = false;
  data_for_recursion.property_trees->transform_tree.set_device_scale_factor(
      device_scale_factor);
  data_for_recursion.safe_opaque_background_color = color;

  ClipNode root_clip;
  root_clip.clip_type = ClipNode::ClipType::APPLIES_LOCAL_CLIP;
  root_clip.clip = gfx::RectF(viewport);
  root_clip.transform_id = TransformTree::kRootNodeId;
  data_for_recursion.clip_tree_parent =
      data_for_recursion.property_trees->clip_tree.Insert(
          root_clip, ClipTree::kRootNodeId);

  BuildPropertyTreesInternal(root_layer, data_for_recursion);
  property_trees->needs_rebuild = false;

  // The transform tree is kept up to date as it is built, but the
  // combined_clips stored in the clip tree and the screen_space_opacity and
  // is_drawn in the effect tree aren't computed during tree building.
  property_trees->transform_tree.set_needs_update(false);
  property_trees->clip_tree.set_needs_update(true);
  property_trees->effect_tree.set_needs_update(true);
  property_trees->scroll_tree.set_needs_update(false);
}

#if DCHECK_IS_ON()
static void CheckScrollAndClipPointersForLayer(Layer* layer) {
  if (!layer)
    return;

  if (layer->scroll_children()) {
    for (std::set<Layer*>::iterator it = layer->scroll_children()->begin();
         it != layer->scroll_children()->end(); ++it) {
      DCHECK_EQ((*it)->scroll_parent(), layer);
    }
  }

  if (layer->clip_children()) {
    for (std::set<Layer*>::iterator it = layer->clip_children()->begin();
         it != layer->clip_children()->end(); ++it) {
      DCHECK_EQ((*it)->clip_parent(), layer);
    }
  }
}
#endif

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
  property_trees->is_main_thread = true;
  property_trees->is_active = false;
  SkColor color = root_layer->layer_tree_host()->background_color();
  if (SkColorGetA(color) != 255)
    color = SkColorSetA(color, 255);
  if (root_layer->layer_tree_host()->has_copy_request())
    UpdateSubtreeHasCopyRequestRecursive(root_layer);
  BuildPropertyTreesTopLevelInternal(
      root_layer, page_scale_layer, inner_viewport_scroll_layer,
      outer_viewport_scroll_layer, overscroll_elasticity_layer,
      elastic_overscroll, page_scale_factor, device_scale_factor, viewport,
      device_transform, property_trees, color);
#if DCHECK_IS_ON()
  for (auto* layer : *root_layer->layer_tree_host())
    CheckScrollAndClipPointersForLayer(layer);
#endif
  property_trees->ResetCachedData();
  // During building property trees, all copy requests are moved from layers to
  // effect tree, which are then pushed at commit to compositor thread and
  // handled there. LayerTreeHost::has_copy_request is only required to
  // decide if we want to create a effect node. So, it can be reset now.
  root_layer->layer_tree_host()->SetHasCopyRequest(false);
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
  // Preserve render surfaces when rebuilding.
  std::vector<std::unique_ptr<RenderSurfaceImpl>> render_surfaces;
  property_trees->effect_tree.TakeRenderSurfaces(&render_surfaces);
  property_trees->is_main_thread = false;
  property_trees->is_active = root_layer->IsActive();
  SkColor color = root_layer->layer_tree_impl()->background_color();
  if (SkColorGetA(color) != 255)
    color = SkColorSetA(color, 255);
  UpdateSubtreeHasCopyRequestRecursive(root_layer);
  BuildPropertyTreesTopLevelInternal(
      root_layer, page_scale_layer, inner_viewport_scroll_layer,
      outer_viewport_scroll_layer, overscroll_elasticity_layer,
      elastic_overscroll, page_scale_factor, device_scale_factor, viewport,
      device_transform, property_trees, color);
  property_trees->effect_tree.CreateOrReuseRenderSurfaces(
      &render_surfaces, root_layer->layer_tree_impl());
  property_trees->ResetCachedData();
}

}  // namespace cc
