// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/paint_op_buffer.h"

#include "base/containers/stack_container.h"
#include "cc/paint/display_item_list.h"
#include "cc/paint/paint_record.h"
#include "third_party/skia/include/core/SkAnnotation.h"
#include "third_party/skia/include/core/SkCanvas.h"

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

using RasterFunction = void (*)(const PaintOp* op,
                                SkCanvas* canvas,
                                const SkMatrix& original_ctm);
using RasterWithFlagsFunction = void (*)(const PaintOpWithFlags* op,
                                         const PaintFlags* flags,
                                         SkCanvas* canvas,
                                         const SkMatrix& original_ctm);

NOINLINE static void RasterWithAlphaInternal(RasterFunction raster_fn,
                                             const PaintOp* op,
                                             SkCanvas* canvas,
                                             const SkRect& bounds,
                                             uint8_t alpha) {
  bool unset = bounds.x() == PaintOp::kUnsetRect.x();
  canvas->saveLayerAlpha(unset ? nullptr : &bounds, alpha);
  SkMatrix unused_matrix;
  raster_fn(op, canvas, unused_matrix);
  canvas->restore();
}

// Helper template to share common code for RasterWithAlpha when paint ops
// have or don't have PaintFlags.
template <typename T, bool HasFlags>
struct Rasterizer {
  static void RasterWithAlpha(const T* op,
                              SkCanvas* canvas,
                              const SkRect& bounds,
                              uint8_t alpha) {
    static_assert(
        !T::kHasPaintFlags,
        "This function should not be used for a PaintOp that has PaintFlags");
    DCHECK(T::kIsDrawOp);
    RasterWithAlphaInternal(&T::Raster, op, canvas, bounds, alpha);
  }
};

NOINLINE static void RasterWithAlphaInternalForFlags(
    RasterWithFlagsFunction raster_fn,
    const PaintOpWithFlags* op,
    SkCanvas* canvas,
    const SkRect& bounds,
    uint8_t alpha) {
  SkMatrix unused_matrix;
  if (alpha == 255) {
    raster_fn(op, &op->flags, canvas, unused_matrix);
  } else if (op->flags.SupportsFoldingAlpha()) {
    PaintFlags flags = op->flags;
    flags.setAlpha(SkMulDiv255Round(flags.getAlpha(), alpha));
    raster_fn(op, &flags, canvas, unused_matrix);
  } else {
    bool unset = bounds.x() == PaintOp::kUnsetRect.x();
    canvas->saveLayerAlpha(unset ? nullptr : &bounds, alpha);
    raster_fn(op, &op->flags, canvas, unused_matrix);
    canvas->restore();
  }
}

template <typename T>
struct Rasterizer<T, true> {
  static void RasterWithAlpha(const T* op,
                              SkCanvas* canvas,
                              const SkRect& bounds,
                              uint8_t alpha) {
    static_assert(T::kHasPaintFlags,
                  "This function expects the PaintOp to have PaintFlags");
    DCHECK(T::kIsDrawOp);
    RasterWithAlphaInternalForFlags(&T::RasterWithFlags, op, canvas, bounds,
                                    alpha);
  }
};

