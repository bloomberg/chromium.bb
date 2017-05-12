// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_PAINT_CANVAS_H_
#define CC_PAINT_PAINT_CANVAS_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "cc/paint/paint_export.h"
#include "cc/paint/paint_image.h"
#include "third_party/skia/include/core/SkCanvas.h"

namespace cc {

class DisplayItemList;
class PaintFlags;
class PaintOpBuffer;

using PaintRecord = PaintOpBuffer;

class CC_PAINT_EXPORT PaintCanvas {
 public:
  PaintCanvas() {}
  virtual ~PaintCanvas() {}

  virtual SkMetaData& getMetaData() = 0;

  // TODO(enne): this only appears to mostly be used to determine if this is
  // recording or not, so could be simplified or removed.
  virtual SkImageInfo imageInfo() const = 0;

  // TODO(enne): It would be nice to get rid of flush() entirely, as it
  // doesn't really make sense for recording.  However, this gets used by
  // SkCanvasVideoRenderer which takes a PaintCanvas to paint both
  // software and hardware video.  This is super entangled with ImageBuffer
  // and canvas/video painting in Blink where the same paths are used for
  // both recording and gpu work.
  virtual void flush() = 0;

  virtual int save() = 0;
  virtual int saveLayer(const SkRect* bounds, const PaintFlags* flags) = 0;
  virtual int saveLayerAlpha(const SkRect* bounds, uint8_t alpha) = 0;

  virtual void restore() = 0;
  virtual int getSaveCount() const = 0;
  virtual void restoreToCount(int save_count) = 0;
  virtual void translate(SkScalar dx, SkScalar dy) = 0;
  virtual void scale(SkScalar sx, SkScalar sy) = 0;
  virtual void rotate(SkScalar degrees) = 0;
  virtual void concat(const SkMatrix& matrix) = 0;
  virtual void setMatrix(const SkMatrix& matrix) = 0;

  virtual void clipRect(const SkRect& rect,
                        SkClipOp op,
                        bool do_anti_alias) = 0;
  void clipRect(const SkRect& rect, SkClipOp op) { clipRect(rect, op, false); }
  void clipRect(const SkRect& rect, bool do_anti_alias) {
    clipRect(rect, SkClipOp::kIntersect, do_anti_alias);
  }
  void clipRect(const SkRect& rect) {
    clipRect(rect, SkClipOp::kIntersect, false);
  }

  virtual void clipRRect(const SkRRect& rrect,
                         SkClipOp op,
                         bool do_anti_alias) = 0;
  void clipRRect(const SkRRect& rrect, bool do_anti_alias) {
    clipRRect(rrect, SkClipOp::kIntersect, do_anti_alias);
  }
  void clipRRect(const SkRRect& rrect, SkClipOp op) {
    clipRRect(rrect, op, false);
  }
  void clipRRect(const SkRRect& rrect) {
    clipRRect(rrect, SkClipOp::kIntersect, false);
  }

  virtual void clipPath(const SkPath& path,
                        SkClipOp op,
                        bool do_anti_alias) = 0;
  void clipPath(const SkPath& path, SkClipOp op) { clipPath(path, op, false); }
  void clipPath(const SkPath& path, bool do_anti_alias) {
    clipPath(path, SkClipOp::kIntersect, do_anti_alias);
  }

  virtual bool quickReject(const SkRect& rect) const = 0;
  virtual bool quickReject(const SkPath& path) const = 0;
  virtual SkRect getLocalClipBounds() const = 0;
  virtual bool getLocalClipBounds(SkRect* bounds) const = 0;
  virtual SkIRect getDeviceClipBounds() const = 0;
  virtual bool getDeviceClipBounds(SkIRect* bounds) const = 0;
  virtual void drawColor(SkColor color, SkBlendMode mode) = 0;
  void drawColor(SkColor color) { drawColor(color, SkBlendMode::kSrcOver); }

  // TODO(enne): This is a synonym for drawColor with kSrc.  Remove it.
  virtual void clear(SkColor color) = 0;

