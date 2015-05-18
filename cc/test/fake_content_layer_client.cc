// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_content_layer_client.h"

#include "cc/playback/clip_display_item.h"
#include "cc/playback/drawing_display_item.h"
#include "cc/playback/transform_display_item.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "ui/gfx/skia_util.h"

namespace cc {

FakeContentLayerClient::BitmapData::BitmapData(const SkBitmap& bitmap,
                                               const gfx::Point& point,
                                               const SkPaint& paint)
    : bitmap(bitmap), point(point), paint(paint) {
}

FakeContentLayerClient::BitmapData::BitmapData(const SkBitmap& bitmap,
                                               const gfx::Transform& transform,
                                               const SkPaint& paint)
    : bitmap(bitmap), transform(transform), paint(paint) {
}

FakeContentLayerClient::BitmapData::~BitmapData() {
}

FakeContentLayerClient::FakeContentLayerClient()
    : fill_with_nonsolid_color_(false), last_canvas_(NULL) {
}

FakeContentLayerClient::~FakeContentLayerClient() {
}

void FakeContentLayerClient::PaintContents(
    SkCanvas* canvas,
    const gfx::Rect& paint_rect,
    PaintingControlSetting painting_control) {
  last_canvas_ = canvas;
  last_painting_control_ = painting_control;

  canvas->clipRect(gfx::RectToSkRect(paint_rect));
  for (RectPaintVector::const_iterator it = draw_rects_.begin();
      it != draw_rects_.end(); ++it) {
    const gfx::RectF& draw_rect = it->first;
    const SkPaint& paint = it->second;
    canvas->drawRectCoords(draw_rect.x(),
                           draw_rect.y(),
                           draw_rect.right(),
                           draw_rect.bottom(),
                           paint);
  }

  for (BitmapVector::const_iterator it = draw_bitmaps_.begin();
      it != draw_bitmaps_.end(); ++it) {
    canvas->drawBitmap(it->bitmap, it->point.x(), it->point.y(), &it->paint);
  }

  if (fill_with_nonsolid_color_) {
    gfx::RectF draw_rect = paint_rect;
    bool red = true;
    while (!draw_rect.IsEmpty()) {
      SkPaint paint;
      paint.setColor(red ? SK_ColorRED : SK_ColorBLUE);
      canvas->drawRect(gfx::RectFToSkRect(draw_rect), paint);
      draw_rect.Inset(1, 1);
    }
  }
}

void FakeContentLayerClient::PaintContentsToDisplayList(
    DisplayItemList* display_list,
    const gfx::Rect& clip,
    PaintingControlSetting painting_control) {
  SkPictureRecorder recorder;
  skia::RefPtr<SkCanvas> canvas;
  skia::RefPtr<SkPicture> picture;
  auto* item = display_list->CreateAndAppendItem<ClipDisplayItem>();
  item->SetNew(clip, std::vector<SkRRect>());

  for (RectPaintVector::const_iterator it = draw_rects_.begin();
       it != draw_rects_.end(); ++it) {
    const gfx::RectF& draw_rect = it->first;
    const SkPaint& paint = it->second;
    canvas =
        skia::SharePtr(recorder.beginRecording(gfx::RectFToSkRect(draw_rect)));
    canvas->drawRectCoords(draw_rect.x(), draw_rect.y(), draw_rect.width(),
                           draw_rect.height(), paint);
    picture = skia::AdoptRef(recorder.endRecordingAsPicture());
    auto* item = display_list->CreateAndAppendItem<DrawingDisplayItem>();
    item->SetNew(picture.Pass());
  }

  for (BitmapVector::const_iterator it = draw_bitmaps_.begin();
       it != draw_bitmaps_.end(); ++it) {
    if (!it->transform.IsIdentity()) {
      auto* item = display_list->CreateAndAppendItem<TransformDisplayItem>();
      item->SetNew(it->transform);
    }
    canvas = skia::SharePtr(
        recorder.beginRecording(it->bitmap.width(), it->bitmap.height()));
    canvas->drawBitmap(it->bitmap, it->point.x(), it->point.y(), &it->paint);
    picture = skia::AdoptRef(recorder.endRecordingAsPicture());
    auto* item = display_list->CreateAndAppendItem<DrawingDisplayItem>();
    item->SetNew(picture.Pass());
    if (!it->transform.IsIdentity()) {
      display_list->CreateAndAppendItem<EndTransformDisplayItem>();
    }
  }

  if (fill_with_nonsolid_color_) {
    gfx::RectF draw_rect = clip;
    bool red = true;
    while (!draw_rect.IsEmpty()) {
      SkPaint paint;
      paint.setColor(red ? SK_ColorRED : SK_ColorBLUE);
      canvas = skia::SharePtr(
          recorder.beginRecording(gfx::RectFToSkRect(draw_rect)));
      canvas->drawRect(gfx::RectFToSkRect(draw_rect), paint);
      picture = skia::AdoptRef(recorder.endRecordingAsPicture());
      auto* item = display_list->CreateAndAppendItem<DrawingDisplayItem>();
      item->SetNew(picture.Pass());
      draw_rect.Inset(1, 1);
    }
  }

  display_list->CreateAndAppendItem<EndClipDisplayItem>();
}

bool FakeContentLayerClient::FillsBoundsCompletely() const { return false; }

}  // namespace cc
