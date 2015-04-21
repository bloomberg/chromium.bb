// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/draw_property_utils.h"

#include <vector>

#include "cc/base/math_util.h"
#include "cc/layers/layer.h"
#include "cc/layers/layer_impl.h"
#include "cc/trees/property_tree.h"
#include "cc/trees/property_tree_builder.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace cc {

namespace {

template <typename LayerType>
void CalculateVisibleRects(
    const std::vector<LayerType*>& layers_that_need_visible_rects,
    const ClipTree& clip_tree,
    const TransformTree& transform_tree) {
  for (size_t i = 0; i < layers_that_need_visible_rects.size(); ++i) {
    LayerType* layer = layers_that_need_visible_rects[i];

    // TODO(ajuma): Compute content_scale rather than using it. Note that for
    // PictureLayer and PictureImageLayers, content_bounds == bounds and
    // content_scale_x == content_scale_y == 1.0, so once impl painting is on
    // everywhere, this code will be unnecessary.
    gfx::Size layer_content_bounds = layer->content_bounds();
    float contents_scale_x = layer->contents_scale_x();
    float contents_scale_y = layer->contents_scale_y();
    const bool has_clip = layer->clip_tree_index() > 0;
    const TransformNode* transform_node =
        transform_tree.Node(layer->transform_tree_index());
    if (has_clip) {
      const ClipNode* clip_node = clip_tree.Node(layer->clip_tree_index());
      const TransformNode* clip_transform_node =
          transform_tree.Node(clip_node->data.transform_id);
      const bool target_is_root_surface =
          transform_node->data.content_target_id == 1;
      // When the target is the root surface, we need to include the root
      // transform by walking up to the root of the transform tree.
      const int target_id =
          target_is_root_surface ? 0 : transform_node->data.content_target_id;
      const TransformNode* target_node = transform_tree.Node(target_id);

      gfx::Transform content_to_target = transform_node->data.to_target;

      content_to_target.Translate(layer->offset_to_transform_parent().x(),
                                  layer->offset_to_transform_parent().y());
      content_to_target.Scale(1.0 / contents_scale_x, 1.0 / contents_scale_y);

      gfx::Rect clip_rect_in_target_space;
      gfx::Transform clip_to_target;
      bool success = true;
      if (clip_transform_node->data.target_id == target_node->id) {
        clip_to_target = clip_transform_node->data.to_target;
      } else {
        success = transform_tree.ComputeTransformWithDestinationSublayerScale(
            clip_transform_node->id, target_node->id, &clip_to_target);
      }

      if (target_node->id > clip_node->data.transform_id) {
        if (!success) {
          DCHECK(target_node->data.to_screen_is_animated);

          // An animated singular transform may become non-singular during the
          // animation, so we still need to compute a visible rect. In this
          // situation, we treat the entire layer as visible.
          layer->set_visible_rect_from_property_trees(
              gfx::Rect(layer_content_bounds));
          continue;
        }

        clip_rect_in_target_space =
            gfx::ToEnclosingRect(MathUtil::ProjectClippedRect(
                clip_to_target, clip_node->data.combined_clip));
      } else {
        // Computing a transform to an ancestor should always succeed.
        DCHECK(success);
        clip_rect_in_target_space =
            gfx::ToEnclosingRect(MathUtil::MapClippedRect(
                clip_to_target, clip_node->data.combined_clip));
      }

      gfx::Rect layer_content_rect = gfx::Rect(layer_content_bounds);
      gfx::Rect layer_content_bounds_in_target_space =
          MathUtil::MapEnclosingClippedRect(content_to_target,
                                            layer_content_rect);
      clip_rect_in_target_space.Intersect(layer_content_bounds_in_target_space);
      if (clip_rect_in_target_space.IsEmpty()) {
        layer->set_visible_rect_from_property_trees(gfx::Rect());
        continue;
      }

      gfx::Transform target_to_content;
      gfx::Transform target_to_layer;

      if (transform_node->data.ancestors_are_invertible) {
        target_to_layer = transform_node->data.from_target;
        success = true;
      } else {
        success = transform_tree.ComputeTransformWithSourceSublayerScale(
            target_node->id, transform_node->id, &target_to_layer);
      }

      if (!success) {
        DCHECK(transform_node->data.to_screen_is_animated);

        // An animated singular transform may become non-singular during the
        // animation, so we still need to compute a visible rect. In this
        // situation, we treat the entire layer as visible.
        layer->set_visible_rect_from_property_trees(
            gfx::Rect(layer_content_bounds));
        continue;
      }

      target_to_content.Scale(contents_scale_x, contents_scale_y);
      target_to_content.Translate(-layer->offset_to_transform_parent().x(),
                                  -layer->offset_to_transform_parent().y());
      target_to_content.PreconcatTransform(target_to_layer);

      gfx::Rect visible_rect = MathUtil::ProjectEnclosingClippedRect(
          target_to_content, clip_rect_in_target_space);

      visible_rect.Intersect(gfx::Rect(layer_content_bounds));

      layer->set_visible_rect_from_property_trees(visible_rect);
    } else {
      layer->set_visible_rect_from_property_trees(
          gfx::Rect(layer_content_bounds));
    }
  }
}

template <typename LayerType>
static bool IsRootLayerOfNewRenderingContext(LayerType* layer) {
  if (layer->parent())
    return !layer->parent()->Is3dSorted() && layer->Is3dSorted();
  return layer->Is3dSorted();
}

template <typename LayerType>
static inline bool LayerIsInExisting3DRenderingContext(LayerType* layer) {
  return layer->Is3dSorted() && layer->parent() &&
         layer->parent()->Is3dSorted();
}

template <typename LayerType>
static bool TransformToScreenIsKnown(LayerType* layer,
                                     const TransformTree& tree) {
  const TransformNode* node = tree.Node(layer->transform_tree_index());
  return !node->data.to_screen_is_animated;
}

template <typename LayerType>
static bool IsLayerBackFaceExposed(LayerType* layer,
                                   const TransformTree& tree) {
  if (!TransformToScreenIsKnown(layer, tree))
    return false;
  if (LayerIsInExisting3DRenderingContext(layer))
    return DrawTransformFromPropertyTrees(layer, tree).IsBackFaceVisible();
  return layer->transform().IsBackFaceVisible();
}

template <typename LayerType>
static bool IsSurfaceBackFaceExposed(LayerType* layer,
                                     const TransformTree& tree) {
  if (!TransformToScreenIsKnown(layer, tree))
    return false;
  if (LayerIsInExisting3DRenderingContext(layer))
    return DrawTransformFromPropertyTrees(layer, tree).IsBackFaceVisible();

  if (IsRootLayerOfNewRenderingContext(layer))
    return layer->transform().IsBackFaceVisible();

  // If the render_surface is not part of a new or existing rendering context,
  // then the layers that contribute to this surface will decide back-face
  // visibility for themselves.
  return false;
}

template <typename LayerType>
static bool HasSingularTransform(LayerType* layer, const TransformTree& tree) {
  const TransformNode* node = tree.Node(layer->transform_tree_index());
  return !node->data.is_invertible || !node->data.ancestors_are_invertible;
}

template <typename LayerType>
static bool IsBackFaceInvisible(LayerType* layer, const TransformTree& tree) {
  LayerType* backface_test_layer = layer;
  if (layer->use_parent_backface_visibility()) {
    DCHECK(layer->parent());
    DCHECK(!layer->parent()->use_parent_backface_visibility());
    backface_test_layer = layer->parent();
  }
  return !backface_test_layer->double_sided() &&
         IsLayerBackFaceExposed(backface_test_layer, tree);
}

template <typename LayerType>
static bool IsAnimatingTransformToScreen(LayerType* layer,
                                         const TransformTree& tree) {
  const TransformNode* node = tree.Node(layer->transform_tree_index());
  return node->data.to_screen_is_animated;
}

template <typename LayerType>
static bool IsInvisibleDueToTransform(LayerType* layer,
                                      const TransformTree& tree) {
  if (IsAnimatingTransformToScreen(layer, tree))
    return false;
  return HasSingularTransform(layer, tree) || IsBackFaceInvisible(layer, tree);
}

bool LayerIsInvisible(const Layer* layer) {
  return !layer->opacity() && !layer->OpacityIsAnimating() &&
         !layer->OpacityCanAnimateOnImplThread();
}

bool LayerIsInvisible(const LayerImpl* layer) {
  return !layer->opacity() && !layer->OpacityIsAnimating();
}

template <typename LayerType>
void FindLayersThatNeedVisibleRects(LayerType* layer,
                                    const TransformTree& tree,
                                    bool subtree_is_visible_from_ancestor,
                                    std::vector<LayerType*>* layers_to_update) {
  const bool layer_is_invisible = LayerIsInvisible(layer);
  const bool layer_is_backfacing =
      (layer->has_render_surface() && !layer->double_sided() &&
       IsSurfaceBackFaceExposed(layer, tree));

  const bool subtree_is_invisble = layer_is_invisible || layer_is_backfacing;
  if (subtree_is_invisble)
    return;

  bool layer_is_drawn =
      layer->HasCopyRequest() ||
      (subtree_is_visible_from_ancestor && !layer->hide_layer_and_subtree());

  if (layer_is_drawn && layer->DrawsContent()) {
    const bool visible = !IsInvisibleDueToTransform(layer, tree);
    if (visible)
      layers_to_update->push_back(layer);
  }

  for (size_t i = 0; i < layer->children().size(); ++i) {
    FindLayersThatNeedVisibleRects(layer->child_at(i), tree, layer_is_drawn,
                                   layers_to_update);
  }
}

}  // namespace

