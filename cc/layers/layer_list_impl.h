// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_LAYER_LIST_IMPL_H_
#define CC_LAYERS_LAYER_LIST_IMPL_H_

#include <stdint.h>
#include <unordered_map>

#include "cc/base/cc_export.h"
#include "cc/layers/layer_lists.h"

namespace cc {
class AnimationRegistrar;
class LayerTreeHostImpl;
typedef LayerTreeHostImpl LayerListHostImpl;

// This class will eventually replace LayerTreeImpl.
//
// There is certainly some unfortunate ambiguity with LayerImplList and
// OwnedLayerImplList, but this should be temporary. OwnedLayerImplList is used
// solely for the children of a LayerImpl and this will cease to be a thing as
// we move away from the layer hierarchy. The LayerImplList, however, does get
// used a fair bit to describe a list of LayerImpl*'s. I.e., an unowned layer
// list. In the medium term, I'd like to rename this LayerImplPtrList and, in
// the fullness of time, a LayerPtrList once Layer disappears.
class CC_EXPORT LayerListImpl {
 public:
  explicit LayerListImpl(LayerListHostImpl* host_impl);
  ~LayerListImpl();

  AnimationRegistrar* GetAnimationRegistrar() const;

  LayerImpl* LayerById(int id) const;

  // These should be called by LayerImpl's ctor/dtor.
  void RegisterLayer(LayerImpl* layer);
  void UnregisterLayer(LayerImpl* layer);

  size_t NumLayers();

  LayerImpl* FindActiveLayerById(int id);
  LayerImpl* FindPendingLayerById(int id);

  // TODO(vollick): once we've built compositor worker on top of animations,
  // then this association of id to element layers will not be necessary. The
  // association will instead be maintained via the animation.
  void AddToElementMap(LayerImpl* layer);
  void RemoveFromElementMap(LayerImpl* layer);

  // TODO(thakis): Consider marking this CC_EXPORT once we understand
  // http://crbug.com/575700 better.
  struct ElementLayers {
    // Transform and opacity mutations apply to this layer.
    LayerImpl* main = nullptr;
    // Scroll mutations apply to this layer.
    LayerImpl* scroll = nullptr;
  };

  // TODO(vollick): this should be removed as well.
  ElementLayers GetMutableLayers(uint64_t element_id);

 private:
  bool IsActiveList() const;
  bool IsPendingList() const;

  // TODO(vollick): Remove after compositor worker is built on animations.
  using ElementLayersMap = std::unordered_map<uint64_t, ElementLayers>;
  ElementLayersMap element_layers_map_;

  using LayerIdMap = std::unordered_map<int, LayerImpl*>;
  LayerIdMap layer_id_map_;

  LayerListHostImpl* layer_list_host_;
  scoped_ptr<OwnedLayerImplList> layer_;
};

}  // namespace cc

#endif  // CC_LAYERS_LAYER_LIST_IMPL_H_