  virtual void drawLine(SkScalar x0,
                        SkScalar y0,
                        SkScalar x1,
                        SkScalar y1,
                        const PaintFlags& flags) = 0;
  virtual void drawRect(const SkRect& rect, const PaintFlags& flags) = 0;
  virtual void drawIRect(const SkIRect& rect, const PaintFlags& flags) = 0;
  virtual void drawOval(const SkRect& oval, const PaintFlags& flags) = 0;
  virtual void drawRRect(const SkRRect& rrect, const PaintFlags& flags) = 0;
  virtual void drawDRRect(const SkRRect& outer,
                          const SkRRect& inner,
                          const PaintFlags& flags) = 0;
  virtual void drawCircle(SkScalar cx,
                          SkScalar cy,
                          SkScalar radius,
                          const PaintFlags& flags) = 0;
  virtual void drawArc(const SkRect& oval,
                       SkScalar start_angle,
                       SkScalar sweep_angle,
                       bool use_center,
                       const PaintFlags& flags) = 0;
  virtual void drawRoundRect(const SkRect& rect,
                             SkScalar rx,
                             SkScalar ry,
                             const PaintFlags& flags) = 0;
  virtual void drawPath(const SkPath& path, const PaintFlags& flags) = 0;
  virtual void drawImage(const PaintImage& image,
                         SkScalar left,
                         SkScalar top,
                         const PaintFlags* flags) = 0;
  void drawImage(const PaintImage& image, SkScalar left, SkScalar top) {
    drawImage(image, left, top, nullptr);
  }

  enum SrcRectConstraint {
    kStrict_SrcRectConstraint = SkCanvas::kStrict_SrcRectConstraint,
    kFast_SrcRectConstraint = SkCanvas::kFast_SrcRectConstraint,
  };

  virtual void drawImageRect(const PaintImage& image,
                             const SkRect& src,
                             const SkRect& dst,
                             const PaintFlags* flags,
                             SrcRectConstraint constraint) = 0;
  virtual void drawBitmap(const SkBitmap& bitmap,
                          SkScalar left,
                          SkScalar top,
                          const PaintFlags* flags) = 0;
  void drawBitmap(const SkBitmap& bitmap, SkScalar left, SkScalar top) {
    drawBitmap(bitmap, left, top, nullptr);
  }

  virtual void drawText(const void* text,
                        size_t byte_length,
                        SkScalar x,
                        SkScalar y,
                        const PaintFlags& flags) = 0;
  virtual void drawPosText(const void* text,
                           size_t byte_length,
                           const SkPoint pos[],
                           const PaintFlags& flags) = 0;
  virtual void drawTextBlob(sk_sp<SkTextBlob> blob,
                            SkScalar x,
                            SkScalar y,
                            const PaintFlags& flags) = 0;

  virtual void drawDisplayItemList(
      scoped_refptr<DisplayItemList> display_item_list) = 0;

  // Unlike SkCanvas::drawPicture, this only plays back the PaintRecord and does
  // not add an additional clip.  This is closer to SkPicture::playback.
  virtual void drawPicture(sk_sp<const PaintRecord> record) = 0;

  virtual bool isClipEmpty() const = 0;
  virtual bool isClipRect() const = 0;
  virtual const SkMatrix& getTotalMatrix() const = 0;

  enum class AnnotationType {
    URL,
    NAMED_DESTINATION,
    LINK_TO_DESTINATION,
  };
  virtual void Annotate(AnnotationType type,
                        const SkRect& rect,
                        sk_sp<SkData> data) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(PaintCanvas);
};

class CC_PAINT_EXPORT PaintCanvasAutoRestore {
 public:
  PaintCanvasAutoRestore(PaintCanvas* canvas, bool save) : canvas_(canvas) {
    if (canvas_) {
      save_count_ = canvas_->getSaveCount();
      if (save) {
        canvas_->save();
      }
    }
  }

  ~PaintCanvasAutoRestore() {
    if (canvas_) {
      canvas_->restoreToCount(save_count_);
    }
  }

  void restore() {
    if (canvas_) {
      canvas_->restoreToCount(save_count_);
      canvas_ = nullptr;
    }
  }

 private:
  PaintCanvas* canvas_ = nullptr;
  int save_count_ = 0;
};

// Following routines are used in print preview workflow to mark the
// preview metafile.
#if defined(OS_MACOSX)
CC_PAINT_EXPORT void SetIsPreviewMetafile(PaintCanvas* canvas, bool is_preview);
CC_PAINT_EXPORT bool IsPreviewMetafile(PaintCanvas* canvas);
#endif

}  // namespace cc

#endif  // CC_PAINT_PAINT_CANVAS_H_
