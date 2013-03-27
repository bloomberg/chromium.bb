// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_TILED_LAYER_TEST_COMMON_H_
#define CC_TEST_TILED_LAYER_TEST_COMMON_H_

#include "cc/base/region.h"
#include "cc/layers/tiled_layer.h"
#include "cc/layers/tiled_layer_impl.h"
#include "cc/output/texture_copier.h"
#include "cc/resources/layer_updater.h"
#include "cc/resources/prioritized_resource.h"
#include "cc/resources/resource_provider.h"
#include "cc/resources/resource_update_queue.h"
#include "cc/scheduler/texture_uploader.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

namespace cc {

class FakeTiledLayer;

class FakeLayerUpdater : public LayerUpdater {
 public:
  class Resource : public LayerUpdater::Resource {
   public:
    Resource(FakeLayerUpdater* updater,
             scoped_ptr<PrioritizedResource> resource);
    virtual ~Resource();

    virtual void Update(ResourceUpdateQueue* queue,
                        gfx::Rect source_rect,
                        gfx::Vector2d dest_offset,
                        bool partial_update) OVERRIDE;

   private:
    FakeLayerUpdater* layer_;
    SkBitmap bitmap_;

    DISALLOW_COPY_AND_ASSIGN(Resource);
  };

  FakeLayerUpdater();

  virtual scoped_ptr<LayerUpdater::Resource> CreateResource(
      PrioritizedResourceManager* resource) OVERRIDE;

  virtual void PrepareToUpdate(gfx::Rect content_rect,
                               gfx::Size tile_size,
                               float contents_width_scale,
                               float contents_height_scale,
                               gfx::Rect* resulting_opaque_rect) OVERRIDE;
  // Sets the rect to invalidate during the next call to PrepareToUpdate().
  // After the next call to PrepareToUpdate() the rect is reset.
  void SetRectToInvalidate(gfx::Rect rect, FakeTiledLayer* layer);
  // Last rect passed to PrepareToUpdate().
  gfx::Rect last_update_rect() const { return last_update_rect_; }

  // Number of times PrepareToUpdate has been invoked.
  int prepare_count() const { return prepare_count_; }
  void ClearPrepareCount() { prepare_count_ = 0; }

  // Number of times Update() has been invoked on a texture.
  int update_count() const { return update_count_; }
  void ClearUpdateCount() { update_count_ = 0; }
  void Update() { update_count_++; }

  void SetOpaquePaintRect(gfx::Rect opaque_paint_rect) {
    opaque_paint_rect_ = opaque_paint_rect;
  }

 protected:
  virtual ~FakeLayerUpdater();

 private:
  int prepare_count_;
  int update_count_;
  gfx::Rect rect_to_invalidate_;
  gfx::Rect last_update_rect_;
  gfx::Rect opaque_paint_rect_;
  scoped_refptr<FakeTiledLayer> layer_;

  DISALLOW_COPY_AND_ASSIGN(FakeLayerUpdater);
};

class FakeTiledLayerImpl : public TiledLayerImpl {
 public:
  FakeTiledLayerImpl(LayerTreeImpl* tree_impl, int id);
  virtual ~FakeTiledLayerImpl();

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

  virtual void SetNeedsDisplayRect(const gfx::RectF& rect) OVERRIDE;
  const gfx::RectF& last_needs_display_rect() const {
    return last_needs_display_rect_;
  }

  virtual void SetTexturePriorities(
      const PriorityCalculator& priority_calculator) OVERRIDE;

  virtual PrioritizedResourceManager* ResourceManager() const OVERRIDE;
  FakeLayerUpdater* fake_layer_updater() { return fake_updater_.get(); }
  gfx::RectF update_rect() { return update_rect_; }

  // Simulate CalcDrawProperties.
  void UpdateContentsScale(float ideal_contents_scale);

 protected:
  virtual LayerUpdater* Updater() const OVERRIDE;
  virtual void CreateUpdaterIfNeeded() OVERRIDE {}
  virtual ~FakeTiledLayer();

 private:
  scoped_refptr<FakeLayerUpdater> fake_updater_;
  PrioritizedResourceManager* resource_manager_;
  gfx::RectF last_needs_display_rect_;

  DISALLOW_COPY_AND_ASSIGN(FakeTiledLayer);
};

class FakeTiledLayerWithScaledBounds : public FakeTiledLayer {
 public:
  explicit FakeTiledLayerWithScaledBounds(
      PrioritizedResourceManager* resource_manager);

  void SetContentBounds(gfx::Size content_bounds);
  virtual void CalculateContentsScale(float ideal_contents_scale,
                                      bool animating_transform_to_screen,
                                      float* contents_scale_x,
                                      float* contents_scale_y,
                                      gfx::Size* content_bounds) OVERRIDE;

 protected:
  virtual ~FakeTiledLayerWithScaledBounds();
  gfx::Size forced_content_bounds_;

  DISALLOW_COPY_AND_ASSIGN(FakeTiledLayerWithScaledBounds);
};

}  // namespace cc

#endif  // CC_TEST_TILED_LAYER_TEST_COMMON_H_