// These should never be used, as we should recurse into them to draw their
// contained op with alpha instead.
template <bool HasFlags>
struct Rasterizer<DrawRecordOp, HasFlags> {
  static void RasterWithAlpha(const DrawRecordOp* op,
                              SkCanvas* canvas,
                              const SkRect& bounds,
                              uint8_t alpha) {
    NOTREACHED();
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
#define M(T) &T::Raster,
static const RasterFunction g_raster_functions[kNumOpTypes] = {TYPES(M)};
#undef M

using RasterAlphaFunction = void (*)(const PaintOp* op,
                                     SkCanvas* canvas,
                                     const SkRect& bounds,
                                     uint8_t alpha);
#define M(T) \
  T::kIsDrawOp ? [](const PaintOp* op, SkCanvas* canvas, const SkRect& bounds, \
                    uint8_t alpha) { \
    Rasterizer<T, T::kHasPaintFlags>::RasterWithAlpha( \
        static_cast<const T*>(op), canvas, bounds, alpha); \
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

#define M(T)                                               \
  static_assert(alignof(T) <= PaintOpBuffer::PaintOpAlign, \
                #T " must have alignment no bigger than PaintOpAlign");
TYPES(M);
#undef M

#undef TYPES

SkRect PaintOp::kUnsetRect = {SK_ScalarInfinity, 0, 0, 0};

void AnnotateOp::Raster(const PaintOp* base_op,
                        SkCanvas* canvas,
                        const SkMatrix& original_ctm) {
  auto* op = static_cast<const AnnotateOp*>(base_op);
  switch (op->annotation_type) {
    case PaintCanvas::AnnotationType::URL:
      SkAnnotateRectWithURL(canvas, op->rect, op->data.get());
      break;
    case PaintCanvas::AnnotationType::LINK_TO_DESTINATION:
      SkAnnotateLinkToDestination(canvas, op->rect, op->data.get());
      break;
    case PaintCanvas::AnnotationType::NAMED_DESTINATION: {
      SkPoint point = SkPoint::Make(op->rect.x(), op->rect.y());
      SkAnnotateNamedDestination(canvas, point, op->data.get());
      break;
    }
  }
}

void ClipPathOp::Raster(const PaintOp* base_op,
                        SkCanvas* canvas,
                        const SkMatrix& original_ctm) {
  auto* op = static_cast<const ClipPathOp*>(base_op);
  canvas->clipPath(op->path, op->op, op->antialias);
}

void ClipRectOp::Raster(const PaintOp* base_op,
                        SkCanvas* canvas,
                        const SkMatrix& original_ctm) {
  auto* op = static_cast<const ClipRectOp*>(base_op);
  canvas->clipRect(op->rect, op->op, op->antialias);
}

void ClipRRectOp::Raster(const PaintOp* base_op,
                         SkCanvas* canvas,
                         const SkMatrix& original_ctm) {
  auto* op = static_cast<const ClipRRectOp*>(base_op);
  canvas->clipRRect(op->rrect, op->op, op->antialias);
}

void ConcatOp::Raster(const PaintOp* base_op,
                      SkCanvas* canvas,
                      const SkMatrix& original_ctm) {
  auto* op = static_cast<const ConcatOp*>(base_op);
  canvas->concat(op->matrix);
}

void DrawArcOp::RasterWithFlags(const PaintOpWithFlags* base_op,
                                const PaintFlags* flags,
                                SkCanvas* canvas,
                                const SkMatrix& original_ctm) {
  auto* op = static_cast<const DrawArcOp*>(base_op);
  canvas->drawArc(op->oval, op->start_angle, op->sweep_angle, op->use_center,
                  ToSkPaint(*flags));
}

void DrawCircleOp::RasterWithFlags(const PaintOpWithFlags* base_op,
                                   const PaintFlags* flags,
                                   SkCanvas* canvas,
                                   const SkMatrix& original_ctm) {
  auto* op = static_cast<const DrawCircleOp*>(base_op);
  canvas->drawCircle(op->cx, op->cy, op->radius, ToSkPaint(*flags));
}

void DrawColorOp::Raster(const PaintOp* base_op,
                         SkCanvas* canvas,
                         const SkMatrix& original_ctm) {
  auto* op = static_cast<const DrawColorOp*>(base_op);
  canvas->drawColor(op->color, op->mode);
}

void DrawDRRectOp::RasterWithFlags(const PaintOpWithFlags* base_op,
                                   const PaintFlags* flags,
                                   SkCanvas* canvas,
                                   const SkMatrix& original_ctm) {
  auto* op = static_cast<const DrawDRRectOp*>(base_op);
  canvas->drawDRRect(op->outer, op->inner, ToSkPaint(*flags));
}

void DrawImageOp::RasterWithFlags(const PaintOpWithFlags* base_op,
                                  const PaintFlags* flags,
                                  SkCanvas* canvas,
                                  const SkMatrix& original_ctm) {
  auto* op = static_cast<const DrawImageOp*>(base_op);
  canvas->drawImage(op->image.sk_image().get(), op->left, op->top,
                    ToSkPaint(flags));
}

void DrawImageRectOp::RasterWithFlags(const PaintOpWithFlags* base_op,
                                      const PaintFlags* flags,
                                      SkCanvas* canvas,
                                      const SkMatrix& original_ctm) {
  auto* op = static_cast<const DrawImageRectOp*>(base_op);
  // TODO(enne): Probably PaintCanvas should just use the skia enum directly.
  SkCanvas::SrcRectConstraint skconstraint =
      static_cast<SkCanvas::SrcRectConstraint>(op->constraint);
  canvas->drawImageRect(op->image.sk_image().get(), op->src, op->dst,
                        ToSkPaint(flags), skconstraint);
}

void DrawIRectOp::RasterWithFlags(const PaintOpWithFlags* base_op,
                                  const PaintFlags* flags,
                                  SkCanvas* canvas,
                                  const SkMatrix& original_ctm) {
  auto* op = static_cast<const DrawIRectOp*>(base_op);
  canvas->drawIRect(op->rect, ToSkPaint(*flags));
}

void DrawLineOp::RasterWithFlags(const PaintOpWithFlags* base_op,
                                 const PaintFlags* flags,
                                 SkCanvas* canvas,
                                 const SkMatrix& original_ctm) {
  auto* op = static_cast<const DrawLineOp*>(base_op);
  canvas->drawLine(op->x0, op->y0, op->x1, op->y1, ToSkPaint(*flags));
}

void DrawOvalOp::RasterWithFlags(const PaintOpWithFlags* base_op,
                                 const PaintFlags* flags,
                                 SkCanvas* canvas,
                                 const SkMatrix& original_ctm) {
  auto* op = static_cast<const DrawOvalOp*>(base_op);
  canvas->drawOval(op->oval, ToSkPaint(*flags));
}

void DrawPathOp::RasterWithFlags(const PaintOpWithFlags* base_op,
                                 const PaintFlags* flags,
                                 SkCanvas* canvas,
                                 const SkMatrix& original_ctm) {
  auto* op = static_cast<const DrawPathOp*>(base_op);
  canvas->drawPath(op->path, ToSkPaint(*flags));
}

void DrawPosTextOp::RasterWithFlags(const PaintOpWithFlags* base_op,
                                    const PaintFlags* flags,
                                    SkCanvas* canvas,
                                    const SkMatrix& original_ctm) {
  auto* op = static_cast<const DrawPosTextOp*>(base_op);
  canvas->drawPosText(op->GetData(), op->bytes, op->GetArray(),
                      ToSkPaint(*flags));
}

void DrawRecordOp::Raster(const PaintOp* base_op,
                          SkCanvas* canvas,
                          const SkMatrix& original_ctm) {
  // Don't use drawPicture here, as it adds an implicit clip.
  auto* op = static_cast<const DrawRecordOp*>(base_op);
  op->record->Playback(canvas);
}

void DrawRectOp::RasterWithFlags(const PaintOpWithFlags* base_op,
                                 const PaintFlags* flags,
                                 SkCanvas* canvas,
                                 const SkMatrix& original_ctm) {
  auto* op = static_cast<const DrawRectOp*>(base_op);
  canvas->drawRect(op->rect, ToSkPaint(*flags));
}

void DrawRRectOp::RasterWithFlags(const PaintOpWithFlags* base_op,
                                  const PaintFlags* flags,
                                  SkCanvas* canvas,
                                  const SkMatrix& original_ctm) {
  auto* op = static_cast<const DrawRRectOp*>(base_op);
  canvas->drawRRect(op->rrect, ToSkPaint(*flags));
}

void DrawTextOp::RasterWithFlags(const PaintOpWithFlags* base_op,
                                 const PaintFlags* flags,
                                 SkCanvas* canvas,
                                 const SkMatrix& original_ctm) {
  auto* op = static_cast<const DrawTextOp*>(base_op);
  canvas->drawText(op->GetData(), op->bytes, op->x, op->y, ToSkPaint(*flags));
}

void DrawTextBlobOp::RasterWithFlags(const PaintOpWithFlags* base_op,
                                     const PaintFlags* flags,
                                     SkCanvas* canvas,
                                     const SkMatrix& original_ctm) {
  auto* op = static_cast<const DrawTextBlobOp*>(base_op);
  canvas->drawTextBlob(op->blob.get(), op->x, op->y, ToSkPaint(*flags));
}

void RestoreOp::Raster(const PaintOp* base_op,
                       SkCanvas* canvas,
                       const SkMatrix& original_ctm) {
  canvas->restore();
}

void RotateOp::Raster(const PaintOp* base_op,
                      SkCanvas* canvas,
                      const SkMatrix& original_ctm) {
  auto* op = static_cast<const RotateOp*>(base_op);
  canvas->rotate(op->degrees);
}

void SaveOp::Raster(const PaintOp* base_op,
                    SkCanvas* canvas,
                    const SkMatrix& original_ctm) {
  canvas->save();
}

void SaveLayerOp::RasterWithFlags(const PaintOpWithFlags* base_op,
                                  const PaintFlags* flags,
                                  SkCanvas* canvas,
                                  const SkMatrix& original_ctm) {
  auto* op = static_cast<const SaveLayerOp*>(base_op);
  // See PaintOp::kUnsetRect
  bool unset = op->bounds.left() == SK_ScalarInfinity;

  canvas->saveLayer(unset ? nullptr : &op->bounds, ToSkPaint(flags));
}

void SaveLayerAlphaOp::Raster(const PaintOp* base_op,
                              SkCanvas* canvas,
                              const SkMatrix& original_ctm) {
  auto* op = static_cast<const SaveLayerAlphaOp*>(base_op);
  // See PaintOp::kUnsetRect
  bool unset = op->bounds.left() == SK_ScalarInfinity;
  if (op->preserve_lcd_text_requests) {
    SkPaint paint;
    paint.setAlpha(op->alpha);
    canvas->saveLayerPreserveLCDTextRequests(unset ? nullptr : &op->bounds,
                                             &paint);
  } else {
    canvas->saveLayerAlpha(unset ? nullptr : &op->bounds, op->alpha);
  }
}

void ScaleOp::Raster(const PaintOp* base_op,
                     SkCanvas* canvas,
                     const SkMatrix& original_ctm) {
  auto* op = static_cast<const ScaleOp*>(base_op);
  canvas->scale(op->sx, op->sy);
}

void SetMatrixOp::Raster(const PaintOp* base_op,
                         SkCanvas* canvas,
                         const SkMatrix& original_ctm) {
  auto* op = static_cast<const SetMatrixOp*>(base_op);
  canvas->setMatrix(SkMatrix::Concat(original_ctm, op->matrix));
}

void TranslateOp::Raster(const PaintOp* base_op,
                         SkCanvas* canvas,
                         const SkMatrix& original_ctm) {
  auto* op = static_cast<const TranslateOp*>(base_op);
  canvas->translate(op->dx, op->dy);
}

bool PaintOp::IsDrawOp() const {
  return g_is_draw_op[type];
}

void PaintOp::Raster(SkCanvas* canvas, const SkMatrix& original_ctm) const {
  g_raster_functions[type](this, canvas, original_ctm);
}

void PaintOp::RasterWithAlpha(SkCanvas* canvas,
                              const SkRect& bounds,
                              uint8_t alpha) const {
  g_raster_alpha_functions[type](this, canvas, bounds, alpha);
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

int DrawRecordOp::CountSlowPaths() const {
  return record->numSlowPaths();
}

AnnotateOp::AnnotateOp(PaintCanvas::AnnotationType annotation_type,
                       const SkRect& rect,
                       sk_sp<SkData> data)
    : annotation_type(annotation_type), rect(rect), data(std::move(data)) {}

AnnotateOp::~AnnotateOp() = default;

DrawImageOp::DrawImageOp(const PaintImage& image,
                         SkScalar left,
                         SkScalar top,
                         const PaintFlags* flags)
    : PaintOpWithFlags(flags ? *flags : PaintFlags()),
      image(image),
      left(left),
      top(top) {}

bool DrawImageOp::HasDiscardableImages() const {
  // TODO(khushalsagar): Callers should not be able to change the lazy generated
  // state for a PaintImage.
  return image.sk_image()->isLazyGenerated();
}

DrawImageOp::~DrawImageOp() = default;

DrawImageRectOp::DrawImageRectOp(const PaintImage& image,
                                 const SkRect& src,
                                 const SkRect& dst,
                                 const PaintFlags* flags,
                                 PaintCanvas::SrcRectConstraint constraint)
    : PaintOpWithFlags(flags ? *flags : PaintFlags()),
      image(image),
      src(src),
      dst(dst),
      constraint(constraint) {}

bool DrawImageRectOp::HasDiscardableImages() const {
  return image.sk_image()->isLazyGenerated();
}

DrawImageRectOp::~DrawImageRectOp() = default;

DrawPosTextOp::DrawPosTextOp(size_t bytes,
                             size_t count,
                             const PaintFlags& flags)
    : PaintOpWithArray(flags, bytes, count) {}

DrawPosTextOp::~DrawPosTextOp() = default;

DrawRecordOp::DrawRecordOp(sk_sp<const PaintRecord> record)
    : record(std::move(record)) {}

DrawRecordOp::~DrawRecordOp() = default;

size_t DrawRecordOp::AdditionalBytesUsed() const {
  return record->bytes_used();
}

bool DrawRecordOp::HasDiscardableImages() const {
  return record->HasDiscardableImages();
}

DrawTextBlobOp::DrawTextBlobOp(sk_sp<SkTextBlob> blob,
                               SkScalar x,
                               SkScalar y,
                               const PaintFlags& flags)
    : PaintOpWithFlags(flags), blob(std::move(blob)), x(x), y(y) {}

DrawTextBlobOp::~DrawTextBlobOp() = default;

PaintOpBuffer::PaintOpBuffer() = default;

PaintOpBuffer::PaintOpBuffer(PaintOpBuffer&& other) {
  *this = std::move(other);
}

PaintOpBuffer::~PaintOpBuffer() {
  Reset();
}

void PaintOpBuffer::operator=(PaintOpBuffer&& other) {
  data_ = std::move(other.data_);
  used_ = other.used_;
  reserved_ = other.reserved_;
  op_count_ = other.op_count_;
  num_slow_paths_ = other.num_slow_paths_;
  subrecord_bytes_used_ = other.subrecord_bytes_used_;
  has_discardable_images_ = other.has_discardable_images_;

  // Make sure the other pob can destruct safely.
  other.used_ = 0;
  other.op_count_ = 0;
  other.reserved_ = 0;
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

// When |op| is a nested PaintOpBuffer, this returns the PaintOp inside
// that buffer if the buffer contains a single drawing op, otherwise it
// returns null. This searches recursively if the PaintOpBuffer contains only
// another PaintOpBuffer.
static const PaintOp* GetNestedSingleDrawingOp(const PaintOp* op) {
  if (!op->IsDrawOp())
    return nullptr;

  while (op->GetType() == PaintOpType::DrawRecord) {
    auto* draw_record_op = static_cast<const DrawRecordOp*>(op);
    if (draw_record_op->record->size() > 1) {
      // If there's more than one op, then we need to keep the
      // SaveLayer.
      return nullptr;
    }

    // Recurse into the single-op DrawRecordOp and make sure it's a
    // drawing op.
    op = draw_record_op->record->GetFirstOp();
    if (!op->IsDrawOp())
      return nullptr;
  }

  return op;
}

void PaintOpBuffer::Playback(SkCanvas* canvas,
                             SkPicture::AbortCallback* callback,
                             const std::vector<size_t>* indices) const {
  if (!op_count_)
    return;
  if (indices && indices->empty())
    return;

  // Prevent PaintOpBuffers from having side effects back into the canvas.
  SkAutoCanvasRestore save_restore(canvas, true);

  // TODO(enne): a PaintRecord that contains a SetMatrix assumes that the
  // SetMatrix is local to that PaintRecord itself.  Said differently, if you
  // translate(x, y), then draw a paint record with a SetMatrix(identity),
  // the translation should be preserved instead of clobbering the top level
  // transform.  This could probably be done more efficiently.
  SkMatrix original = canvas->getTotalMatrix();

  // FIFO queue of paint ops that have been peeked at.
  base::StackVector<const PaintOp*, 3> stack;
  Iterator iter(this, indices);
  auto next_op = [&stack, &iter]() -> const PaintOp* {
    if (stack->size()) {
      const PaintOp* op = stack->front();
      // Shift paintops forward.
      stack->erase(stack->begin());
      return op;
    }
    if (!iter)
      return nullptr;
    const PaintOp* op = *iter;
    ++iter;
    return op;
  };

  while (const PaintOp* op = next_op()) {
    // Check if we should abort. This should happen at the start of loop since
    // there are a couple of raster branches below, and we need to ensure that
    // we do this check after every one of them.
    if (callback && callback->abort())
      return;

    // Optimize out save/restores or save/draw/restore that can be a single
    // draw.  See also: similar code in SkRecordOpts.
    // TODO(enne): consider making this recursive?
    // TODO(enne): should we avoid this if the SaveLayerAlphaOp has bounds?
    if (op->GetType() == PaintOpType::SaveLayerAlpha) {
      const PaintOp* second = next_op();
      const PaintOp* third = nullptr;
      if (second) {
        if (second->GetType() == PaintOpType::Restore) {
          continue;
        }

        // Find a nested drawing PaintOp to replace |second| if possible, while
        // holding onto the pointer to |second| in case we can't find a nested
        // drawing op to replace it with.
        const PaintOp* draw_op = GetNestedSingleDrawingOp(second);

        if (draw_op) {
          third = next_op();
          if (third && third->GetType() == PaintOpType::Restore) {
            auto* save_op = static_cast<const SaveLayerAlphaOp*>(op);
            draw_op->RasterWithAlpha(canvas, save_op->bounds, save_op->alpha);
            continue;
          }
        }

        // Store deferred ops for later.
        stack->push_back(second);
        if (third)
          stack->push_back(third);
      }
    }
    // TODO(enne): skip SaveLayer followed by restore with nothing in
    // between, however SaveLayer with image filters on it (or maybe
    // other PaintFlags options) are not a noop.  Figure out what these
    // are so we can skip them correctly.

    op->Raster(canvas, original);
  }
}

void PaintOpBuffer::ReallocBuffer(size_t new_size) {
  DCHECK_GE(new_size, used_);
  std::unique_ptr<char, base::AlignedFreeDeleter> new_data(
      static_cast<char*>(base::AlignedAlloc(new_size, PaintOpAlign)));
  memcpy(new_data.get(), data_.get(), used_);
  data_ = std::move(new_data);
  reserved_ = new_size;
}

std::pair<void*, size_t> PaintOpBuffer::AllocatePaintOp(size_t sizeof_op,
                                                        size_t bytes) {
  // Compute a skip such that all ops in the buffer are aligned to the
  // maximum required alignment of all ops.
  size_t skip = MathUtil::UncheckedRoundUp(sizeof_op + bytes, PaintOpAlign);
  DCHECK_LT(skip, static_cast<size_t>(1) << 24);
  if (used_ + skip > reserved_) {
    // Start reserved_ at kInitialBufferSize and then double.
    // ShrinkToFit can make this smaller afterwards.
    size_t new_size = reserved_ ? reserved_ : kInitialBufferSize;
    while (used_ + skip > new_size)
      new_size *= 2;
    ReallocBuffer(new_size);
  }
  DCHECK_LE(used_ + skip, reserved_);

  void* op = data_.get() + used_;
  used_ += skip;
  op_count_++;
  return std::make_pair(op, skip);
}

void PaintOpBuffer::ShrinkToFit() {
  if (!used_ || used_ == reserved_)
    return;
  ReallocBuffer(used_);
}

}  // namespace cc
