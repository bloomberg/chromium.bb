// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_PAINT_CANVAS_H_
#define CC_PAINT_PAINT_CANVAS_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "cc/paint/paint_export.h"
#include "cc/paint/paint_flags.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/utils/SkNoDrawCanvas.h"

namespace cc {

class PaintFlags;
class PaintRecord;

class CC_PAINT_EXPORT PaintCanvas {
 public:
  // TODO(enne): remove all constructors but the one that takes an SkCanvas
  // and a future one that will take a PaintOpBuffer to build a PaintRecord.
  explicit PaintCanvas(SkCanvas* canvas);
  explicit PaintCanvas(const SkBitmap& bitmap);
  explicit PaintCanvas(const SkBitmap& bitmap, const SkSurfaceProps& props);
  ~PaintCanvas();

  ALWAYS_INLINE SkMetaData& getMetaData() { return canvas_->getMetaData(); }
  ALWAYS_INLINE SkImageInfo imageInfo() const { return canvas_->imageInfo(); }
  ALWAYS_INLINE bool getProps(SkSurfaceProps* props) const {
    return canvas_->getProps(props);
  }
  // TODO(enne): It would be nice to get rid of flush() entirely, as it
  // doesn't really make sense for recording.  However, this gets used by
  // SkCanvasVideoRenderer which takes a PaintCanvas to paint both
  // software and hardware video.  This is super entangled with ImageBuffer
  // and canvas/video painting in Blink where the same paths are used for
  // both recording and gpu work.
  ALWAYS_INLINE void flush() { canvas_->flush(); }

  ALWAYS_INLINE SkISize getBaseLayerSize() const {
    return canvas_->getBaseLayerSize();
  }
  ALWAYS_INLINE bool peekPixels(SkPixmap* pixmap) {
    return canvas_->peekPixels(pixmap);
  }
  ALWAYS_INLINE bool readPixels(const SkImageInfo& dest_info,
                                void* dest_pixels,
                                size_t dest_row_bytes,
                                int src_x,
                                int src_y) {
    return canvas_->readPixels(dest_info, dest_pixels, dest_row_bytes, src_x,
                               src_y);
  }
  ALWAYS_INLINE bool readPixels(SkBitmap* bitmap, int src_x, int src_y) {
    return canvas_->readPixels(bitmap, src_x, src_y);
  }
  ALWAYS_INLINE bool readPixels(const SkIRect& srcRect, SkBitmap* bitmap) {
    return canvas_->readPixels(srcRect, bitmap);
  }
  ALWAYS_INLINE bool writePixels(const SkImageInfo& info,
                                 const void* pixels,
                                 size_t row_bytes,
                                 int x,
                                 int y) {
    return canvas_->writePixels(info, pixels, row_bytes, x, y);
  }
  ALWAYS_INLINE int save() { return canvas_->save(); }
  ALWAYS_INLINE int saveLayer(const SkRect* bounds, const PaintFlags* flags) {
    return canvas_->saveLayer(bounds, ToSkPaint(flags));
  }
  ALWAYS_INLINE int saveLayer(const SkRect& bounds, const PaintFlags* flags) {
    return canvas_->saveLayer(bounds, ToSkPaint(flags));
  }
  ALWAYS_INLINE int saveLayerPreserveLCDTextRequests(const SkRect* bounds,
                                                     const PaintFlags* flags) {
    return canvas_->saveLayerPreserveLCDTextRequests(bounds, ToSkPaint(flags));
  }
  ALWAYS_INLINE int saveLayerAlpha(const SkRect* bounds, U8CPU alpha) {
    return canvas_->saveLayerAlpha(bounds, alpha);
  }

