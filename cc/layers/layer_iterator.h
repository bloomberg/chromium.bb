// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_LAYER_ITERATOR_H_
#define CC_LAYERS_LAYER_ITERATOR_H_

#include "base/memory/ref_counted.h"
#include "cc/base/cc_export.h"
#include "cc/trees/layer_tree_host_common.h"

namespace cc {

// These classes provide means to iterate over the
// RenderSurface-Layer tree.

// Example code follows, for a tree of Layer/RenderSurface objects.
// See below for details.
//
// void DoStuffOnLayers(
//     const LayerList& render_surface_layer_list) {
//   typedef LayerIterator<Layer,
//                         LayerList,
//                         RenderSurface,
//                         LayerIteratorActions::FrontToBack>
//       LayerIteratorType;
//
//   LayerIteratorType end =
//       LayerIteratorType::End(&render_surface_layer_list);
//   for (LayerIteratorType
//            it = LayerIteratorType::Begin(&render_surface_layer_list);
//        it != end;
//        ++it) {
//     // Only one of these will be true
//     if (it.represents_target_render_surface())
//       foo(*it);  // *it is a layer representing a target RenderSurface
//     if (it.represents_contributing_render_surface())
//       bar(*it);  // *it is a layer representing a RenderSurface that
//                  // contributes to the layer's target RenderSurface
//     if (it.represents_itself())
//       baz(*it);  // *it is a layer representing itself,
//                  // as it contributes to its own target RenderSurface
//   }
// }

// A RenderSurface R may be referred to in one of two different contexts.
// One RenderSurface is "current" at any time, for whatever operation
// is being performed. This current surface is referred to as a target surface.
// For example, when R is being painted it would be the target surface.
// Once R has been painted, its contents may be included into another
// surface S. While S is considered the target surface when it is being
// painted, R is called a contributing surface in this context as it
// contributes to the content of the target surface S.
//
// The iterator's current position in the tree always points to some layer.
// The state of the iterator indicates the role of the layer,
// and will be one of the following three states.
// A single layer L will appear in the iteration process in at least one,
// and possibly all, of these states.
// 1. Representing the target surface: The iterator in this state,
// pointing at layer L, indicates that the target RenderSurface
// is now the surface owned by L. This will occur exactly once for each
// RenderSurface in the tree.
// 2. Representing a contributing surface: The iterator in this state,
// pointing at layer L, refers to the RenderSurface owned
// by L as a contributing surface, without changing the current
// target RenderSurface.
// 3. Representing itself: The iterator in this state, pointing at layer L,
// refers to the layer itself, as a child of the
// current target RenderSurface.
//
// The BackToFront iterator will return a layer representing the target surface
// before returning layers representing themselves as children of the current
// target surface. Whereas the FrontToBack ordering will iterate over children
// layers of a surface before the layer representing the surface
// as a target surface.
//
// To use the iterators:
//
// Create a stepping iterator and end iterator by calling
// LayerIterator::Begin() and LayerIterator::End() and passing in the
// list of layers owning target RenderSurfaces. Step through the tree
// by incrementing the stepping iterator while it is != to
// the end iterator. At each step the iterator knows what the layer
// is representing, and you can query the iterator to decide
// what actions to perform with the layer given what it represents.

////////////////////////////////////////////////////////////////////////////////

// Non-templated constants
struct LayerIteratorValue {
  static const int kInvalidTargetRenderSurfaceLayerIndex = -1;
  // This must be -1 since the iterator action code assumes that this value can
  // be reached by subtracting one from the position of the first layer in the
  // current target surface's child layer list, which is 0.
  static const int kLayerIndexRepresentingTargetRenderSurface = -1;
};

// The position of a layer iterator that is independent
// of its many template types.
template <typename LayerType> struct LayerIteratorPosition {
  bool represents_target_render_surface;
  bool represents_contributing_render_surface;
  bool represents_itself;
  LayerType* target_render_surface_layer;
  LayerType* current_layer;
};

// An iterator class for walking over layers in the
// RenderSurface-Layer tree.
template <typename LayerType,
          typename LayerList,
          typename RenderSurfaceType,
          typename IteratorActionType>
class LayerIterator {
  typedef LayerIterator<LayerType,
                        LayerList,
                        RenderSurfaceType,
                        IteratorActionType> LayerIteratorType;

 public:
  LayerIterator() : render_surface_layer_list_(NULL) {}

  static LayerIteratorType Begin(const LayerList* render_surface_layer_list) {
    return LayerIteratorType(render_surface_layer_list, true);
  }
  static LayerIteratorType End(const LayerList* render_surface_layer_list) {
    return LayerIteratorType(render_surface_layer_list, false);
  }

  LayerIteratorType& operator++() {
    actions_.Next(this);
    return *this;
  }
  bool operator==(const LayerIterator& other) const {
    return target_render_surface_layer_index_ ==
           other.target_render_surface_layer_index_ &&
           current_layer_index_ == other.current_layer_index_;
  }
  bool operator!=(const LayerIteratorType& other) const {
    return !(*this == other);
  }

  LayerType* operator->() const { return current_layer(); }
  LayerType* operator*() const { return current_layer(); }

