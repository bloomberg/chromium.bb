// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_BITMAP_CONTENT_LAYER_UPDATER_H_
#define CC_RESOURCES_BITMAP_CONTENT_LAYER_UPDATER_H_

#include "cc/base/cc_export.h"
#include "cc/resources/content_layer_updater.h"

class SkCanvas;

namespace cc {

class LayerPainter;

// This class rasterizes the content_rect into a skia bitmap canvas. It then
// updates textures by copying from the canvas into the texture, using
// MapSubImage if possible.
class CC_EXPORT BitmapContentLayerUpdater : public ContentLayerUpdater {
 public:
  class Resource : public LayerUpdater::Resource {
   public:
    Resource(BitmapContentLayerUpdater* updater,
             scoped_ptr<PrioritizedResource> resource);
    virtual ~Resource();

    virtual void Update(ResourceUpdateQueue* queue,
                        gfx::Rect source_rect,
                        gfx::Vector2d dest_offset,
                        bool partial_update,
                        RenderingStats* stats) OVERRIDE;

   private:
    BitmapContentLayerUpdater* updater_;

    DISALLOW_COPY_AND_ASSIGN(Resource);
  };

  static scoped_refptr<BitmapContentLayerUpdater> Create(
      scoped_ptr<LayerPainter> painter);

  virtual scoped_ptr<LayerUpdater::Resource> CreateResource(
      PrioritizedResourceManager* manager) OVERRIDE;
  virtual void PrepareToUpdate(gfx::Rect content_rect,
                               gfx::Size tile_size,
                               float contents_width_scale,
                               float contents_height_scale,
                               gfx::Rect* resulting_opaque_rect,
                               RenderingStats* stats) OVERRIDE;
  void UpdateTexture(ResourceUpdateQueue* queue,
                     PrioritizedResource* resource,
                     gfx::Rect source_rect,
                     gfx::Vector2d dest_offset,
                     bool partial_update);

  virtual void SetOpaque(bool opaque) OVERRIDE;

 protected:
  explicit BitmapContentLayerUpdater(scoped_ptr<LayerPainter> painter);
  virtual ~BitmapContentLayerUpdater();

  scoped_ptr<SkCanvas> canvas_;
  gfx::Size canvas_size_;
  bool opaque_;

  DISALLOW_COPY_AND_ASSIGN(BitmapContentLayerUpdater);
};

}  // namespace cc

#endif  // CC_RESOURCES_BITMAP_CONTENT_LAYER_UPDATER_H_