void ComputeClips(ClipTree* clip_tree, const TransformTree& transform_tree) {
  if (!clip_tree->needs_update())
    return;
  for (int i = 0; i < static_cast<int>(clip_tree->size()); ++i) {
    ClipNode* clip_node = clip_tree->Node(i);

    // Only descendants of a real clipping layer (i.e., not 0) may have their
    // clip adjusted due to intersecting with an ancestor clip.
    const bool is_clipped = clip_node->parent_id > 0;
    if (!is_clipped) {
      clip_node->data.combined_clip = clip_node->data.clip;
      continue;
    }

    ClipNode* parent_clip_node = clip_tree->parent(clip_node);
    const TransformNode* parent_transform_node =
        transform_tree.Node(parent_clip_node->data.transform_id);
    const TransformNode* transform_node =
        transform_tree.Node(clip_node->data.transform_id);

    // Clips must be combined in target space. We cannot, for example, combine
    // clips in the space of the child clip. The reason is non-affine
    // transforms. Say we have the following tree T->A->B->C, and B clips C, but
    // draw into target T. It may be the case that A applies a perspective
    // transform, and B and C are at different z positions. When projected into
    // target space, the relative sizes and positions of B and C can shift.
    // Since it's the relationship in target space that matters, that's where we
    // must combine clips.
    gfx::Transform parent_to_target;
    gfx::Transform clip_to_target;
    gfx::Transform target_to_clip;

    const bool target_is_root_surface = clip_node->data.target_id == 1;
    // When the target is the root surface, we need to include the root
    // transform by walking up to the root of the transform tree.
    const int target_id =
        target_is_root_surface ? 0 : clip_node->data.target_id;

    bool success = true;
    if (parent_transform_node->data.content_target_id ==
        clip_node->data.target_id) {
      parent_to_target = parent_transform_node->data.to_target;
    } else {
      success &= transform_tree.ComputeTransformWithDestinationSublayerScale(
          parent_transform_node->id, target_id, &parent_to_target);
    }

    if (transform_node->data.content_target_id == clip_node->data.target_id) {
      clip_to_target = transform_node->data.to_target;
    } else {
      success &= transform_tree.ComputeTransformWithDestinationSublayerScale(
          transform_node->id, target_id, &clip_to_target);
    }

    if (transform_node->data.content_target_id == clip_node->data.target_id &&
        transform_node->data.ancestors_are_invertible) {
      target_to_clip = transform_node->data.from_target;
    } else {
      success &= clip_to_target.GetInverse(&target_to_clip);
    }

    // If we can't compute a transform, it's because we had to use the inverse
    // of a singular transform. We won't draw in this case, so there's no need
    // to compute clips.
    if (!success)
      continue;

    // In order to intersect with as small a rect as possible, we do a
    // preliminary clip in target space so that when we project back, there's
    // less likelihood of intersecting the view plane.
    gfx::RectF inherited_clip_in_target_space = MathUtil::MapClippedRect(
        parent_to_target, parent_clip_node->data.combined_clip);

    gfx::RectF clip_in_target_space =
        MathUtil::MapClippedRect(clip_to_target, clip_node->data.clip);

    gfx::RectF intersected_in_target_space = gfx::IntersectRects(
        inherited_clip_in_target_space, clip_in_target_space);

    clip_node->data.combined_clip = MathUtil::ProjectClippedRect(
        target_to_clip, intersected_in_target_space);

    clip_node->data.combined_clip.Intersect(clip_node->data.clip);
  }
  clip_tree->set_needs_update(false);
}