  ALWAYS_INLINE void restore() { canvas_->restore(); }
  ALWAYS_INLINE int getSaveCount() const { return canvas_->getSaveCount(); }
  ALWAYS_INLINE void restoreToCount(int save_count) {
    canvas_->restoreToCount(save_count);
  }
  ALWAYS_INLINE void translate(SkScalar dx, SkScalar dy) {
    canvas_->translate(dx, dy);
  }
  ALWAYS_INLINE void scale(SkScalar sx, SkScalar sy) { canvas_->scale(sx, sy); }
  ALWAYS_INLINE void rotate(SkScalar degrees) { canvas_->rotate(degrees); }
  ALWAYS_INLINE void rotate(SkScalar degrees, SkScalar px, SkScalar py) {
    canvas_->rotate(degrees, px, py);
  }
  ALWAYS_INLINE void skew(SkScalar sx, SkScalar sy) { canvas_->skew(sx, sy); }
  ALWAYS_INLINE void concat(const SkMatrix& matrix) { canvas_->concat(matrix); }
  ALWAYS_INLINE void setMatrix(const SkMatrix& matrix) {
    canvas_->setMatrix(matrix);
  }
  ALWAYS_INLINE void resetMatrix() { canvas_->resetMatrix(); }
  ALWAYS_INLINE void clipRect(const SkRect& rect,
                              SkClipOp op,
                              bool do_anti_alias = false) {
    canvas_->clipRect(rect, op, do_anti_alias);
  }
  ALWAYS_INLINE void clipRect(const SkRect& rect, bool do_anti_alias = false) {
    canvas_->clipRect(rect, do_anti_alias);
  }
  ALWAYS_INLINE void clipRRect(const SkRRect& rrect,
                               SkClipOp op,
                               bool do_anti_alias) {
    canvas_->clipRRect(rrect, op, do_anti_alias);
  }
  ALWAYS_INLINE void clipRRect(const SkRRect& rrect, SkClipOp op) {
    canvas_->clipRRect(rrect, op);
  }
  ALWAYS_INLINE void clipRRect(const SkRRect& rrect,
                               bool do_anti_alias = false) {
    canvas_->clipRRect(rrect, do_anti_alias);
  }
  ALWAYS_INLINE void clipPath(const SkPath& path,
                              SkClipOp op,
                              bool do_anti_alias) {
    canvas_->clipPath(path, op, do_anti_alias);
  }
  ALWAYS_INLINE void clipPath(const SkPath& path, SkClipOp op) {
    canvas_->clipPath(path, op);
  }
  ALWAYS_INLINE void clipPath(const SkPath& path, bool do_anti_alias = false) {
    canvas_->clipPath(path, do_anti_alias);
  }
  ALWAYS_INLINE void clipRegion(const SkRegion& device_region,
                                SkClipOp op = SkClipOp::kIntersect) {
    canvas_->clipRegion(device_region, op);
  }
  ALWAYS_INLINE bool quickReject(const SkRect& rect) const {
    return canvas_->quickReject(rect);
  }
  ALWAYS_INLINE bool quickReject(const SkPath& path) const {
    return canvas_->quickReject(path);
  }
  ALWAYS_INLINE SkRect getLocalClipBounds() const {
    return canvas_->getLocalClipBounds();
  }
  ALWAYS_INLINE bool getLocalClipBounds(SkRect* bounds) const {
    return canvas_->getLocalClipBounds(bounds);
  }
  ALWAYS_INLINE SkIRect getDeviceClipBounds() const {
    return canvas_->getDeviceClipBounds();
  }
  ALWAYS_INLINE bool getDeviceClipBounds(SkIRect* bounds) const {
    return canvas_->getDeviceClipBounds(bounds);
  }
  ALWAYS_INLINE void drawColor(SkColor color,
                               SkBlendMode mode = SkBlendMode::kSrcOver) {
    canvas_->drawColor(color, mode);
  }
  ALWAYS_INLINE void clear(SkColor color) { canvas_->clear(color); }
  ALWAYS_INLINE void discard() { canvas_->discard(); }

