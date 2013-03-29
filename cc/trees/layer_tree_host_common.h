// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_LAYER_TREE_HOST_COMMON_H_
#define CC_TREES_LAYER_TREE_HOST_COMMON_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "cc/base/cc_export.h"
#include "cc/base/scoped_ptr_vector.h"
#include "cc/layers/layer_lists.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/transform.h"
#include "ui/gfx/vector2d.h"

namespace cc {

class LayerImpl;
class Layer;

class CC_EXPORT LayerTreeHostCommon {
 public:
  static gfx::Rect CalculateVisibleRect(gfx::Rect target_surface_rect,
                                        gfx::Rect layer_bound_rect,
                                        const gfx::Transform& transform);

  static void CalculateDrawProperties(
      Layer* root_layer,
      gfx::Size device_viewport_size,
      float device_scale_factor,
      float page_scale_factor,
      int max_texture_size,
      bool can_use_lcd_text,
      LayerList* render_surface_layer_list);
  static void CalculateDrawProperties(
      LayerImpl* root_layer,
      gfx::Size device_viewport_size,
      float device_scale_factor,
      float page_scale_factor,
      int max_texture_size,
      bool can_use_lcd_text,
      LayerImplList* render_surface_layer_list,
      bool update_tile_priorities);

  // Performs hit testing for a given render_surface_layer_list.
  static LayerImpl* FindLayerThatIsHitByPoint(
      gfx::PointF screen_space_point,
      const LayerImplList& render_surface_layer_list);

  static LayerImpl* FindLayerThatIsHitByPointInTouchHandlerRegion(
      gfx::PointF screen_space_point,
      const LayerImplList& render_surface_layer_list);

  static bool LayerHasTouchEventHandlersAt(gfx::PointF screen_space_point,
                                           LayerImpl* layer_impl);

  template <typename LayerType>
  static bool RenderSurfaceContributesToTarget(LayerType*,
                                               int target_surface_layer_id);

  template <class Function, typename LayerType>
  static void CallFunctionForSubtree(LayerType* root_layer);

  // Returns a layer with the given id if one exists in the subtree starting
  // from the given root layer (including mask and replica layers).
  template <typename LayerType>
  static LayerType* FindLayerInSubtree(LayerType* root_layer, int layer_id);

  static Layer* get_child_as_raw_ptr(
      const LayerList& children,
      size_t index) {
    return children[index].get();
  }

  static LayerImpl* get_child_as_raw_ptr(
      const OwnedLayerImplList& children,
      size_t index) {
    return children[index];
  }

  struct ScrollUpdateInfo {
    int layer_id;
    gfx::Vector2d scroll_delta;
  };
};

struct CC_EXPORT ScrollAndScaleSet {
  ScrollAndScaleSet();
  ~ScrollAndScaleSet();

  std::vector<LayerTreeHostCommon::ScrollUpdateInfo> scrolls;
  float page_scale_delta;
};

template <typename LayerType>
bool LayerTreeHostCommon::RenderSurfaceContributesToTarget(
    LayerType* layer,
    int target_surface_layer_id) {
  // A layer will either contribute its own content, or its render surface's
  // content, to the target surface. The layer contributes its surface's content
  // when both the following are true:
  //  (1) The layer actually has a render surface, and
  //  (2) The layer's render surface is not the same as the target surface.
  //
  // Otherwise, the layer just contributes itself to the target surface.

  return layer->render_surface() && layer->id() != target_surface_layer_id;
}

template <typename LayerType>
LayerType* LayerTreeHostCommon::FindLayerInSubtree(LayerType* root_layer,
                                                   int layer_id) {
  if (root_layer->id() == layer_id)
    return root_layer;

  if (root_layer->mask_layer() && root_layer->mask_layer()->id() == layer_id)
    return root_layer->mask_layer();

  if (root_layer->replica_layer() &&
      root_layer->replica_layer()->id() == layer_id)
    return root_layer->replica_layer();

  for (size_t i = 0; i < root_layer->children().size(); ++i) {
    if (LayerType* found = FindLayerInSubtree(
            get_child_as_raw_ptr(root_layer->children(), i), layer_id))
      return found;
  }
  return NULL;
}

template <class Function, typename LayerType>
void LayerTreeHostCommon::CallFunctionForSubtree(LayerType* root_layer) {
  Function()(root_layer);

  if (LayerType* mask_layer = root_layer->mask_layer())
    Function()(mask_layer);
  if (LayerType* replica_layer = root_layer->replica_layer()) {
    Function()(replica_layer);
    if (LayerType* mask_layer = replica_layer->mask_layer())
      Function()(mask_layer);
  }

  for (size_t i = 0; i < root_layer->children().size(); ++i) {
    CallFunctionForSubtree<Function>(
        get_child_as_raw_ptr(root_layer->children(), i));
  }
}

}  // namespace cc

#endif  // CC_TREES_LAYER_TREE_HOST_COMMON_H_
