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
  return canvas_->saveLayer(bounds, ToSkPaint(flags));
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

bool SkiaPaintCanvas::quickReject(const SkRect& rect) const {
  return canvas_->quickReject(rect);
}

bool SkiaPaintCanvas::quickReject(const SkPath& path) const {
  return canvas_->quickReject(path);
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
  SkiaPaintCanvas::canvas_->drawLine(x0, y0, x1, y1, ToSkPaint(flags));
}

void SkiaPaintCanvas::drawRect(const SkRect& rect, const PaintFlags& flags) {
  canvas_->drawRect(rect, ToSkPaint(flags));
}

void SkiaPaintCanvas::drawIRect(const SkIRect& rect, const PaintFlags& flags) {
  canvas_->drawIRect(rect, ToSkPaint(flags));
}

void SkiaPaintCanvas::drawOval(const SkRect& oval, const PaintFlags& flags) {
  canvas_->drawOval(oval, ToSkPaint(flags));
}

void SkiaPaintCanvas::drawRRect(const SkRRect& rrect, const PaintFlags& flags) {
  canvas_->drawRRect(rrect, ToSkPaint(flags));
}

void SkiaPaintCanvas::drawDRRect(const SkRRect& outer,
                                 const SkRRect& inner,
                                 const PaintFlags& flags) {
  canvas_->drawDRRect(outer, inner, ToSkPaint(flags));
}

void SkiaPaintCanvas::drawCircle(SkScalar cx,
                                 SkScalar cy,
                                 SkScalar radius,
                                 const PaintFlags& flags) {
  canvas_->drawCircle(cx, cy, radius, ToSkPaint(flags));
}

void SkiaPaintCanvas::drawArc(const SkRect& oval,
                              SkScalar start_angle,
                              SkScalar sweep_angle,
                              bool use_center,
                              const PaintFlags& flags) {
  canvas_->drawArc(oval, start_angle, sweep_angle, use_center,
                   ToSkPaint(flags));
}

void SkiaPaintCanvas::drawRoundRect(const SkRect& rect,
                                    SkScalar rx,
                                    SkScalar ry,
                                    const PaintFlags& flags) {
  canvas_->drawRoundRect(rect, rx, ry, ToSkPaint(flags));
}

void SkiaPaintCanvas::drawPath(const SkPath& path, const PaintFlags& flags) {
  canvas_->drawPath(path, ToSkPaint(flags));
}

void SkiaPaintCanvas::drawImage(const PaintImage& image,
                                SkScalar left,
                                SkScalar top,
                                const PaintFlags* flags) {
  canvas_->drawImage(image.sk_image().get(), left, top, ToSkPaint(flags));
}

void SkiaPaintCanvas::drawImageRect(const PaintImage& image,
                                    const SkRect& src,
                                    const SkRect& dst,
                                    const PaintFlags* flags,
                                    SrcRectConstraint constraint) {
  canvas_->drawImageRect(image.sk_image().get(), src, dst, ToSkPaint(flags),
                         static_cast<SkCanvas::SrcRectConstraint>(constraint));
}

void SkiaPaintCanvas::drawBitmap(const SkBitmap& bitmap,
                                 SkScalar left,
                                 SkScalar top,
                                 const PaintFlags* flags) {
  canvas_->drawBitmap(bitmap, left, top, ToSkPaint(flags));
}

void SkiaPaintCanvas::drawText(const void* text,
                               size_t byte_length,
                               SkScalar x,
                               SkScalar y,
                               const PaintFlags& flags) {
  canvas_->drawText(text, byte_length, x, y, ToSkPaint(flags));
}

void SkiaPaintCanvas::drawPosText(const void* text,
                                  size_t byte_length,
                                  const SkPoint pos[],
                                  const PaintFlags& flags) {
  canvas_->drawPosText(text, byte_length, pos, ToSkPaint(flags));
}

void SkiaPaintCanvas::drawTextBlob(sk_sp<SkTextBlob> blob,
                                   SkScalar x,
                                   SkScalar y,
                                   const PaintFlags& flags) {
  canvas_->drawTextBlob(blob.get(), x, y, ToSkPaint(flags));
}

void SkiaPaintCanvas::drawPicture(sk_sp<const PaintRecord> record) {
  record->playback(canvas_);
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

}  // namespace cc
