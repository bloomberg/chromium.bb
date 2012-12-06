// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYER_TREE_IMPL_H_
#define CC_LAYER_TREE_IMPL_H_

#include "cc/layer_impl.h"

namespace cc {

class LayerTreeHostImpl;
class LayerTreeImpl;
class HeadsUpDisplayLayerImpl;

class CC_EXPORT LayerTreeImplClient {
 public:
  virtual void OnCanDrawStateChangedForTree(LayerTreeImpl*)  = 0;

 protected:
  virtual ~LayerTreeImplClient() {}
};

class CC_EXPORT LayerTreeImpl {
 public:
  static scoped_ptr<LayerTreeImpl> create(LayerTreeImplClient* client)
  {
    return make_scoped_ptr(new LayerTreeImpl(client));
  }
  virtual ~LayerTreeImpl();

  LayerImpl* RootLayer() const { return root_layer_.get(); }
  void SetRootLayer(scoped_ptr<LayerImpl>);
  scoped_ptr<LayerImpl> DetachLayerTree();

  int source_frame_number() const { return source_frame_number_; }
  void set_source_frame_number(int frame_number) { source_frame_number_ = frame_number; }

  HeadsUpDisplayLayerImpl* hud_layer() { return hud_layer_; }
  void set_hud_layer(HeadsUpDisplayLayerImpl* layer_impl) { hud_layer_ = layer_impl; }

  LayerImpl* root_scroll_layer() { return root_scroll_layer_; }
  void set_root_scroll_layer(LayerImpl* layer_impl) { root_scroll_layer_ = layer_impl; }

  LayerImpl* currently_scrolling_layer() { return currently_scrolling_layer_; }
  void set_currently_scrolling_layer(LayerImpl* layer_impl) { currently_scrolling_layer_ = layer_impl; }

  void ClearCurrentlyScrollingLayer();

protected:
  LayerTreeImpl(LayerTreeImplClient* client);

  LayerTreeImplClient* client_;
  int source_frame_number_;
  scoped_ptr<LayerImpl> root_layer_;
  HeadsUpDisplayLayerImpl* hud_layer_;
  LayerImpl* root_scroll_layer_;
  LayerImpl* currently_scrolling_layer_;

  // Persisted state
  int scrolling_layer_id_from_previous_tree_;

  DISALLOW_COPY_AND_ASSIGN(LayerTreeImpl);
};

}

#endif  // CC_LAYER_TREE_IMPL_H_
