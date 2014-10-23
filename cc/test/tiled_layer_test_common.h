// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_TILED_LAYER_TEST_COMMON_H_
#define CC_TEST_TILED_LAYER_TEST_COMMON_H_

#include "cc/base/region.h"
#include "cc/layers/tiled_layer.h"
#include "cc/layers/tiled_layer_impl.h"
#include "cc/resources/layer_updater.h"
#include "cc/resources/prioritized_resource.h"
#include "cc/resources/resource_provider.h"
#include "cc/resources/resource_update_queue.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace cc {

class FakeTiledLayer;

class FakeLayerUpdater : public LayerUpdater {
 public:
  class Resource : public LayerUpdater::Resource {
   public:
    Resource(FakeLayerUpdater* updater,
             scoped_ptr<PrioritizedResource> resource);
    ~Resource() override;

    void Update(ResourceUpdateQueue* queue,
                const gfx::Rect& source_rect,
                const gfx::Vector2d& dest_offset,
                bool partial_update) override;

   private:
    FakeLayerUpdater* layer_;
    SkBitmap bitmap_;

    DISALLOW_COPY_AND_ASSIGN(Resource);
  };

  FakeLayerUpdater();

  scoped_ptr<LayerUpdater::Resource> CreateResource(
      PrioritizedResourceManager* resource) override;

  void PrepareToUpdate(const gfx::Size& content_size,
                       const gfx::Rect& paint_rect,
                       const gfx::Size& tile_size,
                       float contents_width_scale,
                       float contents_height_scale) override;
  // Sets the rect to invalidate during the next call to PrepareToUpdate().
  // After the next call to PrepareToUpdate() the rect is reset.
  void SetRectToInvalidate(const gfx::Rect& rect, FakeTiledLayer* layer);
  // Last rect passed to PrepareToUpdate().
  gfx::Rect last_update_rect() const { return last_update_rect_; }

  // Value of |contents_width_scale| last passed to PrepareToUpdate().
  float last_contents_width_scale() const { return last_contents_width_scale_; }

  // Number of times PrepareToUpdate has been invoked.
  int prepare_count() const { return prepare_count_; }
  void ClearPrepareCount() { prepare_count_ = 0; }

  // Number of times Update() has been invoked on a texture.
  int update_count() const { return update_count_; }
  void ClearUpdateCount() { update_count_ = 0; }
  void Update() { update_count_++; }

 protected:
  ~FakeLayerUpdater() override;

 private:
  int prepare_count_;
  int update_count_;
  gfx::Rect rect_to_invalidate_;
  gfx::Rect last_update_rect_;
  float last_contents_width_scale_;
  scoped_refptr<FakeTiledLayer> layer_;

  DISALLOW_COPY_AND_ASSIGN(FakeLayerUpdater);
};

class FakeTiledLayerImpl : public TiledLayerImpl {
 public:
  FakeTiledLayerImpl(LayerTreeImpl* tree_impl, int id);
  ~FakeTiledLayerImpl() override;

  using TiledLayerImpl::HasTileAt;
  using TiledLayerImpl::HasResourceIdForTileAt;
};

class FakeTiledLayer : public TiledLayer {
 public:
  explicit FakeTiledLayer(PrioritizedResourceManager* resource_manager);

  static gfx::Size tile_size() { return gfx::Size(100, 100); }

  using TiledLayer::InvalidateContentRect;
  using TiledLayer::NeedsIdlePaint;
  using TiledLayer::SkipsDraw;
  using TiledLayer::NumPaintedTiles;
  using TiledLayer::IdlePaintRect;

  void SetNeedsDisplayRect(const gfx::Rect& rect) override;
  const gfx::Rect& last_needs_display_rect() const {
    return last_needs_display_rect_;
  }

  void SetTexturePriorities(
      const PriorityCalculator& priority_calculator) override;

  PrioritizedResourceManager* ResourceManager() override;
  FakeLayerUpdater* fake_layer_updater() { return fake_updater_.get(); }
  gfx::RectF update_rect() { return update_rect_; }

  // Simulate CalcDrawProperties.
  void UpdateContentsScale(float ideal_contents_scale);

  void ResetNumDependentsNeedPushProperties();

 protected:
  LayerUpdater* Updater() const override;
  void CreateUpdaterIfNeeded() override {}
  ~FakeTiledLayer() override;

 private:
  scoped_refptr<FakeLayerUpdater> fake_updater_;
  PrioritizedResourceManager* resource_manager_;
  gfx::Rect last_needs_display_rect_;

  DISALLOW_COPY_AND_ASSIGN(FakeTiledLayer);
};

class FakeTiledLayerWithScaledBounds : public FakeTiledLayer {
 public:
  explicit FakeTiledLayerWithScaledBounds(
      PrioritizedResourceManager* resource_manager);

  void SetContentBounds(const gfx::Size& content_bounds);
  void CalculateContentsScale(float ideal_contents_scale,
                              float* contents_scale_x,
                              float* contents_scale_y,
                              gfx::Size* content_bounds) override;

 protected:
  ~FakeTiledLayerWithScaledBounds() override;
  gfx::Size forced_content_bounds_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeTiledLayerWithScaledBounds);
};

}  // namespace cc

#endif  // CC_TEST_TILED_LAYER_TEST_COMMON_H_
