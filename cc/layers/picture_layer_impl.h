// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_PICTURE_LAYER_IMPL_H_
#define CC_LAYERS_PICTURE_LAYER_IMPL_H_

#include "cc/base/scoped_ptr_vector.h"
#include "cc/layers/layer_impl.h"
#include "cc/resources/picture_layer_tiling.h"
#include "cc/resources/picture_layer_tiling_set.h"
#include "cc/resources/picture_pile_impl.h"
#include "skia/ext/refptr.h"
#include "third_party/skia/include/core/SkPicture.h"

namespace cc {

struct AppendQuadsData;
class QuadSink;

class CC_EXPORT PictureLayerImpl : public LayerImpl,
                                   public PictureLayerTilingClient {
 public:
  static scoped_ptr<PictureLayerImpl> Create(LayerTreeImpl* tree_impl, int id) {
    return make_scoped_ptr(new PictureLayerImpl(tree_impl, id));
  }
  virtual ~PictureLayerImpl();

  // LayerImpl overrides.
  virtual const char* LayerTypeAsString() const OVERRIDE;
  virtual scoped_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* tree_impl)
      OVERRIDE;
  virtual void PushPropertiesTo(LayerImpl* layer) OVERRIDE;
  virtual void AppendQuads(QuadSink* quad_sink,
                           AppendQuadsData* append_quads_data) OVERRIDE;
  virtual void DumpLayerProperties(std::string* str, int indent) const OVERRIDE;
  virtual void UpdateTilePriorities() OVERRIDE;
  virtual void DidBecomeActive() OVERRIDE;
  virtual void DidLoseOutputSurface() OVERRIDE;
  virtual void CalculateContentsScale(float ideal_contents_scale,
                                      bool animating_transform_to_screen,
                                      float* contents_scale_x,
                                      float* contents_scale_y,
                                      gfx::Size* content_bounds) OVERRIDE;
  virtual skia::RefPtr<SkPicture> GetPicture() OVERRIDE;

  // PictureLayerTilingClient overrides.
  virtual scoped_refptr<Tile> CreateTile(PictureLayerTiling* tiling,
                                         gfx::Rect content_rect) OVERRIDE;
  virtual void UpdatePile(Tile* tile) OVERRIDE;
  virtual gfx::Size CalculateTileSize(
      gfx::Size current_tile_size,
      gfx::Size content_bounds) OVERRIDE;

  // PushPropertiesTo active tree => pending tree
  void SyncFromActiveLayer();
  void SyncTiling(
      const PictureLayerTiling* tiling,
      const Region& pending_layer_invalidation);

  void CreateTilingSet();
  void TransferTilingSet(scoped_ptr<PictureLayerTilingSet> tilings);

  // Mask-related functions
  void SetIsMask(bool is_mask);
  virtual ResourceProvider::ResourceId ContentsResourceId() const OVERRIDE;

  virtual bool AreVisibleResourcesReady() const OVERRIDE;

  virtual scoped_ptr<base::Value> AsValue() const OVERRIDE;

 protected:
  PictureLayerImpl(LayerTreeImpl* tree_impl, int id);
  PictureLayerTiling* AddTiling(float contents_scale);
  void RemoveTiling(float contents_scale);
  void SyncFromActiveLayer(const PictureLayerImpl* other);
  void ManageTilings(bool animating_transform_to_screen);
  virtual bool ShouldAdjustRasterScale(
      bool animating_transform_to_screen) const;
  virtual void CalculateRasterContentsScale(
      bool animating_transform_to_screen,
      float* raster_contents_scale,
      float* low_res_raster_contents_scale) const;
  void CleanUpTilingsOnActiveLayer(
      std::vector<PictureLayerTiling*> used_tilings);
  PictureLayerImpl* PendingTwin() const;
  PictureLayerImpl* ActiveTwin() const;
  float MinimumContentsScale() const;
  void UpdateLCDTextStatus();

  virtual void GetDebugBorderProperties(
      SkColor* color, float* width) const OVERRIDE;

  scoped_ptr<PictureLayerTilingSet> tilings_;
  scoped_refptr<PicturePileImpl> pile_;
  Region invalidation_;

  gfx::Transform last_screen_space_transform_;
  gfx::Size last_bounds_;
  float last_content_scale_;
  bool is_mask_;

  float ideal_page_scale_;
  float ideal_device_scale_;
  float ideal_source_scale_;
  float ideal_contents_scale_;

  float raster_page_scale_;
  float raster_device_scale_;
  float raster_source_scale_;
  float raster_contents_scale_;
  float low_res_raster_contents_scale_;

  bool raster_source_scale_was_animating_;
  bool is_using_lcd_text_;

  friend class PictureLayer;
  DISALLOW_COPY_AND_ASSIGN(PictureLayerImpl);
};

}

#endif  // CC_LAYERS_PICTURE_LAYER_IMPL_H_