void ComputeTransforms(TransformTree* transform_tree) {
  if (!transform_tree->needs_update())
    return;
  for (int i = 1; i < static_cast<int>(transform_tree->size()); ++i)
    transform_tree->UpdateTransforms(i);
  transform_tree->set_needs_update(false);
}

template <typename LayerType>
void ComputeVisibleRectsUsingPropertyTreesInternal(
    LayerType* root_layer,
    PropertyTrees* property_trees) {
  if (property_trees->transform_tree.needs_update())
    property_trees->clip_tree.set_needs_update(true);
  ComputeTransforms(&property_trees->transform_tree);
  ComputeClips(&property_trees->clip_tree, property_trees->transform_tree);

  std::vector<LayerType*> layers_to_update;
  const bool subtree_is_visible_from_ancestor = true;
  FindLayersThatNeedVisibleRects(root_layer, property_trees->transform_tree,
                                 subtree_is_visible_from_ancestor,
                                 &layers_to_update);
  CalculateVisibleRects(layers_to_update, property_trees->clip_tree,
                        property_trees->transform_tree);
}

void BuildPropertyTreesAndComputeVisibleRects(
    Layer* root_layer,
    const Layer* page_scale_layer,
    float page_scale_factor,
    float device_scale_factor,
    const gfx::Rect& viewport,
    const gfx::Transform& device_transform,
    PropertyTrees* property_trees) {
  PropertyTreeBuilder::BuildPropertyTrees(
      root_layer, page_scale_layer, page_scale_factor, device_scale_factor,
      viewport, device_transform, property_trees);
  ComputeVisibleRectsUsingPropertyTrees(root_layer, property_trees);
}

