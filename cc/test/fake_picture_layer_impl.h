// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_PICTURE_LAYER_IMPL_H_
#define CC_TEST_FAKE_PICTURE_LAYER_IMPL_H_

#include "base/memory/scoped_ptr.h"
#include "cc/layers/picture_layer_impl.h"

namespace cc {

class FakePictureLayerImpl : public PictureLayerImpl {
 public:
  static scoped_ptr<FakePictureLayerImpl> Create(
      LayerTreeImpl* tree_impl, int id) {
    return make_scoped_ptr(new FakePictureLayerImpl(tree_impl, id));
  }

  // Create layer from a pile that covers the entire layer.
  static scoped_ptr<FakePictureLayerImpl> CreateWithPile(
      LayerTreeImpl* tree_impl, int id, scoped_refptr<PicturePileImpl> pile) {
    return make_scoped_ptr(new FakePictureLayerImpl(tree_impl, id, pile));
  }

  // Create layer from a pile that only covers part of the layer.
  static scoped_ptr<FakePictureLayerImpl> CreateWithPartialPile(
      LayerTreeImpl* tree_impl,
      int id,
      scoped_refptr<PicturePileImpl> pile,
      const gfx::Size& layer_bounds) {
    return make_scoped_ptr(
        new FakePictureLayerImpl(tree_impl, id, pile, layer_bounds));
  }

  virtual scoped_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* tree_impl)
      OVERRIDE;
  virtual void AppendQuads(QuadSink* quad_sink,
                           AppendQuadsData* append_quads_data) OVERRIDE;
  virtual gfx::Size CalculateTileSize(
      const gfx::Size& content_bounds) const OVERRIDE;

  using PictureLayerImpl::AddTiling;
  using PictureLayerImpl::CleanUpTilingsOnActiveLayer;
  using PictureLayerImpl::CanHaveTilings;
  using PictureLayerImpl::MarkVisibleResourcesAsRequired;
  using PictureLayerImpl::DoPostCommitInitializationIfNeeded;
  using PictureLayerImpl::MinimumContentsScale;

  bool needs_post_commit_initialization() const {
    return needs_post_commit_initialization_;
  }

  bool is_using_lcd_text() const { return is_using_lcd_text_; }
  void force_set_lcd_text(bool enabled) { is_using_lcd_text_ = enabled; }

  PictureLayerTiling* HighResTiling() const;
  PictureLayerTiling* LowResTiling() const;
  size_t num_tilings() const { return tilings_->num_tilings(); }

  PictureLayerImpl* twin_layer() { return twin_layer_; }
  void set_twin_layer(PictureLayerImpl* twin) { twin_layer_ = twin; }
  PictureLayerTilingSet* tilings() { return tilings_.get(); }
  PicturePileImpl* pile() { return pile_.get(); }
  size_t append_quads_count() { return append_quads_count_; }

  const Region& invalidation() const { return invalidation_; }
  void set_invalidation(const Region& region) { invalidation_ = region; }

  void set_fixed_tile_size(const gfx::Size& size) { fixed_tile_size_ = size; }

  void CreateDefaultTilingsAndTiles();
  void SetAllTilesVisible();
  void SetAllTilesReady();
  void SetAllTilesReadyInTiling(PictureLayerTiling* tiling);
  void ResetAllTilesPriorities();

 protected:
  FakePictureLayerImpl(
      LayerTreeImpl* tree_impl,
      int id,
      scoped_refptr<PicturePileImpl> pile);
  FakePictureLayerImpl(LayerTreeImpl* tree_impl,
                       int id,
                       scoped_refptr<PicturePileImpl> pile,
                       const gfx::Size& layer_bounds);
  FakePictureLayerImpl(LayerTreeImpl* tree_impl, int id);

 private:
  gfx::Size fixed_tile_size_;

  size_t append_quads_count_;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_PICTURE_LAYER_IMPL_H_
