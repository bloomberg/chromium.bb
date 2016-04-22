// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_content_layer_client.h"

#include <stddef.h>

#include "cc/playback/clip_display_item.h"
#include "cc/playback/display_item_list_settings.h"
#include "cc/playback/drawing_display_item.h"
#include "cc/playback/transform_display_item.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/skia_util.h"

namespace cc {

FakeContentLayerClient::ImageData::ImageData(sk_sp<const SkImage> img,
                                             const gfx::Point& point,
                                             const SkPaint& paint)
    : image(std::move(img)), point(point), paint(paint) {}

FakeContentLayerClient::ImageData::ImageData(sk_sp<const SkImage> img,
                                             const gfx::Transform& transform,
                                             const SkPaint& paint)
    : image(std::move(img)), transform(transform), paint(paint) {}

FakeContentLayerClient::ImageData::ImageData(const ImageData& other) = default;

FakeContentLayerClient::ImageData::~ImageData() {}

FakeContentLayerClient::FakeContentLayerClient()
    : display_list_use_cached_picture_(true),
      fill_with_nonsolid_color_(false),
      last_canvas_(nullptr),
      last_painting_control_(PAINTING_BEHAVIOR_NORMAL),
      reported_memory_usage_(0),
      bounds_set_(false) {}

FakeContentLayerClient::~FakeContentLayerClient() {
}

gfx::Rect FakeContentLayerClient::PaintableRegion() {
  CHECK(bounds_set_);
  return gfx::Rect(bounds_);
}

scoped_refptr<DisplayItemList>
FakeContentLayerClient::PaintContentsToDisplayList(
    PaintingControlSetting painting_control) {
  // Cached picture is used because unit tests expect to be able to
  // use GatherPixelRefs.
  DisplayItemListSettings settings;
  settings.use_cached_picture = display_list_use_cached_picture_;
  scoped_refptr<DisplayItemList> display_list =
      DisplayItemList::Create(PaintableRegion(), settings);
  SkPictureRecorder recorder;
  sk_sp<SkCanvas> canvas;

  for (RectPaintVector::const_iterator it = draw_rects_.begin();
       it != draw_rects_.end(); ++it) {
    const gfx::RectF& draw_rect = it->first;
    const SkPaint& paint = it->second;
    canvas = sk_ref_sp(recorder.beginRecording(gfx::RectFToSkRect(draw_rect)));
    canvas->drawRect(gfx::RectFToSkRect(draw_rect), paint);
    display_list->CreateAndAppendItem<DrawingDisplayItem>(
        ToEnclosingRect(draw_rect), recorder.finishRecordingAsPicture());
  }

  for (ImageVector::const_iterator it = draw_images_.begin();
       it != draw_images_.end(); ++it) {
    if (!it->transform.IsIdentity()) {
      display_list->CreateAndAppendItem<TransformDisplayItem>(PaintableRegion(),
                                                              it->transform);
    }
    canvas = sk_ref_sp(
        recorder.beginRecording(it->image->width(), it->image->height()));
    canvas->drawImage(it->image.get(), it->point.x(), it->point.y(),
                      &it->paint);
    display_list->CreateAndAppendItem<DrawingDisplayItem>(
        PaintableRegion(), recorder.finishRecordingAsPicture());
    if (!it->transform.IsIdentity()) {
      display_list->CreateAndAppendItem<EndTransformDisplayItem>(
          PaintableRegion());
    }
  }

  if (fill_with_nonsolid_color_) {
    gfx::Rect draw_rect = PaintableRegion();
    bool red = true;
    while (!draw_rect.IsEmpty()) {
      SkPaint paint;
      paint.setColor(red ? SK_ColorRED : SK_ColorBLUE);
      canvas = sk_ref_sp(recorder.beginRecording(gfx::RectToSkRect(draw_rect)));
      canvas->drawIRect(gfx::RectToSkIRect(draw_rect), paint);
      display_list->CreateAndAppendItem<DrawingDisplayItem>(
          draw_rect, recorder.finishRecordingAsPicture());
      draw_rect.Inset(1, 1);
    }
  }


  display_list->Finalize();
  return display_list;
}

bool FakeContentLayerClient::FillsBoundsCompletely() const { return false; }

size_t FakeContentLayerClient::GetApproximateUnsharedMemoryUsage() const {
  return reported_memory_usage_;
}

}  // namespace cc
