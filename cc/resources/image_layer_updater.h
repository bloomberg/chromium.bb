// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_IMAGE_LAYER_UPDATER_H_
#define CC_RESOURCES_IMAGE_LAYER_UPDATER_H_

#include "cc/resources/layer_updater.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace cc {

class ResourceUpdateQueue;

class ImageLayerUpdater : public LayerUpdater {
 public:
  class Resource : public LayerUpdater::Resource {
   public:
    Resource(ImageLayerUpdater* updater,
             scoped_ptr<PrioritizedResource> texture);
    virtual ~Resource();

    virtual void Update(ResourceUpdateQueue* queue,
                        gfx::Rect source_rect,
                        gfx::Vector2d dest_offset,
                        bool partial_update,
                        RenderingStats* stats) OVERRIDE;

   private:
    ImageLayerUpdater* updater_;

    DISALLOW_COPY_AND_ASSIGN(Resource);
  };

  static scoped_refptr<ImageLayerUpdater> Create();

  virtual scoped_ptr<LayerUpdater::Resource> CreateResource(
      PrioritizedResourceManager*) OVERRIDE;

  void UpdateTexture(ResourceUpdateQueue* queue,
                     PrioritizedResource* texture,
                     gfx::Rect source_rect,
                     gfx::Vector2d dest_offset,
                     bool partial_update);

  void set_bitmap(const SkBitmap& bitmap) { bitmap_ = bitmap; }

 private:
  ImageLayerUpdater() {}
  virtual ~ImageLayerUpdater() {}

  SkBitmap bitmap_;

  DISALLOW_COPY_AND_ASSIGN(ImageLayerUpdater);
};

}  // namespace cc

#endif  // CC_RESOURCES_IMAGE_LAYER_UPDATER_H_
