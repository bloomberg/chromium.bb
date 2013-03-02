// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/trace_event.h"
#include "cc/content_layer_client.h"
#include "cc/picture.h"
#include "cc/rendering_stats.h"
#include "skia/ext/analysis_canvas.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/utils/SkPictureUtils.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/gfx/skia_util.h"

namespace {
// URI label for a lazily decoded SkPixelRef.
const char labelLazyDecoded[] = "lazy";
}

namespace cc {

scoped_refptr<Picture> Picture::Create(gfx::Rect layer_rect) {
  return make_scoped_refptr(new Picture(layer_rect));
}

Picture::Picture(gfx::Rect layer_rect)
    : layer_rect_(layer_rect) {
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
  scoped_array<SkPicture> clones(new SkPicture[num_threads]);
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
                     const SkTileGridPicture::TileGridInfo& tileGridInfo) {
  TRACE_EVENT2("cc", "Picture::Record",
               "width", layer_rect_.width(), "height", layer_rect_.height());

  // Record() should only be called once.
  DCHECK(!picture_);
  DCHECK(!tileGridInfo.fTileInterval.isEmpty());
  picture_ = skia::AdoptRef(new SkTileGridPicture(
      layer_rect_.width(), layer_rect_.height(), tileGridInfo));

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
  painter->paintContents(canvas, layer_rect_, opaque_layer_rect);
  if (stats) {
    stats->totalPaintTime += base::TimeTicks::Now() - begin_paint_time;
    stats->totalPixelsPainted +=
        layer_rect_.width() * layer_rect_.height();
  }

  canvas->restore();
  picture_->endRecording();

  opaque_rect_ = gfx::ToEnclosedRect(opaque_layer_rect);
}

void Picture::Raster(
    SkCanvas* canvas,
    gfx::Rect content_rect,
    float contents_scale) {
  TRACE_EVENT2("cc", "Picture::Raster",
               "layer width", layer_rect_.width(),
               "layer height", layer_rect_.height());
  DCHECK(picture_);

  canvas->save();
  canvas->clipRect(gfx::RectToSkRect(content_rect));
  canvas->scale(contents_scale, contents_scale);
  canvas->translate(layer_rect_.x(), layer_rect_.y());
  canvas->drawPicture(*picture_);
  canvas->restore();
}

bool Picture::IsCheapInRect(const gfx::Rect& layer_rect) const {
  TRACE_EVENT0("cc", "Picture::IsCheapInRect");

  SkBitmap emptyBitmap;
  emptyBitmap.setConfig(SkBitmap::kNo_Config, layer_rect.width(),
                        layer_rect.height());
  skia::AnalysisDevice device(emptyBitmap);
  skia::AnalysisCanvas canvas(&device);

  canvas.drawPicture(*picture_);
  return canvas.isCheap();
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
  for (unsigned int i = 0; i < pixel_refs->size() / sizeof(SkPixelRef*); ++i) {
    if (*refs && (*refs)->getURI() && !strncmp(
        (*refs)->getURI(), labelLazyDecoded, 4)) {
      pixel_ref_list.push_back(static_cast<skia::LazyPixelRef*>(*refs));
    }
    refs++;
  }
  pixel_refs->unref();
}

}  // namespace cc
