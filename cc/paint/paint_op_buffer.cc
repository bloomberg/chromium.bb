// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/paint_op_buffer.h"

#include "cc/paint/display_item_list.h"
#include "cc/paint/paint_record.h"
#include "third_party/skia/include/core/SkAnnotation.h"

namespace cc {

#define TYPES(M)           \
  M(AnnotateOp)            \
  M(ClipPathOp)            \
  M(ClipRectOp)            \
  M(ClipRRectOp)           \
  M(ConcatOp)              \
  M(DrawArcOp)             \
  M(DrawCircleOp)          \
  M(DrawColorOp)           \
  M(DrawDisplayItemListOp) \
  M(DrawDRRectOp)          \
  M(DrawImageOp)           \
  M(DrawImageRectOp)       \
  M(DrawIRectOp)           \
  M(DrawLineOp)            \
  M(DrawOvalOp)            \
  M(DrawPathOp)            \
  M(DrawPosTextOp)         \
  M(DrawRecordOp)          \
  M(DrawRectOp)            \
  M(DrawRRectOp)           \
  M(DrawTextOp)            \
  M(DrawTextBlobOp)        \
  M(NoopOp)                \
  M(RestoreOp)             \
  M(RotateOp)              \
  M(SaveOp)                \
  M(SaveLayerOp)           \
  M(SaveLayerAlphaOp)      \
  M(ScaleOp)               \
  M(SetMatrixOp)           \
  M(TranslateOp)

// Helper template to share common code for RasterWithAlpha when paint ops
// have or don't have PaintFlags.
template <typename T, bool HasFlags>
struct Rasterizer {
  static void Raster(const T* op,
                     SkCanvas* canvas,
                     const SkMatrix& original_ctm) {
    // Paint ops with kHasPaintFlags need to declare RasterWithPaintFlags
    // otherwise, the paint op needs its own Raster function.  Without its
    // own, this becomes an infinite loop as PaintOp::Raster calls itself.
    static_assert(
        !std::is_same<decltype(&PaintOp::Raster), decltype(&T::Raster)>::value,
        "No Raster function");

    op->Raster(canvas);
  }
  static void RasterWithAlpha(const T* op, SkCanvas* canvas, uint8_t alpha) {
    DCHECK(T::kIsDrawOp);
    // TODO(enne): is it ok to just drop the bounds here?
    canvas->saveLayerAlpha(nullptr, alpha);
    op->Raster(canvas);
    canvas->restore();
  }
};

template <typename T>
struct Rasterizer<T, true> {
  static void Raster(const T* op,
                     SkCanvas* canvas,
                     const SkMatrix& original_ctm) {
    op->RasterWithFlags(canvas, op->flags);
  }
  static void RasterWithAlpha(const T* op, SkCanvas* canvas, uint8_t alpha) {
    DCHECK(T::kIsDrawOp);
    SkMatrix unused_matrix;
    if (alpha == 255) {
      Raster(op, canvas, unused_matrix);
    } else if (op->flags.SupportsFoldingAlpha()) {
      PaintFlags flags = op->flags;
      flags.setAlpha(SkMulDiv255Round(flags.getAlpha(), alpha));
      op->RasterWithFlags(canvas, flags);
    } else {
      canvas->saveLayerAlpha(nullptr, alpha);
      op->RasterWithFlags(canvas, op->flags);
      canvas->restore();
    }
  }
};

template <>
struct Rasterizer<SetMatrixOp, false> {
  static void Raster(const SetMatrixOp* op,
                     SkCanvas* canvas,
                     const SkMatrix& original_ctm) {
    op->Raster(canvas, original_ctm);
  }
  static void RasterWithAlpha(const SetMatrixOp* op,
                              SkCanvas* canvas,
                              uint8_t alpha) {
    NOTREACHED();
  }
};

template <>
struct Rasterizer<DrawRecordOp, false> {
  static void Raster(const DrawRecordOp* op,
                     SkCanvas* canvas,
                     const SkMatrix& original_ctm) {
    op->Raster(canvas);
  }
  static void RasterWithAlpha(const DrawRecordOp* op,
                              SkCanvas* canvas,
                              uint8_t alpha) {
    // This "looking into records" optimization is done here instead of
    // in the PaintOpBuffer::Raster function as DisplayItemList calls
    // into RasterWithAlpha directly.
    if (op->record->approximateOpCount() == 1) {
      op->record->GetFirstOp()->RasterWithAlpha(canvas, alpha);
      return;
    }

    canvas->saveLayerAlpha(nullptr, alpha);
    op->Raster(canvas);
    canvas->restore();
  }
};

// TODO(enne): partially specialize RasterWithAlpha for draw color?

static constexpr size_t kNumOpTypes =
    static_cast<size_t>(PaintOpType::LastPaintOpType) + 1;

// Verify that every op is in the TYPES macro.
#define M(T) +1
static_assert(kNumOpTypes == TYPES(M), "Missing op in list");
#undef M

using RasterFunction = void (*)(const PaintOp* op,
                                SkCanvas* canvas,
                                const SkMatrix& original_ctm);
#define M(T)                                                              \
  [](const PaintOp* op, SkCanvas* canvas, const SkMatrix& original_ctm) { \
    Rasterizer<T, T::kHasPaintFlags>::Raster(static_cast<const T*>(op),   \
                                             canvas, original_ctm);       \
  },
static const RasterFunction g_raster_functions[kNumOpTypes] = {TYPES(M)};
#undef M

using RasterAlphaFunction = void (*)(const PaintOp* op,
                                     SkCanvas* canvas,
                                     uint8_t alpha);
#define M(T) \
  T::kIsDrawOp ? \
  [](const PaintOp* op, SkCanvas* canvas, uint8_t alpha) { \
    Rasterizer<T, T::kHasPaintFlags>::RasterWithAlpha( \
        static_cast<const T*>(op), canvas, alpha); \
  } : static_cast<RasterAlphaFunction>(nullptr),
static const RasterAlphaFunction g_raster_alpha_functions[kNumOpTypes] = {
    TYPES(M)};
#undef M

// Most state ops (matrix, clip, save, restore) have a trivial destructor.
// TODO(enne): evaluate if we need the nullptr optimization or if
// we even need to differentiate trivial destructors here.
using VoidFunction = void (*)(PaintOp* op);
#define M(T)                                           \
  !std::is_trivially_destructible<T>::value            \
      ? [](PaintOp* op) { static_cast<T*>(op)->~T(); } \
      : static_cast<VoidFunction>(nullptr),
static const VoidFunction g_destructor_functions[kNumOpTypes] = {TYPES(M)};
#undef M

#define M(T) T::kIsDrawOp,
static bool g_is_draw_op[kNumOpTypes] = {TYPES(M)};
#undef M

#define M(T)                                         \
  static_assert(sizeof(T) <= sizeof(LargestPaintOp), \
                #T " must be no bigger than LargestPaintOp");
TYPES(M);
#undef M

#undef TYPES

SkRect PaintOp::kUnsetRect = {SK_ScalarInfinity, 0, 0, 0};

void AnnotateOp::Raster(SkCanvas* canvas) const {
  switch (annotation_type) {
    case PaintCanvas::AnnotationType::URL:
      SkAnnotateRectWithURL(canvas, rect, data.get());
      break;
    case PaintCanvas::AnnotationType::LINK_TO_DESTINATION:
      SkAnnotateLinkToDestination(canvas, rect, data.get());
      break;
    case PaintCanvas::AnnotationType::NAMED_DESTINATION: {
      SkPoint point = SkPoint::Make(rect.x(), rect.y());
      SkAnnotateNamedDestination(canvas, point, data.get());
      break;
    }
  }
}

void ClipPathOp::Raster(SkCanvas* canvas) const {
  canvas->clipPath(path, op, antialias);
}

void ClipRectOp::Raster(SkCanvas* canvas) const {
  canvas->clipRect(rect, op, antialias);
}

void ClipRRectOp::Raster(SkCanvas* canvas) const {
  canvas->clipRRect(rrect, op, antialias);
}

void ConcatOp::Raster(SkCanvas* canvas) const {
  canvas->concat(matrix);
}

void DrawArcOp::RasterWithFlags(SkCanvas* canvas,
                                const PaintFlags& flags) const {
  canvas->drawArc(oval, start_angle, sweep_angle, use_center, ToSkPaint(flags));
}

void DrawCircleOp::RasterWithFlags(SkCanvas* canvas,
                                   const PaintFlags& flags) const {
  canvas->drawCircle(cx, cy, radius, ToSkPaint(flags));
}

void DrawColorOp::Raster(SkCanvas* canvas) const {
  canvas->drawColor(color, mode);
}

void DrawDisplayItemListOp::Raster(SkCanvas* canvas) const {
  list->Raster(canvas, nullptr);
}

void DrawDRRectOp::RasterWithFlags(SkCanvas* canvas,
                                   const PaintFlags& flags) const {
  canvas->drawDRRect(outer, inner, ToSkPaint(flags));
}

void DrawImageOp::RasterWithFlags(SkCanvas* canvas,
                                  const PaintFlags& flags) const {
  canvas->drawImage(image.get(), left, top, ToSkPaint(&flags));
}

void DrawImageRectOp::RasterWithFlags(SkCanvas* canvas,
                                      const PaintFlags& flags) const {
  // TODO(enne): Probably PaintCanvas should just use the skia enum directly.
  SkCanvas::SrcRectConstraint skconstraint =
      static_cast<SkCanvas::SrcRectConstraint>(constraint);
  canvas->drawImageRect(image.get(), src, dst, ToSkPaint(&flags), skconstraint);
}

void DrawIRectOp::RasterWithFlags(SkCanvas* canvas,
                                  const PaintFlags& flags) const {
  canvas->drawIRect(rect, ToSkPaint(flags));
}

void DrawLineOp::RasterWithFlags(SkCanvas* canvas,
                                 const PaintFlags& flags) const {
  canvas->drawLine(x0, y0, x1, y1, ToSkPaint(flags));
}

void DrawOvalOp::RasterWithFlags(SkCanvas* canvas,
                                 const PaintFlags& flags) const {
  canvas->drawOval(oval, ToSkPaint(flags));
}

void DrawPathOp::RasterWithFlags(SkCanvas* canvas,
                                 const PaintFlags& flags) const {
  canvas->drawPath(path, ToSkPaint(flags));
}

void DrawPosTextOp::RasterWithFlags(SkCanvas* canvas,
                                    const PaintFlags& flags) const {
  canvas->drawPosText(paint_op_data(this), bytes, paint_op_array<SkPoint>(this),
                      ToSkPaint(flags));
}

void DrawRecordOp::Raster(SkCanvas* canvas) const {
  record->playback(canvas);
}

void DrawRectOp::RasterWithFlags(SkCanvas* canvas,
                                 const PaintFlags& flags) const {
  canvas->drawRect(rect, ToSkPaint(flags));
}

void DrawRRectOp::RasterWithFlags(SkCanvas* canvas,
                                  const PaintFlags& flags) const {
  canvas->drawRRect(rrect, ToSkPaint(flags));
}

void DrawTextOp::RasterWithFlags(SkCanvas* canvas,
                                 const PaintFlags& flags) const {
  canvas->drawText(paint_op_data(this), bytes, x, y, ToSkPaint(flags));
}

void DrawTextBlobOp::RasterWithFlags(SkCanvas* canvas,
                                     const PaintFlags& flags) const {
  canvas->drawTextBlob(blob.get(), x, y, ToSkPaint(flags));
}

void RestoreOp::Raster(SkCanvas* canvas) const {
  canvas->restore();
}

void RotateOp::Raster(SkCanvas* canvas) const {
  canvas->rotate(degrees);
}

void SaveOp::Raster(SkCanvas* canvas) const {
  canvas->save();
}

void SaveLayerOp::RasterWithFlags(SkCanvas* canvas,
                                  const PaintFlags& flags) const {
  // See PaintOp::kUnsetRect
  bool unset = bounds.left() == SK_ScalarInfinity;

  canvas->saveLayer(unset ? nullptr : &bounds, ToSkPaint(&flags));
}

void SaveLayerAlphaOp::Raster(SkCanvas* canvas) const {
  // See PaintOp::kUnsetRect
  bool unset = bounds.left() == SK_ScalarInfinity;
  canvas->saveLayerAlpha(unset ? nullptr : &bounds, alpha);
}

void ScaleOp::Raster(SkCanvas* canvas) const {
  canvas->scale(sx, sy);
}

void SetMatrixOp::Raster(SkCanvas* canvas, const SkMatrix& original_ctm) const {
  canvas->setMatrix(SkMatrix::Concat(original_ctm, matrix));
}

void TranslateOp::Raster(SkCanvas* canvas) const {
  canvas->translate(dx, dy);
}

bool PaintOp::IsDrawOp() const {
  return g_is_draw_op[type];
}

void PaintOp::Raster(SkCanvas* canvas, const SkMatrix& original_ctm) const {
  g_raster_functions[type](this, canvas, original_ctm);
}

void PaintOp::RasterWithAlpha(SkCanvas* canvas, uint8_t alpha) const {
  g_raster_alpha_functions[type](this, canvas, alpha);
}

DrawDisplayItemListOp::DrawDisplayItemListOp(
    scoped_refptr<DisplayItemList> list)
    : list(list) {}

size_t DrawDisplayItemListOp::AdditionalBytesUsed() const {
  return list->ApproximateMemoryUsage();
}

int ClipPathOp::CountSlowPaths() const {
  return antialias && !path.isConvex() ? 1 : 0;
}

int DrawLineOp::CountSlowPaths() const {
  if (const SkPathEffect* effect = flags.getPathEffect()) {
    SkPathEffect::DashInfo info;
    SkPathEffect::DashType dashType = effect->asADash(&info);
    if (flags.getStrokeCap() != PaintFlags::kRound_Cap &&
        dashType == SkPathEffect::kDash_DashType && info.fCount == 2) {
      // The PaintFlags will count this as 1, so uncount that here as
      // this kind of line is special cased and not slow.
      return -1;
    }
  }
  return 0;
}

int DrawPathOp::CountSlowPaths() const {
  // This logic is copied from SkPathCounter instead of attempting to expose
  // that from Skia.
  if (!flags.isAntiAlias() || path.isConvex())
    return 0;

  PaintFlags::Style paintStyle = flags.getStyle();
  const SkRect& pathBounds = path.getBounds();
  if (paintStyle == PaintFlags::kStroke_Style && flags.getStrokeWidth() == 0) {
    // AA hairline concave path is not slow.
    return 0;
  } else if (paintStyle == PaintFlags::kFill_Style &&
             pathBounds.width() < 64.f && pathBounds.height() < 64.f &&
             !path.isVolatile()) {
    // AADF eligible concave path is not slow.
    return 0;
  } else {
    return 1;
  }
}

AnnotateOp::AnnotateOp(PaintCanvas::AnnotationType annotation_type,
                       const SkRect& rect,
                       sk_sp<SkData> data)
    : annotation_type(annotation_type), rect(rect), data(std::move(data)) {}

AnnotateOp::~AnnotateOp() = default;

DrawDisplayItemListOp::~DrawDisplayItemListOp() = default;

DrawImageOp::DrawImageOp(sk_sp<const SkImage> image,
                         SkScalar left,
                         SkScalar top,
                         const PaintFlags* flags)
    : image(std::move(image)),
      left(left),
      top(top),
      flags(flags ? *flags : PaintFlags()) {}

DrawImageOp::~DrawImageOp() = default;

DrawImageRectOp::DrawImageRectOp(sk_sp<const SkImage> image,
                                 const SkRect& src,
                                 const SkRect& dst,
                                 const PaintFlags* flags,
                                 PaintCanvas::SrcRectConstraint constraint)
    : image(std::move(image)),
      flags(flags ? *flags : PaintFlags()),
      src(src),
      dst(dst),
      constraint(constraint) {}

DrawImageRectOp::~DrawImageRectOp() = default;

DrawPosTextOp::DrawPosTextOp(size_t bytes,
                             size_t count,
                             const PaintFlags& flags)
    : PaintOpWithDataArray(bytes, count), flags(flags) {}

DrawPosTextOp::~DrawPosTextOp() = default;

DrawRecordOp::DrawRecordOp(sk_sp<const PaintRecord> record)
    : record(std::move(record)) {}

DrawRecordOp::~DrawRecordOp() = default;

size_t DrawRecordOp::AdditionalBytesUsed() const {
  return record->approximateBytesUsed();
}

DrawTextBlobOp::DrawTextBlobOp(sk_sp<SkTextBlob> blob,
                               SkScalar x,
                               SkScalar y,
                               const PaintFlags& flags)
    : blob(std::move(blob)), x(x), y(y), flags(flags) {}

DrawTextBlobOp::~DrawTextBlobOp() = default;

PaintOpBuffer::PaintOpBuffer() : cull_rect_(SkRect::MakeEmpty()) {}

PaintOpBuffer::PaintOpBuffer(const SkRect& cull_rect) : cull_rect_(cull_rect) {}

PaintOpBuffer::~PaintOpBuffer() {
  Reset();
}

void PaintOpBuffer::Reset() {
  for (auto* op : Iterator(this)) {
    auto func = g_destructor_functions[op->type];
    if (func)
      func(op);
  }

  // Leave data_ allocated, reserved_ unchanged.
  used_ = 0;
  op_count_ = 0;
  num_slow_paths_ = 0;
}

void PaintOpBuffer::playback(SkCanvas* canvas) const {
  // TODO(enne): a PaintRecord that contains a SetMatrix assumes that the
  // SetMatrix is local to that PaintRecord itself.  Said differently, if you
  // translate(x, y), then draw a paint record with a SetMatrix(identity),
  // the translation should be preserved instead of clobbering the top level
  // transform.  This could probably be done more efficiently.
  SkMatrix original = canvas->getTotalMatrix();

  for (Iterator iter(this); iter; ++iter) {
    // Optimize out save/restores or save/draw/restore that can be a single
    // draw.  See also: similar code in SkRecordOpts and cc's DisplayItemList.
    // TODO(enne): consider making this recursive?
    const PaintOp* op = *iter;
    if (op->GetType() == PaintOpType::SaveLayerAlpha) {
      const PaintOp* second = iter.peek1();
      if (second) {
        if (second->GetType() == PaintOpType::Restore) {
          ++iter;
          continue;
        }
        if (second->IsDrawOp()) {
          const PaintOp* third = iter.peek2();
          if (third && third->GetType() == PaintOpType::Restore) {
            const SaveLayerAlphaOp* save_op =
                static_cast<const SaveLayerAlphaOp*>(op);
            second->RasterWithAlpha(canvas, save_op->alpha);
            ++iter;
            ++iter;
            continue;
          }
        }
      }
    }
    // TODO(enne): skip SaveLayer followed by restore with nothing in
    // between, however SaveLayer with image filters on it (or maybe
    // other PaintFlags options) are not a noop.  Figure out what these
    // are so we can skip them correctly.

    op->Raster(canvas, original);
  }
}

void PaintOpBuffer::playback(SkCanvas* canvas,
                             SkPicture::AbortCallback* callback) const {
  // The abort callback is only used for analysis, in general, so
  // this playback code can be more straightforward and not do the
  // optimizations in the other function.
  if (!callback) {
    playback(canvas);
    return;
  }

  SkMatrix original = canvas->getTotalMatrix();

  // TODO(enne): ideally callers would just iterate themselves and we
  // can remove the entire notion of an abort callback.
  for (auto* op : Iterator(this)) {
    op->Raster(canvas, original);
    if (callback && callback->abort())
      return;
  }
}

void PaintOpBuffer::ShrinkToFit() {
  if (!used_ || used_ == reserved_)
    return;
  data_.realloc(used_);
  reserved_ = used_;
}

}  // namespace cc