  ALWAYS_INLINE void drawLine(SkScalar x0,
                              SkScalar y0,
                              SkScalar x1,
                              SkScalar y1,
                              const PaintFlags& flags) {
    canvas_->drawLine(x0, y0, x1, y1, ToSkPaint(flags));
  }
  ALWAYS_INLINE void drawRect(const SkRect& rect, const PaintFlags& flags) {
    canvas_->drawRect(rect, ToSkPaint(flags));
  }
  ALWAYS_INLINE void drawIRect(const SkIRect& rect, const PaintFlags& flags) {
    canvas_->drawIRect(rect, ToSkPaint(flags));
  }
  ALWAYS_INLINE void drawOval(const SkRect& oval, const PaintFlags& flags) {
    canvas_->drawOval(oval, ToSkPaint(flags));
  }
  ALWAYS_INLINE void drawRRect(const SkRRect& rrect, const PaintFlags& flags) {
    canvas_->drawRRect(rrect, ToSkPaint(flags));
  }
  ALWAYS_INLINE void drawDRRect(const SkRRect& outer,
                                const SkRRect& inner,
                                const PaintFlags& flags) {
    canvas_->drawDRRect(outer, inner, ToSkPaint(flags));
  }
  ALWAYS_INLINE void drawCircle(SkScalar cx,
                                SkScalar cy,
                                SkScalar radius,
                                const PaintFlags& flags) {
    canvas_->drawCircle(cx, cy, radius, ToSkPaint(flags));
  }
  ALWAYS_INLINE void drawArc(const SkRect& oval,
                             SkScalar start_angle,
                             SkScalar sweep_angle,
                             bool use_center,
                             const PaintFlags& flags) {
    canvas_->drawArc(oval, start_angle, sweep_angle, use_center,
                     ToSkPaint(flags));
  }
  ALWAYS_INLINE void drawRoundRect(const SkRect& rect,
                                   SkScalar rx,
                                   SkScalar ry,
                                   const PaintFlags& flags) {
    canvas_->drawRoundRect(rect, rx, ry, ToSkPaint(flags));
  }
  ALWAYS_INLINE void drawPath(const SkPath& path, const PaintFlags& flags) {
    canvas_->drawPath(path, ToSkPaint(flags));
  }
  ALWAYS_INLINE void drawImage(const SkImage* image,
                               SkScalar left,
                               SkScalar top,
                               const PaintFlags* flags = nullptr) {
    canvas_->drawImage(image, left, top, ToSkPaint(flags));
  }
  ALWAYS_INLINE void drawImage(const sk_sp<SkImage>& image,
                               SkScalar left,
                               SkScalar top,
                               const PaintFlags* flags = nullptr) {
    canvas_->drawImage(image, left, top, ToSkPaint(flags));
  }

  enum SrcRectConstraint {
    kStrict_SrcRectConstraint = SkCanvas::kStrict_SrcRectConstraint,
    kFast_SrcRectConstraint = SkCanvas::kFast_SrcRectConstraint,
  };