void BuildPropertyTreesAndComputeVisibleRects(
    LayerImpl* root_layer,
    const LayerImpl* page_scale_layer,
    float page_scale_factor,
    float device_scale_factor,
    const gfx::Rect& viewport,
    const gfx::Transform& device_transform,
    PropertyTrees* property_trees) {
  PropertyTreeBuilder::BuildPropertyTrees(
      root_layer, page_scale_layer, page_scale_factor, device_scale_factor,
      viewport, device_transform, property_trees);
  ComputeVisibleRectsUsingPropertyTrees(root_layer, property_trees);
}

void ComputeVisibleRectsUsingPropertyTrees(Layer* root_layer,
                                           PropertyTrees* property_trees) {
  ComputeVisibleRectsUsingPropertyTreesInternal(root_layer, property_trees);
}

void ComputeVisibleRectsUsingPropertyTrees(LayerImpl* root_layer,
                                           PropertyTrees* property_trees) {
  ComputeVisibleRectsUsingPropertyTreesInternal(root_layer, property_trees);
}

template <typename LayerType>
gfx::Transform DrawTransformFromPropertyTreesInternal(
    const LayerType* layer,
    const TransformTree& tree) {
  const TransformNode* node = tree.Node(layer->transform_tree_index());
  // TODO(vollick): ultimately we'll need to find this information (whether or
  // not we establish a render surface) somewhere other than the layer.
  const TransformNode* target_node =
      layer->render_surface() ? node : tree.Node(node->data.content_target_id);

  gfx::Transform xform;
  const bool owns_non_root_surface = layer->parent() && layer->render_surface();
  if (!owns_non_root_surface) {
    // If you're not the root, or you don't own a surface, you need to apply
    // your local offset.
    xform = node->data.to_target;
    if (layer->should_flatten_transform_from_property_tree())
      xform.FlattenTo2d();
    xform.Translate(layer->offset_to_transform_parent().x(),
                    layer->offset_to_transform_parent().y());
    // A fixed-position layer does not necessarily have the same render target
    // as its transform node. In particular, its transform node may be an
    // ancestor of its render target's transform node. For example, given layer
    // tree R->S->F, suppose F is fixed and S owns a render surface (e.g., say S
    // has opacity 0.9 and both S and F draw content). Then F's transform node
    // is the root node, so the target space transform from that node is defined
    // with respect to the root render surface. But F will render to S's
    // surface, so must apply a change of basis transform to the target space
    // transform from its transform node.
    if (layer->position_constraint().is_fixed_position()) {
      gfx::Transform tree_target_to_render_target;
      tree.ComputeTransform(node->data.content_target_id,
                            layer->render_target()->transform_tree_index(),
                            &tree_target_to_render_target);
      xform.ConcatTransform(tree_target_to_render_target);
    }
  } else {
    // Surfaces need to apply their sublayer scale.
    xform.Scale(target_node->data.sublayer_scale.x(),
                target_node->data.sublayer_scale.y());
  }
  xform.Scale(1.0 / layer->contents_scale_x(), 1.0 / layer->contents_scale_y());
  return xform;
}

