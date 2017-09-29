// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RASTER_PLAYBACK_IMAGE_PROVIDER_H_
#define CC_RASTER_PLAYBACK_IMAGE_PROVIDER_H_

#include "base/containers/flat_map.h"
#include "cc/cc_export.h"
#include "cc/paint/image_id.h"
#include "cc/paint/image_provider.h"
#include "ui/gfx/color_space.h"

namespace cc {
class ImageDecodeCache;

// PlaybackImageProvider is used to replace lazy generated PaintImages with
// decoded images for raster from the ImageDecodeCache.
class CC_EXPORT PlaybackImageProvider : public ImageProvider {
 public:
  struct CC_EXPORT Settings {
    Settings();
    Settings(const Settings& other);
    ~Settings();

    // The set of image ids to skip during raster.
    PaintImageIdFlatSet images_to_skip;

    // The set of images which must be decoded by the provider before beginning
    // raster. The images are decoded and locked by the provider in BeginRaster
    // and unlocked in EndRaster.
    std::vector<DrawImage> at_raster_images;

    // The frame index to use for the given image id. If no index is provided,
    // the frame index provided in the PaintImage will be used.
    base::flat_map<PaintImage::Id, size_t> image_to_current_frame_index;
  };

  // If no settings are provided, all images are skipped during rasterization.
  PlaybackImageProvider(ImageDecodeCache* cache,
                        const gfx::ColorSpace& target_color_space,
                        base::Optional<Settings> settings);
  ~PlaybackImageProvider() override;

  void BeginRaster() override;
  void EndRaster() override;

  PlaybackImageProvider(PlaybackImageProvider&& other);
  PlaybackImageProvider& operator=(PlaybackImageProvider&& other);

  // ImageProvider implementation.
  ScopedDecodedDrawImage GetDecodedDrawImage(
      const DrawImage& draw_image) override;

 private:
  ImageDecodeCache* cache_;
  gfx::ColorSpace target_color_space_;
  base::Optional<Settings> settings_;

  bool in_raster_ = false;
  std::vector<ImageProvider::ScopedDecodedDrawImage> decoded_at_raster_;

  DISALLOW_COPY_AND_ASSIGN(PlaybackImageProvider);
};

}  // namespace cc

#endif  // CC_RASTER_PLAYBACK_IMAGE_PROVIDER_H_
