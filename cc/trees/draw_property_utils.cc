// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/draw_property_utils.h"

#include <vector>

#include "cc/base/math_util.h"
#include "cc/layers/layer.h"
#include "cc/trees/property_tree.h"
#include "cc/trees/property_tree_builder.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace cc {

namespace {

void CalculateVisibleRects(
    const std::vector<Layer*>& layers_that_need_visible_rects,
    const ClipTree& clip_tree,
    const TransformTree& transform_tree) {
  for (size_t i = 0; i < layers_that_need_visible_rects.size(); ++i) {
    Layer* layer = layers_that_need_visible_rects[i];

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
      const TransformNode* target_node =
          transform_tree.Node(transform_node->data.content_target_id);

      gfx::Transform clip_to_target;
      gfx::Transform content_to_target;
      gfx::Transform target_to_content;
      gfx::Transform target_to_layer;

      bool success =
          transform_tree.ComputeTransform(clip_transform_node->id,
                                          target_node->id, &clip_to_target) &&
          transform_tree.ComputeTransform(transform_node->id, target_node->id,
                                          &content_to_target) &&
          transform_tree.ComputeTransform(target_node->id, transform_node->id,
                                          &target_to_layer);

      // This should only fail if we somehow got here with a singular ancestor.
      DCHECK(success);

      target_to_content.Scale(contents_scale_x, contents_scale_y);
      target_to_content.Translate(-layer->offset_to_transform_parent().x(),
                                  -layer->offset_to_transform_parent().y());
      target_to_content.PreconcatTransform(target_to_layer);

      content_to_target.Translate(layer->offset_to_transform_parent().x(),
                                  layer->offset_to_transform_parent().y());
      content_to_target.Scale(1.0 / contents_scale_x, 1.0 / contents_scale_y);

      gfx::Rect layer_content_rect = gfx::Rect(layer_content_bounds);
      gfx::RectF layer_content_bounds_in_target_space =
          MathUtil::MapClippedRect(content_to_target, layer_content_rect);
      gfx::RectF clip_rect_in_target_space;
      if (target_node->id > clip_node->id) {
        clip_rect_in_target_space = MathUtil::ProjectClippedRect(
            clip_to_target, clip_node->data.combined_clip);
      } else {
        clip_rect_in_target_space = MathUtil::MapClippedRect(
            clip_to_target, clip_node->data.combined_clip);
      }

      clip_rect_in_target_space.Intersect(layer_content_bounds_in_target_space);

      gfx::Rect visible_rect =
          gfx::ToEnclosingRect(MathUtil::ProjectClippedRect(
              target_to_content, clip_rect_in_target_space));

      visible_rect.Intersect(gfx::Rect(layer_content_bounds));

      layer->set_visible_rect_from_property_trees(visible_rect);
    } else {
      layer->set_visible_rect_from_property_trees(
          gfx::Rect(layer_content_bounds));
    }
  }
}

static bool IsRootLayerOfNewRenderingContext(Layer* layer) {
  if (layer->parent())
    return !layer->parent()->Is3dSorted() && layer->Is3dSorted();
  return layer->Is3dSorted();
}

static inline bool LayerIsInExisting3DRenderingContext(Layer* layer) {
  return layer->Is3dSorted() && layer->parent() &&
         layer->parent()->Is3dSorted();
}

static bool TransformToScreenIsKnown(Layer* layer, const TransformTree& tree) {
  const TransformNode* node = tree.Node(layer->transform_tree_index());
  return !node->data.to_screen_is_animated;
}

static bool IsLayerBackFaceExposed(Layer* layer, const TransformTree& tree) {
  if (!TransformToScreenIsKnown(layer, tree))
    return false;
  if (LayerIsInExisting3DRenderingContext(layer))
    return layer->draw_transform_from_property_trees(tree).IsBackFaceVisible();
  return layer->transform().IsBackFaceVisible();
}

static bool IsSurfaceBackFaceExposed(Layer* layer,
                                     const TransformTree& tree) {
  if (!TransformToScreenIsKnown(layer, tree))
    return false;
  if (LayerIsInExisting3DRenderingContext(layer))
    return layer->draw_transform_from_property_trees(tree).IsBackFaceVisible();

  if (IsRootLayerOfNewRenderingContext(layer))
    return layer->transform().IsBackFaceVisible();

  // If the render_surface is not part of a new or existing rendering context,
  // then the layers that contribute to this surface will decide back-face
  // visibility for themselves.
  return false;
}

static bool HasSingularTransform(Layer* layer, const TransformTree& tree) {
  const TransformNode* node = tree.Node(layer->transform_tree_index());
  return !node->data.is_invertible || !node->data.ancestors_are_invertible;
}

static bool IsBackFaceInvisible(Layer* layer, const TransformTree& tree) {
  Layer* backface_test_layer = layer;
  if (layer->use_parent_backface_visibility()) {
    DCHECK(layer->parent());
    DCHECK(!layer->parent()->use_parent_backface_visibility());
    backface_test_layer = layer->parent();
  }
  return !backface_test_layer->double_sided() &&
         IsLayerBackFaceExposed(backface_test_layer, tree);
}

static bool IsInvisibleDueToTransform(Layer* layer, const TransformTree& tree) {
  return HasSingularTransform(layer, tree) || IsBackFaceInvisible(layer, tree);
}

void FindLayersThatNeedVisibleRects(Layer* layer,
                                    const TransformTree& tree,
                                    bool subtree_is_visible_from_ancestor,
                                    std::vector<Layer*>* layers_to_update) {
  const bool layer_is_invisible =
      (!layer->opacity() && !layer->OpacityIsAnimating() &&
       !layer->OpacityCanAnimateOnImplThread());
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
    FindLayersThatNeedVisibleRects(layer->children()[i].get(),
                                   tree,
                                   layer_is_drawn,
                                   layers_to_update);
  }
}

}  // namespace

void ComputeClips(ClipTree* clip_tree, const TransformTree& transform_tree) {
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

    bool success =
        transform_tree.ComputeTransform(parent_transform_node->id,
                                        clip_node->data.target_id,
                                        &parent_to_target) &&
        transform_tree.ComputeTransform(
            transform_node->id, clip_node->data.target_id, &clip_to_target) &&
        transform_tree.ComputeTransform(clip_node->data.target_id,
                                        transform_node->id, &target_to_clip);

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
}

void ComputeTransforms(TransformTree* transform_tree) {
  for (int i = 1; i < static_cast<int>(transform_tree->size()); ++i)
    transform_tree->UpdateTransforms(i);
}

void ComputeVisibleRectsUsingPropertyTrees(
    Layer* root_layer,
    const Layer* page_scale_layer,
    float page_scale_factor,
    float device_scale_factor,
    const gfx::Rect& viewport,
    const gfx::Transform& device_transform,
    TransformTree* transform_tree,
    ClipTree* clip_tree,
    OpacityTree* opacity_tree) {
  PropertyTreeBuilder::BuildPropertyTrees(
      root_layer, page_scale_layer, page_scale_factor, device_scale_factor,
      viewport, device_transform, transform_tree, clip_tree, opacity_tree);
  ComputeTransforms(transform_tree);
  ComputeClips(clip_tree, *transform_tree);

  std::vector<Layer*> layers_to_update;
  const bool subtree_is_visible_from_ancestor = true;
  FindLayersThatNeedVisibleRects(root_layer, *transform_tree,
                                 subtree_is_visible_from_ancestor,
                                 &layers_to_update);
  CalculateVisibleRects(layers_to_update, *clip_tree, *transform_tree);
}

}  // namespace cc
