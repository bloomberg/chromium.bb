// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/picture.h"

#include "base/base64.h"
#include "base/debug/trace_event.h"
#include "cc/debug/rendering_stats.h"
#include "cc/layers/content_layer_client.h"
#include "skia/ext/analysis_canvas.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkDrawFilter.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/utils/SkPictureUtils.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/gfx/skia_util.h"

namespace {
// URI label for a lazily decoded SkPixelRef.
const char kLabelLazyDecoded[] = "lazy";
// Version ID; to be used in serialization.
const int kPictureVersion = 1;
// Minimum size of a decoded stream that we need.
// 4 bytes for version, 4 * 4 for each of the 2 rects.
const unsigned int kMinPictureSizeBytes = 36;

class DisableLCDTextFilter : public SkDrawFilter {
 public:
  // SkDrawFilter interface.
  virtual bool filter(SkPaint* paint, SkDrawFilter::Type type) OVERRIDE {
    if (type != SkDrawFilter::kText_Type)
      return true;

    paint->setLCDRenderText(false);
    return true;
  }
};
}  // namespace

namespace cc {

scoped_refptr<Picture> Picture::Create(gfx::Rect layer_rect) {
  return make_scoped_refptr(new Picture(layer_rect));
}

scoped_refptr<Picture> Picture::CreateFromBase64String(
    const std::string& encoded_string) {
  bool success;
  scoped_refptr<Picture> picture =
    make_scoped_refptr(new Picture(encoded_string, &success));
  if (!success)
    picture = NULL;
  return picture;
}

Picture::Picture(gfx::Rect layer_rect)
    : layer_rect_(layer_rect) {
}

Picture::Picture(const std::string& encoded_string, bool* success) {
  // Decode the picture from base64.
  std::string decoded;
  base::Base64Decode(encoded_string, &decoded);
  SkMemoryStream stream(decoded.data(), decoded.size());

  if (decoded.size() < kMinPictureSizeBytes) {
    *success = false;
    return;
  }

  int version = stream.readS32();
  if (version != kPictureVersion) {
    *success = false;
    return;
  }

  // First, read the layer and opaque rects.
  int layer_rect_x = stream.readS32();
  int layer_rect_y = stream.readS32();
  int layer_rect_width = stream.readS32();
  int layer_rect_height = stream.readS32();
  layer_rect_ = gfx::Rect(layer_rect_x,
                          layer_rect_y,
                          layer_rect_width,
                          layer_rect_height);
  int opaque_rect_x = stream.readS32();
  int opaque_rect_y = stream.readS32();
  int opaque_rect_width = stream.readS32();
  int opaque_rect_height = stream.readS32();
  opaque_rect_ = gfx::Rect(opaque_rect_x,
                           opaque_rect_y,
                           opaque_rect_width,
                           opaque_rect_height);

  // Read the picture. This creates an empty picture on failure.
  picture_ = skia::AdoptRef(new SkPicture(&stream, success, NULL));
}

Picture::Picture(const skia::RefPtr<SkPicture>& picture,
                 gfx::Rect layer_rect,
                 gfx::Rect opaque_rect) :
    layer_rect_(layer_rect),
    opaque_rect_(opaque_rect),
    picture_(picture) {
}

Picture::~Picture() {
}

scoped_refptr<Picture> Picture::GetCloneForDrawingOnThread(
    unsigned thread_index) const {
  // SkPicture is not thread-safe to rasterize with, this returns a clone
  // to rasterize with on a specific thread.
  CHECK_GT(clones_.size(), thread_index);
  return clones_[thread_index];
}

void Picture::CloneForDrawing(int num_threads) {
  TRACE_EVENT1("cc", "Picture::CloneForDrawing", "num_threads", num_threads);

  DCHECK(picture_);
  scoped_ptr<SkPicture[]> clones(new SkPicture[num_threads]);
  picture_->clone(&clones[0], num_threads);

  clones_.clear();
  for (int i = 0; i < num_threads; i++) {
    scoped_refptr<Picture> clone = make_scoped_refptr(
        new Picture(skia::AdoptRef(new SkPicture(clones[i])),
                    layer_rect_,
                    opaque_rect_));
    clones_.push_back(clone);
  }
}

void Picture::Record(ContentLayerClient* painter,
                     RenderingStats* stats,
                     const SkTileGridPicture::TileGridInfo& tile_grid_info) {
  TRACE_EVENT2("cc", "Picture::Record",
               "width", layer_rect_.width(), "height", layer_rect_.height());

  // Record() should only be called once.
  DCHECK(!picture_);
  DCHECK(!tile_grid_info.fTileInterval.isEmpty());
  picture_ = skia::AdoptRef(new SkTileGridPicture(
      layer_rect_.width(), layer_rect_.height(), tile_grid_info));

  SkCanvas* canvas = picture_->beginRecording(
      layer_rect_.width(),
      layer_rect_.height(),
      SkPicture::kUsePathBoundsForClip_RecordingFlag |
      SkPicture::kOptimizeForClippedPlayback_RecordingFlag);

  canvas->save();
  canvas->translate(SkFloatToScalar(-layer_rect_.x()),
                    SkFloatToScalar(-layer_rect_.y()));

  SkPaint paint;
  paint.setAntiAlias(false);
  paint.setXfermodeMode(SkXfermode::kClear_Mode);
  SkRect layer_skrect = SkRect::MakeXYWH(layer_rect_.x(),
                                         layer_rect_.y(),
                                         layer_rect_.width(),
                                         layer_rect_.height());
  canvas->clipRect(layer_skrect);
  canvas->drawRect(layer_skrect, paint);

  gfx::RectF opaque_layer_rect;
  base::TimeTicks begin_paint_time;
  if (stats)
    begin_paint_time = base::TimeTicks::Now();
  painter->PaintContents(canvas, layer_rect_, &opaque_layer_rect);
  if (stats) {
    stats->total_paint_time += base::TimeTicks::Now() - begin_paint_time;
    stats->total_pixels_painted +=
        layer_rect_.width() * layer_rect_.height();
  }

  canvas->restore();
  picture_->endRecording();

  opaque_rect_ = gfx::ToEnclosedRect(opaque_layer_rect);
}

void Picture::Raster(
    SkCanvas* canvas,
    gfx::Rect content_rect,
    float contents_scale,
    bool enable_lcd_text) {
  TRACE_EVENT2("cc", "Picture::Raster",
               "layer width", layer_rect_.width(),
               "layer height", layer_rect_.height());
  DCHECK(picture_);

  DisableLCDTextFilter disable_lcd_text_filter;

  canvas->save();
  canvas->clipRect(gfx::RectToSkRect(content_rect));
  canvas->scale(contents_scale, contents_scale);
  canvas->translate(layer_rect_.x(), layer_rect_.y());
  // Pictures by default have LCD text enabled.
  if (!enable_lcd_text)
    canvas->setDrawFilter(&disable_lcd_text_filter);
  canvas->drawPicture(*picture_);
  canvas->restore();
}

void Picture::GatherPixelRefs(const gfx::Rect& layer_rect,
                              std::list<skia::LazyPixelRef*>& pixel_ref_list) {
  DCHECK(picture_);
  SkData* pixel_refs = SkPictureUtils::GatherPixelRefs(
      picture_.get(), SkRect::MakeXYWH(layer_rect.x(),
                                       layer_rect.y(),
                                       layer_rect.width(),
                                       layer_rect.height()));
  if (!pixel_refs)
    return;

  void* data = const_cast<void*>(pixel_refs->data());
  if (!data) {
    pixel_refs->unref();
    return;
  }

  SkPixelRef** refs = reinterpret_cast<SkPixelRef**>(data);
  for (size_t i = 0; i < pixel_refs->size() / sizeof(*refs); ++i) {
    if (*refs && (*refs)->getURI() && !strncmp(
        (*refs)->getURI(), kLabelLazyDecoded, 4)) {
      pixel_ref_list.push_back(static_cast<skia::LazyPixelRef*>(*refs));
    }
    refs++;
  }
  pixel_refs->unref();
}

void Picture::AsBase64String(std::string* output) const {
  SkDynamicMemoryWStream stream;

  // First save the version, layer_rect_ and opaque_rect.
  stream.write32(kPictureVersion);

  stream.write32(layer_rect_.x());
  stream.write32(layer_rect_.y());
  stream.write32(layer_rect_.width());
  stream.write32(layer_rect_.height());

  stream.write32(opaque_rect_.x());
  stream.write32(opaque_rect_.y());
  stream.write32(opaque_rect_.width());
  stream.write32(opaque_rect_.height());

  // Serialize the picture.
  picture_->serialize(&stream);

  // Encode the picture as base64.
  size_t serialized_size = stream.bytesWritten();
  scoped_ptr<char[]> serialized_picture(new char[serialized_size]);
  stream.copyTo(serialized_picture.get());
  base::Base64Encode(std::string(serialized_picture.get(), serialized_size),
                     output);
}

}  // namespace cc
