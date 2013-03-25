// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/layer_iterator.h"

#include "cc/layers/layer.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/render_surface.h"
#include "cc/layers/render_surface_impl.h"

namespace cc {

template <typename LayerType,
          typename LayerList,
          typename RenderSurfaceType,
          typename ActionType>
void LayerIteratorActions::BackToFront::Begin(
    LayerIterator<LayerType, LayerList, RenderSurfaceType, ActionType>* it) {
  it->target_render_surface_layer_index_ = 0;
  it->current_layer_index_ =
      LayerIteratorValue::kLayerIndexRepresentingTargetRenderSurface;

  highest_target_render_surface_layer_ = 0;
}

template <typename LayerType,
          typename LayerList,
          typename RenderSurfaceType,
          typename ActionType>
void LayerIteratorActions::BackToFront::End(
    LayerIterator<LayerType, LayerList, RenderSurfaceType, ActionType>* it) {
  it->target_render_surface_layer_index_ =
      LayerIteratorValue::kInvalidTargetRenderSurfaceLayerIndex;
  it->current_layer_index_ = 0;
}

template <typename LayerType,
          typename LayerList,
          typename RenderSurfaceType,
          typename ActionType>
void LayerIteratorActions::BackToFront::Next(
    LayerIterator<LayerType, LayerList, RenderSurfaceType, ActionType>* it) {
  // If the current layer has a RS, move to its layer list. Otherwise,
  // visit the next layer in the current RS layer list.
  if (it->current_layer_represents_contributing_render_surface()) {
    // Save our position in the child_layers list for the RenderSurface,
    // then jump to the next RenderSurface. Save where we
    // came from in the next RenderSurface so we can get back to it.
    it->target_render_surface()->current_layer_index_history_ =
        it->current_layer_index_;
    int previous_target_render_surface_layer =
        it->target_render_surface_layer_index_;

    it->target_render_surface_layer_index_ =
        ++highest_target_render_surface_layer_;
    it->current_layer_index_ =
        LayerIteratorValue::kLayerIndexRepresentingTargetRenderSurface;

    it->target_render_surface()->target_render_surface_layer_index_history_ =
        previous_target_render_surface_layer;
  } else {
    ++it->current_layer_index_;

    int target_render_surface_num_children =
        it->target_render_surface_children().size();
    while (it->current_layer_index_ == target_render_surface_num_children) {
      // Jump back to the previous RenderSurface,
      // and get back the position where we were in that list,
      // and move to the next position there.
      if (!it->target_render_surface_layer_index_) {
        // End of the list
        it->target_render_surface_layer_index_ =
            LayerIteratorValue::kInvalidTargetRenderSurfaceLayerIndex;
        it->current_layer_index_ = 0;
        return;
      }
      it->target_render_surface_layer_index_ = it->target_render_surface()
          ->target_render_surface_layer_index_history_;
      it->current_layer_index_ =
          it->target_render_surface()->current_layer_index_history_ + 1;

      target_render_surface_num_children =
          it->target_render_surface_children().size();
    }
  }
}

template <typename LayerType,
          typename LayerList,
          typename RenderSurfaceType,
          typename ActionType>
void LayerIteratorActions::FrontToBack::Begin(
    LayerIterator<LayerType, LayerList, RenderSurfaceType, ActionType>* it) {
  it->target_render_surface_layer_index_ = 0;
  it->current_layer_index_ = it->target_render_surface_children().size() - 1;
  GoToHighestInSubtree(it);
}

template <typename LayerType,
          typename LayerList,
          typename RenderSurfaceType,
          typename ActionType>
void LayerIteratorActions::FrontToBack::End(
    LayerIterator<LayerType, LayerList, RenderSurfaceType, ActionType>* it) {
  it->target_render_surface_layer_index_ =
      LayerIteratorValue::kInvalidTargetRenderSurfaceLayerIndex;
  it->current_layer_index_ = 0;
}

template <typename LayerType,
          typename LayerList,
          typename RenderSurfaceType,
          typename ActionType>
void LayerIteratorActions::FrontToBack::Next(
    LayerIterator<LayerType, LayerList, RenderSurfaceType, ActionType>* it) {
  // Moves to the previous layer in the current RS layer list.
  // Then we check if the new current layer has its own RS,
  // in which case there are things in that RS layer list that are higher,
  // so we find the highest layer in that subtree.
  // If we move back past the front of the list,
  // we jump up to the previous RS layer list, picking up again where we
  // had previously recursed into the current RS layer list.

  if (!it->current_layer_represents_target_render_surface()) {
    // Subtracting one here will eventually cause the current layer
    // to become that layer representing the target render surface.
    --it->current_layer_index_;
    GoToHighestInSubtree(it);
  } else {
    while (it->current_layer_represents_target_render_surface()) {
      if (!it->target_render_surface_layer_index_) {
        // End of the list
        it->target_render_surface_layer_index_ =
            LayerIteratorValue::kInvalidTargetRenderSurfaceLayerIndex;
        it->current_layer_index_ = 0;
        return;
      }
      it->target_render_surface_layer_index_ = it->target_render_surface()
          ->target_render_surface_layer_index_history_;
      it->current_layer_index_ =
          it->target_render_surface()->current_layer_index_history_;
    }
  }
}

template <typename LayerType,
          typename LayerList,
          typename RenderSurfaceType,
          typename ActionType>
void LayerIteratorActions::FrontToBack::GoToHighestInSubtree(
    LayerIterator<LayerType, LayerList, RenderSurfaceType, ActionType>* it) {
  if (it->current_layer_represents_target_render_surface())
    return;
  while (it->current_layer_represents_contributing_render_surface()) {
    // Save where we were in the current target surface, move to the next one,
    // and save the target surface that we came from there
    // so we can go back to it.
    it->target_render_surface()->current_layer_index_history_ =
        it->current_layer_index_;
    int previous_target_render_surface_layer =
        it->target_render_surface_layer_index_;

    for (LayerType* layer = it->current_layer();
         it->target_render_surface_layer() != layer;
         ++it->target_render_surface_layer_index_) {
    }
    it->current_layer_index_ = it->target_render_surface_children().size() - 1;

    it->target_render_surface()->target_render_surface_layer_index_history_ =
        previous_target_render_surface_layer;
  }
}

typedef std::vector<scoped_refptr<Layer> > LayerList;
typedef std::vector<LayerImpl*> LayerImplList;

// Declare each of the above functions for Layer and LayerImpl classes
// so that they are linked.
template CC_EXPORT void LayerIteratorActions::BackToFront::Begin(
    LayerIterator<Layer, LayerList, RenderSurface, BackToFront>* it);
template CC_EXPORT void LayerIteratorActions::BackToFront::End(
    LayerIterator<Layer, LayerList, RenderSurface, BackToFront>* it);
template CC_EXPORT void LayerIteratorActions::BackToFront::Next(
    LayerIterator<Layer, LayerList, RenderSurface, BackToFront>* it);

template CC_EXPORT void LayerIteratorActions::BackToFront::Begin(
    LayerIterator<LayerImpl, LayerImplList, RenderSurfaceImpl, BackToFront>*
        it);
template CC_EXPORT void LayerIteratorActions::BackToFront::End(
    LayerIterator<LayerImpl, LayerImplList, RenderSurfaceImpl, BackToFront>*
        it);
template CC_EXPORT void LayerIteratorActions::BackToFront::Next(
    LayerIterator<LayerImpl, LayerImplList, RenderSurfaceImpl, BackToFront>*
        it);

template CC_EXPORT void LayerIteratorActions::FrontToBack::Next(
    LayerIterator<Layer, LayerList, RenderSurface, FrontToBack>* it);
template CC_EXPORT void LayerIteratorActions::FrontToBack::End(
    LayerIterator<Layer, LayerList, RenderSurface, FrontToBack>* it);
template CC_EXPORT void LayerIteratorActions::FrontToBack::Begin(
    LayerIterator<Layer, LayerList, RenderSurface, FrontToBack>* it);
template CC_EXPORT void LayerIteratorActions::FrontToBack::GoToHighestInSubtree(
    LayerIterator<Layer, LayerList, RenderSurface, FrontToBack>* it);

template CC_EXPORT void LayerIteratorActions::FrontToBack::Next(
    LayerIterator<LayerImpl, LayerImplList, RenderSurfaceImpl, FrontToBack>*
        it);
template CC_EXPORT void LayerIteratorActions::FrontToBack::End(
    LayerIterator<LayerImpl, LayerImplList, RenderSurfaceImpl, FrontToBack>*
        it);
template CC_EXPORT void LayerIteratorActions::FrontToBack::Begin(
    LayerIterator<LayerImpl, LayerImplList, RenderSurfaceImpl, FrontToBack>*
        it);
template CC_EXPORT void LayerIteratorActions::FrontToBack::GoToHighestInSubtree(
    LayerIterator<LayerImpl, LayerImplList, RenderSurfaceImpl, FrontToBack>*
        it);

}  // namespace cc
