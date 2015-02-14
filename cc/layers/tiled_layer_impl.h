// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_TILED_LAYER_IMPL_H_
#define CC_LAYERS_TILED_LAYER_IMPL_H_

#include <string>

#include "cc/base/cc_export.h"
#include "cc/layers/layer_impl.h"

namespace cc {

class LayerTilingData;
class DrawableTile;

class CC_EXPORT TiledLayerImpl : public LayerImpl {
 public:
  static scoped_ptr<TiledLayerImpl> Create(LayerTreeImpl* tree_impl, int id) {
    return make_scoped_ptr(
        new TiledLayerImpl(tree_impl, id, new LayerImpl::SyncedScrollOffset));
  }
  static scoped_ptr<TiledLayerImpl> Create(
      LayerTreeImpl* tree_impl,
      int id,
      scoped_refptr<LayerImpl::SyncedScrollOffset> synced_scroll_offset) {
    return make_scoped_ptr(
        new TiledLayerImpl(tree_impl, id, synced_scroll_offset));
  }
  ~TiledLayerImpl() override;

  scoped_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* tree_impl) override;
  void PushPropertiesTo(LayerImpl* layer) override;

  bool WillDraw(DrawMode draw_mode,
                ResourceProvider* resource_provider) override;
  void AppendQuads(RenderPass* render_pass,
                   AppendQuadsData* append_quads_data) override;

  void GetContentsResourceId(ResourceProvider::ResourceId* resource_id,
                             gfx::Size* resource_size) const override;

  void set_skips_draw(bool skips_draw) { skips_draw_ = skips_draw; }
  void SetTilingData(const LayerTilingData& tiler);
  void PushTileProperties(int i,
                          int j,
                          ResourceProvider::ResourceId resource,
                          bool contents_swizzled);
  void PushInvalidTile(int i, int j);

  SimpleEnclosedRegion VisibleContentOpaqueRegion() const override;
  void ReleaseResources() override;

  const LayerTilingData* TilingForTesting() const { return tiler_.get(); }

  size_t GPUMemoryUsageInBytes() const override;

 protected:
  TiledLayerImpl(LayerTreeImpl* tree_impl, int id);
  TiledLayerImpl(
      LayerTreeImpl* tree_impl,
      int id,
      scoped_refptr<LayerImpl::SyncedScrollOffset> synced_scroll_offset);
  // Exposed for testing.
  bool HasTileAt(int i, int j) const;
  bool HasResourceIdForTileAt(int i, int j) const;

  void GetDebugBorderProperties(SkColor* color, float* width) const override;
  void AsValueInto(base::trace_event::TracedValue* dict) const override;

 private:
  const char* LayerTypeAsString() const override;

  DrawableTile* TileAt(int i, int j) const;
  DrawableTile* CreateTile(int i, int j);

  bool skips_draw_;

  scoped_ptr<LayerTilingData> tiler_;

  DISALLOW_COPY_AND_ASSIGN(TiledLayerImpl);
};

}  // namespace cc

#endif  // CC_LAYERS_TILED_LAYER_IMPL_H_