  ALWAYS_INLINE void drawImageRect(
      const SkImage* image,
      const SkRect& src,
      const SkRect& dst,
      const PaintFlags* flags,
      SrcRectConstraint constraint = kStrict_SrcRectConstraint) {
    canvas_->drawImageRect(
        image, src, dst, ToSkPaint(flags),
        static_cast<SkCanvas::SrcRectConstraint>(constraint));
  }
  ALWAYS_INLINE void drawImageRect(
      const SkImage* image,
      const SkIRect& isrc,
      const SkRect& dst,
      const PaintFlags* flags,
      SrcRectConstraint constraint = kStrict_SrcRectConstraint) {
    canvas_->drawImageRect(
        image, isrc, dst, ToSkPaint(flags),
        static_cast<SkCanvas::SrcRectConstraint>(constraint));
  }
  ALWAYS_INLINE void drawImageRect(
      const SkImage* image,
      const SkRect& dst,
      const PaintFlags* flags,
      SrcRectConstraint constraint = kStrict_SrcRectConstraint) {
    canvas_->drawImageRect(
        image, dst, ToSkPaint(flags),
        static_cast<SkCanvas::SrcRectConstraint>(constraint));
  }
  ALWAYS_INLINE void drawImageRect(
      const sk_sp<SkImage>& image,
      const SkRect& src,
      const SkRect& dst,
      const PaintFlags* flags,
      SrcRectConstraint constraint = kStrict_SrcRectConstraint) {
    canvas_->drawImageRect(
        image, src, dst, ToSkPaint(flags),
        static_cast<SkCanvas::SrcRectConstraint>(constraint));
  }
  ALWAYS_INLINE void drawImageRect(
      const sk_sp<SkImage>& image,
      const SkIRect& isrc,
      const SkRect& dst,
      const PaintFlags* flags,
      SrcRectConstraint cons = kStrict_SrcRectConstraint) {
    canvas_->drawImageRect(image, isrc, dst, ToSkPaint(flags),
                           static_cast<SkCanvas::SrcRectConstraint>(cons));
  }
  ALWAYS_INLINE void drawImageRect(
      const sk_sp<SkImage>& image,
      const SkRect& dst,
      const PaintFlags* flags,
      SrcRectConstraint cons = kStrict_SrcRectConstraint) {
    canvas_->drawImageRect(image, dst, ToSkPaint(flags),
                           static_cast<SkCanvas::SrcRectConstraint>(cons));
  }
  ALWAYS_INLINE void drawBitmap(const SkBitmap& bitmap,
                                SkScalar left,
                                SkScalar top,
                                const PaintFlags* flags = nullptr) {
    canvas_->drawBitmap(bitmap, left, top, ToSkPaint(flags));
  }
  ALWAYS_INLINE void drawBitmapRect(
      const SkBitmap& bitmap,
      const SkRect& src,
      const SkRect& dst,
      const PaintFlags* flags,
      SrcRectConstraint constraint = kStrict_SrcRectConstraint) {
    canvas_->drawBitmapRect(
        bitmap, src, dst, ToSkPaint(flags),
        static_cast<SkCanvas::SrcRectConstraint>(constraint));
  }
  ALWAYS_INLINE void drawBitmapRect(
      const SkBitmap& bitmap,
      const SkIRect& isrc,
      const SkRect& dst,
      const PaintFlags* flags,
      SrcRectConstraint constraint = kStrict_SrcRectConstraint) {
    canvas_->drawBitmapRect(
        bitmap, isrc, dst, ToSkPaint(flags),
        static_cast<SkCanvas::SrcRectConstraint>(constraint));
  }
  ALWAYS_INLINE void drawBitmapRect(
      const SkBitmap& bitmap,
      const SkRect& dst,
      const PaintFlags* flags,
      SrcRectConstraint constraint = kStrict_SrcRectConstraint) {
    canvas_->drawBitmapRect(
        bitmap, dst, ToSkPaint(flags),
        static_cast<SkCanvas::SrcRectConstraint>(constraint));
  }

  ALWAYS_INLINE void drawText(const void* text,
                              size_t byte_length,
                              SkScalar x,
                              SkScalar y,
                              const PaintFlags& flags) {
    canvas_->drawText(text, byte_length, x, y, ToSkPaint(flags));
  }
  ALWAYS_INLINE void drawPosText(const void* text,
                                 size_t byte_length,
                                 const SkPoint pos[],
                                 const PaintFlags& flags) {
    canvas_->drawPosText(text, byte_length, pos, ToSkPaint(flags));
  }
  ALWAYS_INLINE void drawTextBlob(const SkTextBlob* blob,
                                  SkScalar x,
                                  SkScalar y,
                                  const PaintFlags& flags) {
    canvas_->drawTextBlob(blob, x, y, ToSkPaint(flags));
  }
  ALWAYS_INLINE void drawTextBlob(const sk_sp<SkTextBlob>& blob,
                                  SkScalar x,
                                  SkScalar y,
                                  const PaintFlags& flags) {
    canvas_->drawTextBlob(blob, x, y, ToSkPaint(flags));
  }

