// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_BITMAP_CONTENT_LAYER_UPDATER_H_
#define CC_RESOURCES_BITMAP_CONTENT_LAYER_UPDATER_H_

#include "cc/base/cc_export.h"
#include "cc/resources/content_layer_updater.h"
#include "skia/ext/refptr.h"
#include "third_party/skia/include/core/SkBitmap.h"

class SkCanvas;

namespace cc {

class LayerPainter;
class RenderingStatsInstrumenation;

// This class rasterizes the content_rect into a skia bitmap canvas. It then
// creates a ResourceUpdate with this bitmap canvas and inserts the
// ResourceBundle to the provided ResourceUpdateQueue. Actual texture uploading
// is done by ResourceUpdateController.
class CC_EXPORT BitmapContentLayerUpdater : public ContentLayerUpdater {
 public:
  class Resource : public LayerUpdater::Resource {
   public:
    Resource(BitmapContentLayerUpdater* updater,
             scoped_ptr<PrioritizedResource> resource);
    ~Resource() override;

    void Update(ResourceUpdateQueue* queue,
                const gfx::Rect& source_rect,
                const gfx::Vector2d& dest_offset,
                bool partial_update) override;

   private:
    BitmapContentLayerUpdater* updater_;

    DISALLOW_COPY_AND_ASSIGN(Resource);
  };

  static scoped_refptr<BitmapContentLayerUpdater> Create(
      scoped_ptr<LayerPainter> painter,
      int layer_id);

  scoped_ptr<LayerUpdater::Resource> CreateResource(
      PrioritizedResourceManager* manager) override;
  void PrepareToUpdate(const gfx::Size& content_size,
                       const gfx::Rect& paint_rect,
                       const gfx::Size& tile_size,
                       float contents_width_scale,
                       float contents_height_scale) override;
  void UpdateTexture(ResourceUpdateQueue* queue,
                     PrioritizedResource* resource,
                     const gfx::Rect& source_rect,
                     const gfx::Vector2d& dest_offset,
                     bool partial_update);
  void SetOpaque(bool opaque) override;
  void ReduceMemoryUsage() override;

 protected:
  BitmapContentLayerUpdater(
      scoped_ptr<LayerPainter> painter,
      int layer_id);
  ~BitmapContentLayerUpdater() override;

  SkBitmap bitmap_backing_;
  skia::RefPtr<SkCanvas> canvas_;
  gfx::Size canvas_size_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BitmapContentLayerUpdater);
};

}  // namespace cc

#endif  // CC_RESOURCES_BITMAP_CONTENT_LAYER_UPDATER_H_
