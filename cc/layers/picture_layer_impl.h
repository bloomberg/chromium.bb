// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_PICTURE_LAYER_IMPL_H_
#define CC_LAYERS_PICTURE_LAYER_IMPL_H_

#include <set>
#include <string>
#include <vector>

#include "cc/base/cc_export.h"
#include "cc/base/scoped_ptr_vector.h"
#include "cc/layers/layer_impl.h"
#include "cc/resources/picture_layer_tiling.h"
#include "cc/resources/picture_layer_tiling_set.h"
#include "cc/resources/picture_pile_impl.h"
#include "skia/ext/refptr.h"
#include "third_party/skia/include/core/SkPicture.h"

namespace cc {

struct AppendQuadsData;
class MicroBenchmarkImpl;
class Tile;

class CC_EXPORT PictureLayerImpl
    : public LayerImpl,
      NON_EXPORTED_BASE(public PictureLayerTilingClient) {
 public:
  struct CC_EXPORT Pair {
    Pair();
    Pair(PictureLayerImpl* active_layer, PictureLayerImpl* pending_layer);
    ~Pair();

    PictureLayerImpl* active;
    PictureLayerImpl* pending;
  };

  class CC_EXPORT LayerRasterTileIterator {
   public:
    LayerRasterTileIterator();
    LayerRasterTileIterator(PictureLayerImpl* layer, bool prioritize_low_res);
    ~LayerRasterTileIterator();

    Tile* operator*();
    const Tile* operator*() const;
    LayerRasterTileIterator& operator++();
    operator bool() const;

   private:
    enum IteratorType { LOW_RES, HIGH_RES, NUM_ITERATORS };

    void AdvanceToNextStage();

    PictureLayerImpl* layer_;

    struct IterationStage {
      IteratorType iterator_type;
      TilePriority::PriorityBin tile_type;
    };

    size_t current_stage_;

    // One low res stage, and three high res stages.
    IterationStage stages_[4];
    PictureLayerTiling::TilingRasterTileIterator iterators_[NUM_ITERATORS];
  };

  class CC_EXPORT LayerEvictionTileIterator {
   public:
    LayerEvictionTileIterator();
    LayerEvictionTileIterator(PictureLayerImpl* layer,
                              TreePriority tree_priority);
    ~LayerEvictionTileIterator();

    Tile* operator*();
    const Tile* operator*() const;
    LayerEvictionTileIterator& operator++();
    operator bool() const;

   private:
    bool AdvanceToNextCategory();
    bool AdvanceToNextTilingRangeType();
    bool AdvanceToNextTiling();

    PictureLayerTilingSet::TilingRange CurrentTilingRange() const;
    size_t CurrentTilingIndex() const;

    PictureLayerImpl* layer_;
    TreePriority tree_priority_;

    PictureLayerTiling::EvictionCategory current_category_;
    PictureLayerTilingSet::TilingRangeType current_tiling_range_type_;
    size_t current_tiling_;
    PictureLayerTiling::TilingEvictionTileIterator current_iterator_;
  };

  static scoped_ptr<PictureLayerImpl> Create(LayerTreeImpl* tree_impl, int id) {
    return make_scoped_ptr(new PictureLayerImpl(tree_impl, id));
  }
  ~PictureLayerImpl() override;

  // LayerImpl overrides.
  const char* LayerTypeAsString() const override;
  scoped_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* tree_impl) override;
  void PushPropertiesTo(LayerImpl* layer) override;
  void AppendQuads(RenderPass* render_pass,
                   const Occlusion& occlusion_in_content_space,
                   AppendQuadsData* append_quads_data) override;
  void UpdateTiles(const Occlusion& occlusion_in_content_space,
                   bool resourceless_software_draw) override;
  void NotifyTileStateChanged(const Tile* tile) override;
  void DidBecomeActive() override;
  void DidBeginTracing() override;
  void ReleaseResources() override;
  skia::RefPtr<SkPicture> GetPicture() override;

