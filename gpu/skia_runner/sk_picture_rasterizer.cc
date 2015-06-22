// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/skia_runner/sk_picture_rasterizer.h"

#include <iostream>

#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/core/SkSurfaceProps.h"

namespace skia_runner {

SkPictureRasterizer::SkPictureRasterizer(skia::RefPtr<GrContext> gr_context,
                                         int max_texture_size)
    : use_lcd_text_(false),
      use_distance_field_text_(false),
      msaa_sample_count_(0),
      max_texture_size_(max_texture_size),
      gr_context_(gr_context) {
}

SkPictureRasterizer::~SkPictureRasterizer() {
}

skia::RefPtr<SkImage> SkPictureRasterizer::RasterizeTile(
    const SkPicture* picture,
    const SkRect& rect) const {
  DCHECK(gr_context_);

  SkImageInfo info = SkImageInfo::MakeN32Premul(rect.width(), rect.height());
  uint32_t flags = 0;
  if (use_distance_field_text_)
    flags = SkSurfaceProps::kUseDistanceFieldFonts_Flag;

  SkSurfaceProps surface_props =
      use_lcd_text_
          ? SkSurfaceProps(flags, SkSurfaceProps::kLegacyFontHost_InitType)
          : SkSurfaceProps(flags, kUnknown_SkPixelGeometry);

  skia::RefPtr<SkSurface> sk_surface(skia::AdoptRef(
      SkSurface::NewRenderTarget(gr_context_.get(), SkSurface::kYes_Budgeted,
                                 info, msaa_sample_count_, &surface_props)));
  if (sk_surface) {
    SkCanvas* canvas = sk_surface->getCanvas();
    canvas->translate(-rect.left(), -rect.top());
    canvas->drawPicture(picture);

    return skia::AdoptRef(sk_surface->newImageSnapshot());
  }
  return nullptr;
}

skia::RefPtr<SkImage> SkPictureRasterizer::Rasterize(
    const SkPicture* picture) const {
  if (!gr_context_)
    return nullptr;

  SkRect picture_rect = picture->cullRect();
  if (picture_rect.width() <= max_texture_size_ &&
      picture_rect.height() <= max_texture_size_)
    return RasterizeTile(picture, picture_rect);

  SkImageInfo info =
      SkImageInfo::MakeN32Premul(picture_rect.width(), picture_rect.height());

  skia::RefPtr<SkSurface> sk_surface(
      skia::AdoptRef(SkSurface::NewRaster(info)));
  SkCanvas* canvas = sk_surface->getCanvas();

  int num_tiles_x = picture_rect.width() / max_texture_size_ + 1;
  int num_tiles_y = picture_rect.height() / max_texture_size_ + 1;
  for (int y = 0; y < num_tiles_y; ++y) {
    SkRect tile_rect;
    tile_rect.fTop = picture_rect.top() + y * max_texture_size_;
    tile_rect.fBottom =
        std::min(tile_rect.fTop + max_texture_size_, picture_rect.bottom());
    for (int x = 0; x < num_tiles_x; ++x) {
      tile_rect.fLeft = picture_rect.left() + x * max_texture_size_;
      tile_rect.fRight =
          std::min(tile_rect.fLeft + max_texture_size_, picture_rect.right());
      skia::RefPtr<SkImage> tile(RasterizeTile(picture, tile_rect));
      canvas->drawImage(tile.get(), tile_rect.left(), tile_rect.top());
    }
  }
  return skia::AdoptRef(sk_surface->newImageSnapshot());
}

}  // namepsace skia_runner