gfx::Transform DrawTransformFromPropertyTrees(const Layer* layer,
                                              const TransformTree& tree) {
  return DrawTransformFromPropertyTreesInternal(layer, tree);
}

gfx::Transform DrawTransformFromPropertyTrees(const LayerImpl* layer,
                                              const TransformTree& tree) {
  return DrawTransformFromPropertyTreesInternal(layer, tree);
}

template <typename LayerType>
gfx::Transform ScreenSpaceTransformFromPropertyTreesInternal(
    LayerType* layer,
    const TransformTree& tree) {
  gfx::Transform xform(1, 0, 0, 1, layer->offset_to_transform_parent().x(),
                       layer->offset_to_transform_parent().y());
  if (layer->transform_tree_index() >= 0) {
    gfx::Transform ssxform =
        tree.Node(layer->transform_tree_index())->data.to_screen;
    xform.ConcatTransform(ssxform);
    if (layer->should_flatten_transform_from_property_tree())
      xform.FlattenTo2d();
  }
  xform.Scale(1.0 / layer->contents_scale_x(), 1.0 / layer->contents_scale_y());
  return xform;
}

gfx::Transform ScreenSpaceTransformFromPropertyTrees(
    const Layer* layer,
    const TransformTree& tree) {
  return ScreenSpaceTransformFromPropertyTreesInternal(layer, tree);
}

gfx::Transform ScreenSpaceTransformFromPropertyTrees(
    const LayerImpl* layer,
    const TransformTree& tree) {
  return ScreenSpaceTransformFromPropertyTreesInternal(layer, tree);
}

template <typename LayerType>
float DrawOpacityFromPropertyTreesInternal(LayerType layer,
                                           const OpacityTree& tree) {
  if (!layer->render_target())
    return 0.f;

  const OpacityNode* target_node =
      tree.Node(layer->render_target()->opacity_tree_index());
  const OpacityNode* node = tree.Node(layer->opacity_tree_index());
  if (node == target_node)
    return 1.f;

  float draw_opacity = 1.f;
  while (node != target_node) {
    draw_opacity *= node->data;
    node = tree.parent(node);
  }
  return draw_opacity;
}

float DrawOpacityFromPropertyTrees(const Layer* layer,
                                   const OpacityTree& tree) {
  return DrawOpacityFromPropertyTreesInternal(layer, tree);
}

float DrawOpacityFromPropertyTrees(const LayerImpl* layer,
                                   const OpacityTree& tree) {
  return DrawOpacityFromPropertyTreesInternal(layer, tree);
}

}  // namespace cc