  // PictureLayerTilingClient overrides.
  scoped_refptr<Tile> CreateTile(PictureLayerTiling* tiling,
                                 const gfx::Rect& content_rect) override;
  RasterSource* GetRasterSource() override;
  gfx::Size CalculateTileSize(const gfx::Size& content_bounds) const override;
  const Region* GetPendingInvalidation() override;
  const PictureLayerTiling* GetPendingOrActiveTwinTiling(
      const PictureLayerTiling* tiling) const override;
  PictureLayerTiling* GetRecycledTwinTiling(
      const PictureLayerTiling* tiling) override;
  size_t GetMaxTilesForInterestArea() const override;
  float GetSkewportTargetTimeInSeconds() const override;
  int GetSkewportExtrapolationLimitInContentPixels() const override;
  WhichTree GetTree() const override;
  bool RequiresHighResToDraw() const override;

  // PushPropertiesTo active tree => pending tree.
  void SyncTiling(const PictureLayerTiling* tiling);

  // Mask-related functions.
  void GetContentsResourceId(ResourceProvider::ResourceId* resource_id,
                             gfx::Size* resource_size) const override;

  size_t GPUMemoryUsageInBytes() const override;

  void RunMicroBenchmark(MicroBenchmarkImpl* benchmark) override;

  // Functions used by tile manager.
  PictureLayerImpl* GetPendingOrActiveTwinLayer() const;
  bool IsOnActiveOrPendingTree() const;
  // Virtual for testing.
  virtual bool HasValidTilePriorities() const;
  bool AllTilesRequiredForActivationAreReadyToDraw() const;

 protected:
  friend class LayerRasterTileIterator;

  PictureLayerImpl(LayerTreeImpl* tree_impl, int id);
  PictureLayerTiling* AddTiling(float contents_scale);
  void RemoveTiling(float contents_scale);
  void RemoveAllTilings();
  void SyncFromActiveLayer(const PictureLayerImpl* other);
  void AddTilingsForRasterScale();
  void UpdateTilePriorities(const Occlusion& occlusion_in_content_space);
  virtual bool ShouldAdjustRasterScale() const;
  virtual void RecalculateRasterScales();
  void CleanUpTilingsOnActiveLayer(
      std::vector<PictureLayerTiling*> used_tilings);
  float MinimumContentsScale() const;
  float SnappedContentsScale(float new_contents_scale);
  void ResetRasterScale();
  gfx::Rect GetViewportForTilePriorityInContentSpace() const;
  PictureLayerImpl* GetRecycledTwinLayer() const;
  void UpdatePile(scoped_refptr<PicturePileImpl> pile);

  void DoPostCommitInitializationIfNeeded() {
    if (needs_post_commit_initialization_)
      DoPostCommitInitialization();
  }
  void DoPostCommitInitialization();

  bool CanHaveTilings() const;
  bool CanHaveTilingWithScale(float contents_scale) const;
  void SanityCheckTilingState() const;

  bool ShouldAdjustRasterScaleDuringScaleAnimations() const;

  void GetDebugBorderProperties(SkColor* color, float* width) const override;
  void GetAllTilesForTracing(std::set<const Tile*>* tiles) const override;
  void AsValueInto(base::debug::TracedValue* dict) const override;

  virtual void UpdateIdealScales();
  float MaximumTilingContentsScale() const;

  PictureLayerImpl* twin_layer_;

  scoped_ptr<PictureLayerTilingSet> tilings_;
  scoped_refptr<PicturePileImpl> pile_;
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

  bool raster_source_scale_is_fixed_;
  bool was_screen_space_transform_animating_;
  bool needs_post_commit_initialization_;
  // A sanity state check to make sure UpdateTilePriorities only gets called
  // after a CalculateContentsScale/ManageTilings.
  bool should_update_tile_priorities_;
  bool only_used_low_res_last_append_quads_;

  // Any draw properties derived from |transform|, |viewport|, and |clip|
  // parameters in LayerTreeHostImpl::SetExternalDrawConstraints are not valid
  // for prioritizing tiles during resourceless software draws. This is because
  // resourceless software draws can have wildly different transforms/viewports
  // from regular draws. Save a copy of the required draw properties of the last
  // frame that has a valid viewport for prioritizing tiles.
  gfx::Rect visible_rect_for_tile_priority_;

  friend class PictureLayer;
  DISALLOW_COPY_AND_ASSIGN(PictureLayerImpl);
};

}  // namespace cc

#endif  // CC_LAYERS_PICTURE_LAYER_IMPL_H_
