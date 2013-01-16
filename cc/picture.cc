// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/trace_event.h"
#include "cc/content_layer_client.h"
#include "cc/picture.h"
#include "cc/rendering_stats.h"
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

scoped_refptr<Picture> Picture::Clone() const {
  // SkPicture is not thread-safe to rasterize with, so return a thread-safe
  // clone of it.
  DCHECK(picture_);
  skia::RefPtr<SkPicture> clone = skia::AdoptRef(picture_->clone());
  return make_scoped_refptr(new Picture(clone, layer_rect_, opaque_rect_));
}

void Picture::Record(ContentLayerClient* painter,
                     RenderingStats& stats) {
  TRACE_EVENT2("cc", "Picture::Record",
               "width", layer_rect_.width(), "height", layer_rect_.height());

  // Record() should only be called once.
  DCHECK(!picture_);
  picture_ = skia::AdoptRef(new SkPicture);

  // TODO(enne): Use SkPicture::kOptimizeForClippedPlayback_RecordingFlag
  // once http://code.google.com/p/skia/issues/detail?id=1014 is fixed.
  SkCanvas* canvas = picture_->beginRecording(
      layer_rect_.width(),
      layer_rect_.height(),
      SkPicture::kUsePathBoundsForClip_RecordingFlag);

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
  base::TimeTicks beginPaintTime = base::TimeTicks::Now();
  painter->paintContents(canvas, layer_rect_, opaque_layer_rect);
  stats.totalPaintTime += base::TimeTicks::Now() - beginPaintTime;
  stats.totalPixelsPainted += layer_rect_.width() *
                              layer_rect_.height();

  canvas->restore();
  picture_->endRecording();

  opaque_rect_ = gfx::ToEnclosedRect(opaque_layer_rect);
}

void Picture::Raster(
    SkCanvas* canvas,
    gfx::Rect content_rect,
    float contents_scale) {
  TRACE_EVENT2("cc", "Picture::Raster",
               "width", layer_rect_.width(), "height", layer_rect_.height());
  DCHECK(picture_);

  canvas->save();
  canvas->clipRect(gfx::RectToSkRect(content_rect));
  canvas->scale(contents_scale, contents_scale);
  canvas->translate(layer_rect_.x(), layer_rect_.y());
  canvas->drawPicture(*picture_);
  canvas->restore();
}

void Picture::GatherPixelRefs(const gfx::Rect& rect,
                              std::list<skia::LazyPixelRef*>& result) {
  DCHECK(picture_);
  SkData* pixel_refs = SkPictureUtils::GatherPixelRefs(
      picture_.get(), SkRect::MakeXYWH(rect.x(),
                                       rect.y(),
                                       rect.width(),
                                       rect.height()));
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
      result.push_back(static_cast<skia::LazyPixelRef*>(*refs));
    }
    refs++;
  }
  pixel_refs->unref();
}

}  // namespace cc
