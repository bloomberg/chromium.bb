// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RASTER_PLAYBACK_IMAGE_PROVIDER_H_
#define CC_RASTER_PLAYBACK_IMAGE_PROVIDER_H_

#include "cc/cc_export.h"
#include "cc/paint/image_id.h"
#include "cc/paint/image_provider.h"
#include "ui/gfx/color_space.h"

namespace cc {
class ImageDecodeCache;

// PlaybackImageProvider is used to replace lazy generated PaintImages with
// decoded images for raster from the ImageDecodeCache. The following settings
// can be used to modify rasterization of these images:
// 1) skip_all_images: Ensures that no images are decoded or rasterized.
// 2) images_to_skip: Used to selectively skip images during raster. This should
//    only be used for lazy generated images.
class CC_EXPORT PlaybackImageProvider : public ImageProvider {
 public:
  PlaybackImageProvider(bool skip_all_images,
                        PaintImageIdFlatSet images_to_skip,
                        std::vector<DrawImage> at_raster_images,
                        ImageDecodeCache* cache,
                        const gfx::ColorSpace& taget_color_space);
  ~PlaybackImageProvider() override;

  void BeginRaster() override;
  void EndRaster() override;

  PlaybackImageProvider(PlaybackImageProvider&& other);
  PlaybackImageProvider& operator=(PlaybackImageProvider&& other);

  // ImageProvider implementation.
  ScopedDecodedDrawImage GetDecodedDrawImage(
      const DrawImage& draw_image) override;

 private:
  bool skip_all_images_;
  bool in_raster_ = false;
  PaintImageIdFlatSet images_to_skip_;
  std::vector<DrawImage> at_raster_images_;
  std::vector<ImageProvider::ScopedDecodedDrawImage> decoded_at_raster_;
  ImageDecodeCache* cache_;
  gfx::ColorSpace target_color_space_;

  DISALLOW_COPY_AND_ASSIGN(PlaybackImageProvider);
};

}  // namespace cc

#endif  // CC_RASTER_PLAYBACK_IMAGE_PROVIDER_H_
