// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_PICTURE_LAYER_IMPL_H_
#define CC_LAYERS_PICTURE_LAYER_IMPL_H_

#include <stddef.h>

#include <map>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "cc/cc_export.h"
#include "cc/layers/layer.h"
#include "cc/layers/layer_impl.h"
#include "cc/paint/image_id.h"
#include "cc/tiles/picture_layer_tiling.h"
#include "cc/tiles/picture_layer_tiling_set.h"
#include "cc/tiles/tiling_set_eviction_queue.h"

namespace cc {

class AppendQuadsData;
class MicroBenchmarkImpl;
class Tile;

class CC_EXPORT PictureLayerImpl
    : public LayerImpl,
      NON_EXPORTED_BASE(public PictureLayerTilingClient) {
 public:
  static std::unique_ptr<PictureLayerImpl>
  Create(LayerTreeImpl* tree_impl, int id, Layer::LayerMaskType mask_type) {
    return base::WrapUnique(new PictureLayerImpl(tree_impl, id, mask_type));
  }
  ~PictureLayerImpl() override;

  Layer::LayerMaskType mask_type() const { return mask_type_; }
  void SetLayerMaskType(Layer::LayerMaskType type);

  // LayerImpl overrides.
  const char* LayerTypeAsString() const override;
  std::unique_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* tree_impl) override;
  void PushPropertiesTo(LayerImpl* layer) override;
  void AppendQuads(RenderPass* render_pass,
                   AppendQuadsData* append_quads_data) override;
  void NotifyTileStateChanged(const Tile* tile) override;
  void ResetRasterScale();
  void DidBeginTracing() override;
  void ReleaseResources() override;
  void ReleaseTileResources() override;
  void RecreateTileResources() override;
  Region GetInvalidationRegionForDebugging() override;

  // PictureLayerTilingClient overrides.
  std::unique_ptr<Tile> CreateTile(const Tile::CreateInfo& info) override;
  gfx::Size CalculateTileSize(const gfx::Size& content_bounds) const override;
  const Region* GetPendingInvalidation() override;
  const PictureLayerTiling* GetPendingOrActiveTwinTiling(
      const PictureLayerTiling* tiling) const override;
  bool HasValidTilePriorities() const override;
  bool RequiresHighResToDraw() const override;
  gfx::Rect GetEnclosingRectInTargetSpace() const override;

  void set_gpu_raster_max_texture_size(gfx::Size gpu_raster_max_texture_size) {
    gpu_raster_max_texture_size_ = gpu_raster_max_texture_size;
  }
  void UpdateRasterSource(scoped_refptr<RasterSource> raster_source,
                          Region* new_invalidation,
                          const PictureLayerTilingSet* pending_set);
  bool UpdateTiles();
  // Returns true if the LCD state changed.
  bool UpdateCanUseLCDTextAfterCommit();
  WhichTree GetTree() const;

  // Mask-related functions.
  void GetContentsResourceId(ResourceId* resource_id,
                             gfx::Size* resource_size,
                             gfx::SizeF* resource_uv_size) const override;

  void SetNearestNeighbor(bool nearest_neighbor);

  void SetUseTransformedRasterization(bool use);

  size_t GPUMemoryUsageInBytes() const override;

  void RunMicroBenchmark(MicroBenchmarkImpl* benchmark) override;

  bool CanHaveTilings() const;

  PictureLayerTilingSet* picture_layer_tiling_set() { return tilings_.get(); }

  // Functions used by tile manager.
  PictureLayerImpl* GetPendingOrActiveTwinLayer() const;
  bool IsOnActiveOrPendingTree() const;

  // Used for benchmarking
  RasterSource* GetRasterSource() const { return raster_source_.get(); }

  void set_is_directly_composited_image(bool is_directly_composited_image) {
    is_directly_composited_image_ = is_directly_composited_image;
  }

  void InvalidateRegionForImages(
      const PaintImageIdFlatSet& images_to_invalidate);

  bool RasterSourceUsesLCDTextForTesting() const { return can_use_lcd_text_; }

 protected:
  PictureLayerImpl(LayerTreeImpl* tree_impl,
                   int id,
                   Layer::LayerMaskType mask_type);
  PictureLayerTiling* AddTiling(const gfx::AxisTransform2d& contents_transform);
  void RemoveAllTilings();
  void AddTilingsForRasterScale();
  void AddLowResolutionTilingIfNeeded();
  bool ShouldAdjustRasterScale() const;
  void RecalculateRasterScales();
  gfx::Vector2dF CalculateRasterTranslation(float raster_scale);
  void CleanUpTilingsOnActiveLayer(
      const std::vector<PictureLayerTiling*>& used_tilings);
  float MinimumContentsScale() const;
  float MaximumContentsScale() const;
  void UpdateViewportRectForTilePriorityInContentSpace();
  PictureLayerImpl* GetRecycledTwinLayer() const;

  void SanityCheckTilingState() const;

  void GetDebugBorderProperties(SkColor* color, float* width) const override;
  void GetAllPrioritizedTilesForTracing(
      std::vector<PrioritizedTile>* prioritized_tiles) const override;
  void AsValueInto(base::trace_event::TracedValue* dict) const override;

  void UpdateIdealScales();
  float MaximumTilingContentsScale() const;
  std::unique_ptr<PictureLayerTilingSet> CreatePictureLayerTilingSet();

  PictureLayerImpl* twin_layer_;

  std::unique_ptr<PictureLayerTilingSet> tilings_;
  scoped_refptr<RasterSource> raster_source_;
  Region invalidation_;

  float ideal_page_scale_;
  float ideal_device_scale_;
  float ideal_source_scale_;
  float ideal_contents_scale_;

  float raster_page_scale_;
  float raster_device_scale_;
  float raster_source_scale_;
  float raster_contents_scale_;
  float low_res_raster_contents_scale_;

  Layer::LayerMaskType mask_type_;

  bool was_screen_space_transform_animating_ : 1;
  bool only_used_low_res_last_append_quads_ : 1;

  bool nearest_neighbor_ : 1;
  bool use_transformed_rasterization_ : 1;
  bool is_directly_composited_image_ : 1;
  bool can_use_lcd_text_ : 1;

  // Use this instead of |visible_layer_rect()| for tiling calculations. This
  // takes external viewport and transform for tile priority into account.
  gfx::Rect viewport_rect_for_tile_priority_in_content_space_;

  gfx::Size gpu_raster_max_texture_size_;

  // List of tilings that were used last time we appended quads. This can be
  // used as an optimization not to remove tilings if they are still being
  // drawn. Note that accessing this vector should only be done in the context
  // of comparing pointers, since objects pointed to are not guaranteed to
  // exist.
  std::vector<PictureLayerTiling*> last_append_quads_tilings_;

  DISALLOW_COPY_AND_ASSIGN(PictureLayerImpl);
};

}  // namespace cc

#endif  // CC_LAYERS_PICTURE_LAYER_IMPL_H_
