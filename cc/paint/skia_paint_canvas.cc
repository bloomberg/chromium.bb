// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/skia_paint_canvas.h"

#include "base/memory/ptr_util.h"
#include "cc/paint/display_item_list.h"
#include "cc/paint/paint_recorder.h"
#include "third_party/skia/include/core/SkAnnotation.h"
#include "third_party/skia/include/core/SkColorSpaceXformCanvas.h"
#include "third_party/skia/include/core/SkMetaData.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "third_party/skia/include/utils/SkNWayCanvas.h"

namespace cc {

SkiaPaintCanvas::SkiaPaintCanvas(SkCanvas* canvas) : canvas_(canvas) {}

SkiaPaintCanvas::SkiaPaintCanvas(const SkBitmap& bitmap)
    : canvas_(new SkCanvas(bitmap)), owned_(canvas_) {}

SkiaPaintCanvas::SkiaPaintCanvas(const SkBitmap& bitmap,
                                 const SkSurfaceProps& props)
    : canvas_(new SkCanvas(bitmap, props)), owned_(canvas_) {}

SkiaPaintCanvas::SkiaPaintCanvas(SkCanvas* canvas,
                                 sk_sp<SkColorSpace> target_color_space)
    : canvas_(canvas) {
  WrapCanvasInColorSpaceXformCanvas(target_color_space);
}

SkiaPaintCanvas::~SkiaPaintCanvas() = default;

void SkiaPaintCanvas::WrapCanvasInColorSpaceXformCanvas(
    sk_sp<SkColorSpace> target_color_space) {
  if (target_color_space) {
    color_space_xform_canvas_ =
        SkCreateColorSpaceXformCanvas(canvas_, target_color_space);
    canvas_ = color_space_xform_canvas_.get();
  }
}

SkMetaData& SkiaPaintCanvas::getMetaData() {
  return canvas_->getMetaData();
}

SkImageInfo SkiaPaintCanvas::imageInfo() const {
  return canvas_->imageInfo();
}

void SkiaPaintCanvas::flush() {
  canvas_->flush();
}

int SkiaPaintCanvas::save() {
  return canvas_->save();
}

int SkiaPaintCanvas::saveLayer(const SkRect* bounds, const PaintFlags* flags) {
  if (!flags)
    return canvas_->saveLayer(bounds, nullptr);
  SkPaint paint = flags->ToSkPaint();
  return canvas_->saveLayer(bounds, &paint);
}

int SkiaPaintCanvas::saveLayerAlpha(const SkRect* bounds,
                                    uint8_t alpha,
                                    bool preserve_lcd_text_requests) {
  if (preserve_lcd_text_requests) {
    SkPaint paint;
    paint.setAlpha(alpha);
    return canvas_->saveLayerPreserveLCDTextRequests(bounds, &paint);
  }
  return canvas_->saveLayerAlpha(bounds, alpha);
}

void SkiaPaintCanvas::restore() {
  canvas_->restore();
}

int SkiaPaintCanvas::getSaveCount() const {
  return canvas_->getSaveCount();
}

void SkiaPaintCanvas::restoreToCount(int save_count) {
  canvas_->restoreToCount(save_count);
}

void SkiaPaintCanvas::translate(SkScalar dx, SkScalar dy) {
  canvas_->translate(dx, dy);
}

void SkiaPaintCanvas::scale(SkScalar sx, SkScalar sy) {
  canvas_->scale(sx, sy);
}

void SkiaPaintCanvas::rotate(SkScalar degrees) {
  canvas_->rotate(degrees);
}

void SkiaPaintCanvas::concat(const SkMatrix& matrix) {
  canvas_->concat(matrix);
}

void SkiaPaintCanvas::setMatrix(const SkMatrix& matrix) {
  canvas_->setMatrix(matrix);
}

void SkiaPaintCanvas::clipRect(const SkRect& rect,
                               SkClipOp op,
                               bool do_anti_alias) {
  canvas_->clipRect(rect, op, do_anti_alias);
}

void SkiaPaintCanvas::clipRRect(const SkRRect& rrect,
                                SkClipOp op,
                                bool do_anti_alias) {
  canvas_->clipRRect(rrect, op, do_anti_alias);
}

void SkiaPaintCanvas::clipPath(const SkPath& path,
                               SkClipOp op,
                               bool do_anti_alias) {
  canvas_->clipPath(path, op, do_anti_alias);
}

SkRect SkiaPaintCanvas::getLocalClipBounds() const {
  return canvas_->getLocalClipBounds();
}

bool SkiaPaintCanvas::getLocalClipBounds(SkRect* bounds) const {
  return canvas_->getLocalClipBounds(bounds);
}

SkIRect SkiaPaintCanvas::getDeviceClipBounds() const {
  return canvas_->getDeviceClipBounds();
}

bool SkiaPaintCanvas::getDeviceClipBounds(SkIRect* bounds) const {
  return canvas_->getDeviceClipBounds(bounds);
}

void SkiaPaintCanvas::drawColor(SkColor color, SkBlendMode mode) {
  canvas_->drawColor(color, mode);
}

void SkiaPaintCanvas::clear(SkColor color) {
  canvas_->clear(color);
}

void SkiaPaintCanvas::drawLine(SkScalar x0,
                               SkScalar y0,
                               SkScalar x1,
                               SkScalar y1,
                               const PaintFlags& flags) {
  SkPaint paint = flags.ToSkPaint();
  SkiaPaintCanvas::canvas_->drawLine(x0, y0, x1, y1, paint);
}

void SkiaPaintCanvas::drawRect(const SkRect& rect, const PaintFlags& flags) {
  SkPaint paint = flags.ToSkPaint();
  canvas_->drawRect(rect, paint);
}

void SkiaPaintCanvas::drawIRect(const SkIRect& rect, const PaintFlags& flags) {
  SkPaint paint = flags.ToSkPaint();
  canvas_->drawIRect(rect, paint);
}

void SkiaPaintCanvas::drawOval(const SkRect& oval, const PaintFlags& flags) {
  SkPaint paint = flags.ToSkPaint();
  canvas_->drawOval(oval, paint);
}

void SkiaPaintCanvas::drawRRect(const SkRRect& rrect, const PaintFlags& flags) {
  SkPaint paint = flags.ToSkPaint();
  canvas_->drawRRect(rrect, paint);
}

void SkiaPaintCanvas::drawDRRect(const SkRRect& outer,
                                 const SkRRect& inner,
                                 const PaintFlags& flags) {
  SkPaint paint = flags.ToSkPaint();
  canvas_->drawDRRect(outer, inner, paint);
}

void SkiaPaintCanvas::drawRoundRect(const SkRect& rect,
                                    SkScalar rx,
                                    SkScalar ry,
                                    const PaintFlags& flags) {
  SkPaint paint = flags.ToSkPaint();
  canvas_->drawRoundRect(rect, rx, ry, paint);
}

void SkiaPaintCanvas::drawPath(const SkPath& path, const PaintFlags& flags) {
  SkPaint paint = flags.ToSkPaint();
  canvas_->drawPath(path, paint);
}

void SkiaPaintCanvas::drawImage(const PaintImage& image,
                                SkScalar left,
                                SkScalar top,
                                const PaintFlags* flags) {
  SkPaint paint;
  if (flags)
    paint = flags->ToSkPaint();
  canvas_->drawImage(image.GetSkImage().get(), left, top,
                     flags ? &paint : nullptr);
}

void SkiaPaintCanvas::drawImageRect(const PaintImage& image,
                                    const SkRect& src,
                                    const SkRect& dst,
                                    const PaintFlags* flags,
                                    SrcRectConstraint constraint) {
  SkPaint paint;
  if (flags)
    paint = flags->ToSkPaint();
  canvas_->drawImageRect(image.GetSkImage().get(), src, dst,
                         flags ? &paint : nullptr,
                         static_cast<SkCanvas::SrcRectConstraint>(constraint));
}

void SkiaPaintCanvas::drawBitmap(const SkBitmap& bitmap,
                                 SkScalar left,
                                 SkScalar top,
                                 const PaintFlags* flags) {
  if (flags) {
    SkPaint paint = flags->ToSkPaint();
    canvas_->drawBitmap(bitmap, left, top, &paint);
  } else {
    canvas_->drawBitmap(bitmap, left, top, nullptr);
  }
}

void SkiaPaintCanvas::drawTextBlob(scoped_refptr<PaintTextBlob> blob,
                                   SkScalar x,
                                   SkScalar y,
                                   const PaintFlags& flags) {
  SkPaint paint = flags.ToSkPaint();
  canvas_->drawTextBlob(blob->ToSkTextBlob(), x, y, paint);
}

void SkiaPaintCanvas::drawPicture(sk_sp<const PaintRecord> record) {
  record->Playback(canvas_);
}

bool SkiaPaintCanvas::isClipEmpty() const {
  return canvas_->isClipEmpty();
}

bool SkiaPaintCanvas::isClipRect() const {
  return canvas_->isClipRect();
}

const SkMatrix& SkiaPaintCanvas::getTotalMatrix() const {
  return canvas_->getTotalMatrix();
}

void SkiaPaintCanvas::Annotate(AnnotationType type,
                               const SkRect& rect,
                               sk_sp<SkData> data) {
  switch (type) {
    case AnnotationType::URL:
      SkAnnotateRectWithURL(canvas_, rect, data.get());
      break;
    case AnnotationType::LINK_TO_DESTINATION:
      SkAnnotateLinkToDestination(canvas_, rect, data.get());
      break;
    case AnnotationType::NAMED_DESTINATION: {
      SkPoint point = SkPoint::Make(rect.x(), rect.y());
      SkAnnotateNamedDestination(canvas_, point, data.get());
      break;
    }
  }
}

void SkiaPaintCanvas::drawPicture(sk_sp<const PaintRecord> record,
                                  const PlaybackParams& params) {
  record->Playback(canvas_, params);
}

}  // namespace cc
