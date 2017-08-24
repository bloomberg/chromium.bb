// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/skia_common.h"

#include <stddef.h>

#include "base/memory/ptr_util.h"
#include "cc/paint/display_item_list.h"
#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_image_builder.h"
#include "cc/test/stub_paint_image_generator.h"
#include "third_party/skia/include/core/SkImageGenerator.h"
#include "third_party/skia/include/core/SkPixmap.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/skia_util.h"

namespace cc {

namespace {

class TestImageGenerator : public StubPaintImageGenerator {
 public:
  explicit TestImageGenerator(const SkImageInfo& info)
      : StubPaintImageGenerator(info),
        image_backing_memory_(info.getSafeSize(info.minRowBytes()), 0),
        image_pixmap_(info, image_backing_memory_.data(), info.minRowBytes()) {}

  bool GetPixels(const SkImageInfo& info,
                 void* pixels,
                 size_t rowBytes,
                 uint32_t lazy_pixel_ref) override {
    return image_pixmap_.readPixels(info, pixels, rowBytes, 0, 0);
  }

 private:
  std::vector<uint8_t> image_backing_memory_;
  SkPixmap image_pixmap_;
};

}  // anonymous namespace

void DrawDisplayList(unsigned char* buffer,
                     const gfx::Rect& layer_rect,
                     scoped_refptr<const DisplayItemList> list) {
  SkImageInfo info =
      SkImageInfo::MakeN32Premul(layer_rect.width(), layer_rect.height());
  SkBitmap bitmap;
  bitmap.installPixels(info, buffer, info.minRowBytes());
  SkCanvas canvas(bitmap);
  canvas.clipRect(gfx::RectToSkRect(layer_rect));
  list->Raster(&canvas);
}

bool AreDisplayListDrawingResultsSame(const gfx::Rect& layer_rect,
                                      const DisplayItemList* list_a,
                                      const DisplayItemList* list_b) {
  const size_t pixel_size = 4 * layer_rect.size().GetArea();

  std::unique_ptr<unsigned char[]> pixels_a(new unsigned char[pixel_size]);
  std::unique_ptr<unsigned char[]> pixels_b(new unsigned char[pixel_size]);
  memset(pixels_a.get(), 0, pixel_size);
  memset(pixels_b.get(), 0, pixel_size);
  DrawDisplayList(pixels_a.get(), layer_rect, list_a);
  DrawDisplayList(pixels_b.get(), layer_rect, list_b);

  return !memcmp(pixels_a.get(), pixels_b.get(), pixel_size);
}

sk_sp<PaintImageGenerator> CreatePaintImageGenerator(const gfx::Size& size) {
  return sk_make_sp<TestImageGenerator>(
      SkImageInfo::MakeN32Premul(size.width(), size.height()));
}

PaintImage CreateDiscardablePaintImage(const gfx::Size& size,
                                       sk_sp<SkColorSpace> color_space) {
  return PaintImageBuilder()
      .set_id(PaintImage::GetNextId())
      .set_paint_image_generator(sk_make_sp<TestImageGenerator>(
          SkImageInfo::MakeN32Premul(size.width(), size.height(), color_space)))
      .TakePaintImage();
}

}  // namespace cc