  bool represents_target_render_surface() const {
    return current_layer_represents_target_render_surface();
  }
  bool represents_contributing_render_surface() const {
    return !represents_target_render_surface() &&
           current_layer_represents_contributing_render_surface();
  }
  bool represents_itself() const {
    return !represents_target_render_surface() &&
           !represents_contributing_render_surface();
  }

  LayerType* target_render_surface_layer() const {
    return get_raw_ptr(
        (*render_surface_layer_list_)[target_render_surface_layer_index_]);
  }

  operator const LayerIteratorPosition<LayerType>() const {
    LayerIteratorPosition<LayerType> position;
    position.represents_target_render_surface =
        represents_target_render_surface();
    position.represents_contributing_render_surface =
        represents_contributing_render_surface();
    position.represents_itself = represents_itself();
    position.target_render_surface_layer = target_render_surface_layer();
    position.current_layer = current_layer();
    return position;
  }

 private:
  LayerIterator(const LayerList* render_surface_layer_list, bool start)
      : render_surface_layer_list_(render_surface_layer_list),
        target_render_surface_layer_index_(0) {
    for (size_t i = 0; i < render_surface_layer_list->size(); ++i) {
      if (!(*render_surface_layer_list)[i]->render_surface()) {
        NOTREACHED();
        actions_.End(this);
        return;
      }
    }

    if (start && !render_surface_layer_list->empty())
      actions_.Begin(this);
    else
      actions_.End(this);
  }

  inline static Layer* get_raw_ptr(const scoped_refptr<Layer>& ptr) {
    return ptr.get();
  }
  inline static LayerImpl* get_raw_ptr(LayerImpl* ptr) { return ptr; }

  inline LayerType* current_layer() const {
    return current_layer_represents_target_render_surface()
           ? target_render_surface_layer()
           : get_raw_ptr(
                 target_render_surface_children()[current_layer_index_]);
  }

  inline bool current_layer_represents_contributing_render_surface() const {
    return LayerTreeHostCommon::RenderSurfaceContributesToTarget<LayerType>(
        current_layer(), target_render_surface_layer()->id());
  }
  inline bool current_layer_represents_target_render_surface() const {
    return current_layer_index_ ==
           LayerIteratorValue::kLayerIndexRepresentingTargetRenderSurface;
  }

  inline RenderSurfaceType* target_render_surface() const {
    return target_render_surface_layer()->render_surface();
  }
  inline const LayerList& target_render_surface_children() const {
    return target_render_surface()->layer_list();
  }

  IteratorActionType actions_;
  const LayerList* render_surface_layer_list_;

  // The iterator's current position.

  // A position in the render_surface_layer_list. This points to a layer which
  // owns the current target surface. This is a value from 0 to n-1
  // (n = size of render_surface_layer_list = number of surfaces).
  // A value outside of this range
  // (for example, LayerIteratorValue::kInvalidTargetRenderSurfaceLayerIndex)
  // is used to indicate a position outside the bounds of the tree.
  int target_render_surface_layer_index_;
  // A position in the list of layers that are children of the
  // current target surface. When pointing to one of these layers,
  // this is a value from 0 to n-1 (n = number of children).
  // Since the iterator must also stop at the layers representing
  // the target surface, this is done by setting the current_layerIndex
  // to a value of LayerIteratorValue::LayerRepresentingTargetRenderSurface.
  int current_layer_index_;

  friend struct LayerIteratorActions;
};

// Orderings for iterating over the RenderSurface-Layer tree.
struct CC_EXPORT LayerIteratorActions {
  // Walks layers sorted by z-order from back to front.
  class CC_EXPORT BackToFront {
   public:
    template <typename LayerType,
              typename LayerList,
              typename RenderSurfaceType,
              typename ActionType>
    void Begin(
        LayerIterator<LayerType, LayerList, RenderSurfaceType, ActionType>* it);

    template <typename LayerType,
              typename LayerList,
              typename RenderSurfaceType,
              typename ActionType>
    void End(
        LayerIterator<LayerType, LayerList, RenderSurfaceType, ActionType>* it);

    template <typename LayerType,
              typename LayerList,
              typename RenderSurfaceType,
              typename ActionType>
    void Next(
        LayerIterator<LayerType, LayerList, RenderSurfaceType, ActionType>* it);

   private:
    int highest_target_render_surface_layer_;
  };

  // Walks layers sorted by z-order from front to back
  class CC_EXPORT FrontToBack {
   public:
    template <typename LayerType,
              typename LayerList,
              typename RenderSurfaceType,
              typename ActionType>
    void Begin(
        LayerIterator<LayerType, LayerList, RenderSurfaceType, ActionType>* it);

    template <typename LayerType,
              typename LayerList,
              typename RenderSurfaceType,
              typename ActionType>
    void End(
        LayerIterator<LayerType, LayerList, RenderSurfaceType, ActionType>* it);

    template <typename LayerType,
              typename LayerList,
              typename RenderSurfaceType,
              typename ActionType>
    void Next(
        LayerIterator<LayerType, LayerList, RenderSurfaceType, ActionType>* it);

   private:
    template <typename LayerType,
              typename LayerList,
              typename RenderSurfaceType,
              typename ActionType>
    void GoToHighestInSubtree(
        LayerIterator<LayerType, LayerList, RenderSurfaceType, ActionType>* it);
  };
};

}  // namespace cc

#endif  // CC_LAYERS_LAYER_ITERATOR_H_
