// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYER_TREE_IMPL_H_
#define CC_LAYER_TREE_IMPL_H_

#include "base/hash_tables.h"
#include "cc/layer_impl.h"

#if defined(COMPILER_GCC)
namespace BASE_HASH_NAMESPACE {
template<>
struct hash<cc::LayerImpl*> {
  size_t operator()(cc::LayerImpl* ptr) const {
    return hash<size_t>()(reinterpret_cast<size_t>(ptr));
  }
};
} // namespace BASE_HASH_NAMESPACE
#endif // COMPILER

namespace cc {

class DebugRectHistory;
class FrameRateCounter;
class HeadsUpDisplayLayerImpl;
class LayerTreeDebugState;
class LayerTreeHostImpl;
class LayerTreeImpl;
class LayerTreeSettings;
class OutputSurface;
class ResourceProvider;
class TileManager;

class CC_EXPORT LayerTreeImpl {
 public:
  static scoped_ptr<LayerTreeImpl> create(LayerTreeHostImpl* layer_tree_host_impl)
  {
    return make_scoped_ptr(new LayerTreeImpl(layer_tree_host_impl));
  }
  virtual ~LayerTreeImpl();

  // Methods called by the layer tree that pass-through to LHTI.
  // ---------------------------------------------------------------------------
  const LayerTreeSettings& settings() const;
  OutputSurface* output_surface() const;
  ResourceProvider* resource_provider() const;
  TileManager* tile_manager() const;
  FrameRateCounter* frame_rate_counter() const;

  // Tree specific methods exposed to layer-impl tree.
  // ---------------------------------------------------------------------------
  void SetNeedsRedraw();
  void SetNeedsUpdateDrawProperties();

  // TODO(nduca): These are implemented in cc files temporarily, but will become
  // trivial accessors in a followup patch.
  const LayerTreeDebugState& debug_state() const;
  float device_scale_factor() const;
  const gfx::Size& device_viewport_size() const;
  const gfx::Size& layout_viewport_size() const;
  std::string layer_tree_as_text() const;
  DebugRectHistory* debug_rect_history() const;

  // Other public methods
  // ---------------------------------------------------------------------------
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

  LayerImpl* LayerById(int id);

  // These should be called by LayerImpl's ctor/dtor.
  void RegisterLayer(LayerImpl* layer);
  void UnregisterLayer(LayerImpl* layer);

protected:
  LayerTreeImpl(LayerTreeHostImpl* layer_tree_host_impl);

  LayerTreeHostImpl* layer_tree_host_impl_;
  int source_frame_number_;
  scoped_ptr<LayerImpl> root_layer_;
  HeadsUpDisplayLayerImpl* hud_layer_;
  LayerImpl* root_scroll_layer_;
  LayerImpl* currently_scrolling_layer_;

  typedef base::hash_map<int, LayerImpl*> LayerIdMap;
  LayerIdMap layer_id_map_;

  // Persisted state
  int scrolling_layer_id_from_previous_tree_;

  DISALLOW_COPY_AND_ASSIGN(LayerTreeImpl);
};

}

#endif  // CC_LAYER_TREE_IMPL_H_
