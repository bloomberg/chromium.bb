// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PICTURE_LAYER_IMPL_H_
#define CC_PICTURE_LAYER_IMPL_H_

#include "cc/layer_impl.h"
#include "cc/picture_layer_tiling.h"
#include "cc/picture_layer_tiling_set.h"
#include "cc/picture_pile_impl.h"
#include "cc/scoped_ptr_vector.h"
#include "skia/ext/refptr.h"
#include "third_party/skia/include/core/SkPicture.h"

namespace cc {

struct AppendQuadsData;
class QuadSink;

class CC_EXPORT PictureLayerImpl : public LayerImpl,
                                   public PictureLayerTilingClient {
public:
  static scoped_ptr<PictureLayerImpl> create(LayerTreeImpl* treeImpl, int id)
  {
      return make_scoped_ptr(new PictureLayerImpl(treeImpl, id));
  }
  virtual ~PictureLayerImpl();

  // LayerImpl overrides.
  virtual const char* layerTypeAsString() const OVERRIDE;
  virtual scoped_ptr<LayerImpl> createLayerImpl(
      LayerTreeImpl* treeImpl) OVERRIDE;
  virtual void pushPropertiesTo(LayerImpl* layer) OVERRIDE;
  virtual void appendQuads(QuadSink&, AppendQuadsData&) OVERRIDE;
  virtual void dumpLayerProperties(std::string*, int indent) const OVERRIDE;
  virtual void updateTilePriorities() OVERRIDE;
  virtual void didBecomeActive() OVERRIDE;
  virtual void didLoseOutputSurface() OVERRIDE;
  virtual void calculateContentsScale(
      float ideal_contents_scale,
      bool animating_transform_to_screen,
      float* contents_scale_x,
      float* contents_scale_y,
      gfx::Size* content_bounds) OVERRIDE;
  virtual skia::RefPtr<SkPicture> getPicture() OVERRIDE;

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
  virtual ResourceProvider::ResourceId contentsResourceId() const OVERRIDE;

  virtual bool areVisibleResourcesReady() const OVERRIDE;

  virtual scoped_ptr<base::Value> AsValue() const OVERRIDE;

protected:
  PictureLayerImpl(LayerTreeImpl* treeImpl, int id);
  PictureLayerTiling* AddTiling(float contents_scale);
  void RemoveTiling(float contents_scale);
  void SyncFromActiveLayer(const PictureLayerImpl* other);
  void ManageTilings(bool animating_transform_to_screen);
  virtual void CalculateRasterContentsScale(
      bool animating_transform_to_screen,
      float* raster_contents_scale,
      float* low_res_raster_contents_scale);
  void CleanUpTilingsOnActiveLayer(
      std::vector<PictureLayerTiling*> used_tilings);
  PictureLayerImpl* PendingTwin() const;
  PictureLayerImpl* ActiveTwin() const;
  float MinimumContentsScale() const;

  virtual void getDebugBorderProperties(
      SkColor* color, float* width) const OVERRIDE;

  scoped_ptr<PictureLayerTilingSet> tilings_;
  scoped_refptr<PicturePileImpl> pile_;
  Region invalidation_;

  gfx::Transform last_screen_space_transform_;
  gfx::Size last_bounds_;
  gfx::Size last_content_bounds_;
  float last_content_scale_;
  float ideal_contents_scale_;
  bool is_mask_;

  float ideal_page_scale_;
  float ideal_device_scale_;
  float ideal_source_scale_;

  float raster_page_scale_;
  float raster_device_scale_;
  float raster_source_scale_;
  bool raster_source_scale_was_animating_;

  friend class PictureLayer;
  DISALLOW_COPY_AND_ASSIGN(PictureLayerImpl);
};

}

#endif  // CC_PICTURE_LAYER_IMPL_H_