  ALWAYS_INLINE void drawPicture(const PaintRecord* record) {
    canvas_->drawPicture(ToSkPicture(record));
  }
  ALWAYS_INLINE void drawPicture(const PaintRecord* record,
                                 const SkMatrix* matrix,
                                 const PaintFlags* flags) {
    canvas_->drawPicture(ToSkPicture(record), matrix, ToSkPaint(flags));
  }
  ALWAYS_INLINE void drawPicture(sk_sp<PaintRecord> record) {
    drawPicture(record.get());
  }

  ALWAYS_INLINE bool isClipEmpty() const { return canvas_->isClipEmpty(); }
  ALWAYS_INLINE bool isClipRect() const { return canvas_->isClipRect(); }
  ALWAYS_INLINE const SkMatrix& getTotalMatrix() const {
    return canvas_->getTotalMatrix();
  }

  // For GraphicsContextCanvas only.  Maybe this could be rewritten?
  ALWAYS_INLINE void temporary_internal_describeTopLayer(SkMatrix* matrix,
                                                         SkIRect* clip_bounds) {
    return canvas_->temporary_internal_describeTopLayer(matrix, clip_bounds);
  }

 protected:
  friend class PaintSurface;
  friend class PaintRecord;
  friend class PaintRecorder;
  friend CC_PAINT_EXPORT void PaintCanvasAnnotateRectWithURL(
      PaintCanvas* canvas,
      const SkRect& rect,
      SkData* data);
  friend CC_PAINT_EXPORT void PaintCanvasAnnotateNamedDestination(
      PaintCanvas* canvas,
      const SkPoint& point,
      SkData* data);
  friend CC_PAINT_EXPORT void PaintCanvasAnnotateLinkToDestination(
      PaintCanvas* canvas,
      const SkRect& rect,
      SkData* data);
  friend CC_PAINT_EXPORT bool ToPixmap(PaintCanvas* canvas, SkPixmap* output);

 private:
  SkCanvas* canvas_;
  std::unique_ptr<SkCanvas> owned_;

  DISALLOW_COPY_AND_ASSIGN(PaintCanvas);
};

class CC_PAINT_EXPORT PaintCanvasAutoRestore {
 public:
  ALWAYS_INLINE PaintCanvasAutoRestore(PaintCanvas* canvas, bool save)
      : canvas_(canvas) {
    if (canvas_) {
      save_count_ = canvas_->getSaveCount();
      if (save) {
        canvas_->save();
      }
    }
  }

  ALWAYS_INLINE ~PaintCanvasAutoRestore() {
    if (canvas_) {
      canvas_->restoreToCount(save_count_);
    }
  }

  ALWAYS_INLINE void restore() {
    if (canvas_) {
      canvas_->restoreToCount(save_count_);
      canvas_ = nullptr;
    }
  }

 private:
  PaintCanvas* canvas_ = nullptr;
  int save_count_ = 0;
};

// TODO(enne): Move all these functions into PaintCanvas.  These are only
// separate now to make the transition to concrete types easier by keeping
// the base PaintCanvas type equivalent to the SkCanvas interface and
// all these helper functions potentially operating on both.

// PaintCanvas equivalent of skia::GetWritablePixels.
CC_PAINT_EXPORT bool ToPixmap(PaintCanvas* canvas, SkPixmap* output);

// Following routines are used in print preview workflow to mark the
// preview metafile.
#if defined(OS_MACOSX)
CC_PAINT_EXPORT void SetIsPreviewMetafile(PaintCanvas* canvas, bool is_preview);
CC_PAINT_EXPORT bool IsPreviewMetafile(PaintCanvas* canvas);
#endif

CC_PAINT_EXPORT void PaintCanvasAnnotateRectWithURL(PaintCanvas* canvas,
                                                    const SkRect& rect,
                                                    SkData* data);

CC_PAINT_EXPORT void PaintCanvasAnnotateNamedDestination(PaintCanvas* canvas,
                                                         const SkPoint& point,
                                                         SkData* data);

CC_PAINT_EXPORT void PaintCanvasAnnotateLinkToDestination(PaintCanvas* canvas,
                                                          const SkRect& rect,
                                                          SkData* data);

}  // namespace cc

#endif  // CC_PAINT_PAINT_CANVAS_H_
