// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/solid_color_analyzer.h"

#include "base/trace_event/trace_event.h"
#include "cc/paint/paint_op_buffer.h"
#include "third_party/skia/include/utils/SkNoDrawCanvas.h"

namespace cc {
namespace {
bool ActsLikeClear(SkBlendMode mode, unsigned src_alpha) {
  switch (mode) {
    case SkBlendMode::kClear:
      return true;
    case SkBlendMode::kSrc:
    case SkBlendMode::kSrcIn:
    case SkBlendMode::kDstIn:
    case SkBlendMode::kSrcOut:
    case SkBlendMode::kDstATop:
      return src_alpha == 0;
    case SkBlendMode::kDstOut:
      return src_alpha == 0xFF;
    default:
      return false;
  }
}

bool IsSolidColor(SkColor color, SkBlendMode blendmode) {
  return SkColorGetA(color) == 255 &&
         (blendmode == SkBlendMode::kSrc || blendmode == SkBlendMode::kSrcOver);
}

bool IsSolidColorPaint(const PaintFlags& flags) {
  SkBlendMode blendmode = flags.getBlendMode();

  // Paint is solid color if the following holds:
  // - Alpha is 1.0, style is fill, and there are no special effects
  // - Xfer mode is either kSrc or kSrcOver (kSrcOver is equivalent
  //   to kSrc if source alpha is 1.0, which is already checked).
  return IsSolidColor(flags.getColor(), blendmode) && !flags.HasShader() &&
         !flags.getLooper() && !flags.getMaskFilter() &&
         !flags.getColorFilter() && !flags.getImageFilter() &&
         flags.getStyle() == PaintFlags::kFill_Style;
}

// Returns true if the specified drawn_rect will cover the entire canvas, and
// that the canvas is not clipped (i.e. it covers ALL of the canvas).
bool IsFullQuad(const SkCanvas& canvas, const SkRect& drawn_rect) {
  if (!canvas.isClipRect())
    return false;

  SkIRect clip_irect;
  if (!canvas.getDeviceClipBounds(&clip_irect))
    return false;

  // if the clip is smaller than the canvas, we're partly clipped, so abort.
  if (!clip_irect.contains(SkIRect::MakeSize(canvas.getBaseLayerSize())))
    return false;

  const SkMatrix& matrix = canvas.getTotalMatrix();
  // If the transform results in a non-axis aligned
  // rect, then be conservative and return false.
  if (!matrix.rectStaysRect())
    return false;

  SkRect device_rect;
  matrix.mapRect(&device_rect, drawn_rect);
  SkRect clip_rect;
  clip_rect.set(clip_irect);
  return device_rect.contains(clip_rect);
}

void CheckIfSolidColor(const SkCanvas& canvas,
                       SkColor color,
                       SkBlendMode blendmode,
                       bool* is_solid_color,
                       bool* is_transparent,
                       SkColor* out_color) {
  SkRect rect;
  if (!canvas.getLocalClipBounds(&rect)) {
    *is_transparent = false;
    *is_solid_color = false;
    return;
  }

  bool does_cover_canvas = IsFullQuad(canvas, rect);
  uint8_t alpha = SkColorGetA(color);
  if (does_cover_canvas && ActsLikeClear(blendmode, alpha))
    *is_transparent = true;
  else if (alpha != 0 || blendmode != SkBlendMode::kSrc)
    *is_transparent = false;

  if (does_cover_canvas && IsSolidColor(color, blendmode)) {
    *is_solid_color = true;
    *out_color = color;
  } else {
    *is_solid_color = false;
  }
}

void CheckIfSolidRect(const SkCanvas& canvas,
                      const SkRect& rect,
                      const PaintFlags& flags,
                      bool* is_solid_color,
                      bool* is_transparent,
                      SkColor* color) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "SolidColorAnalyzer::HandleDrawRect");
  if (flags.nothingToDraw())
    return;

  bool does_cover_canvas = IsFullQuad(canvas, rect);
  SkBlendMode blendmode = flags.getBlendMode();
  if (does_cover_canvas && ActsLikeClear(blendmode, flags.getAlpha()))
    *is_transparent = true;
  else if (flags.getAlpha() != 0 || blendmode != SkBlendMode::kSrc)
    *is_transparent = false;

  if (does_cover_canvas && IsSolidColorPaint(flags)) {
    *is_solid_color = true;
    *color = flags.getColor();
  } else {
    *is_solid_color = false;
  }
}

}  // namespace

base::Optional<SkColor> SolidColorAnalyzer::DetermineIfSolidColor(
    const PaintOpBuffer* buffer,
    const gfx::Rect& rect,
    int max_ops_to_analyze,
    const std::vector<size_t>* offsets) {
  if (buffer->size() == 0 || (offsets && offsets->empty()))
    return SK_ColorTRANSPARENT;

  bool is_solid = false;
  bool is_transparent = true;
  SkColor color = SK_ColorTRANSPARENT;

  struct Frame {
    Frame() = default;
    Frame(PaintOpBuffer::CompositeIterator iter,
          const SkMatrix& original_ctm,
          int save_count)
        : iter(iter), original_ctm(original_ctm), save_count(save_count) {}

    PaintOpBuffer::CompositeIterator iter;
    const SkMatrix original_ctm;
    int save_count = 0;
  };

  SkNoDrawCanvas canvas(rect.width(), rect.height());
  canvas.translate(-rect.x(), -rect.y());
  canvas.clipRect(gfx::RectToSkRect(rect), SkClipOp::kIntersect, false);

  std::vector<Frame> stack;
  // We expect to see at least one DrawRecordOp because of the way items are
  // constructed. Reserve this to 2, and go from there.
  stack.reserve(2);
  stack.emplace_back(PaintOpBuffer::CompositeIterator(buffer, offsets),
                     canvas.getTotalMatrix(), canvas.getSaveCount());

  int num_ops = 0;
  while (!stack.empty()) {
    auto& frame = stack.back();
    if (!frame.iter) {
      canvas.restoreToCount(frame.save_count);
      stack.pop_back();
      if (!stack.empty())
        ++stack.back().iter;
      continue;
    }

    const PaintOp* op = *frame.iter;
    const SkMatrix& original_ctm = frame.original_ctm;
    switch (op->GetType()) {
      case PaintOpType::DrawRecord: {
        const DrawRecordOp* record_op = static_cast<const DrawRecordOp*>(op);
        stack.emplace_back(
            PaintOpBuffer::CompositeIterator(record_op->record.get(), nullptr),
            canvas.getTotalMatrix(), canvas.getSaveCount());
        continue;
      }

      // Any of the following ops result in non solid content.
      case PaintOpType::DrawArc:
      case PaintOpType::DrawCircle:
      case PaintOpType::DrawDRRect:
      case PaintOpType::DrawImage:
      case PaintOpType::DrawImageRect:
      case PaintOpType::DrawIRect:
      case PaintOpType::DrawLine:
      case PaintOpType::DrawOval:
      case PaintOpType::DrawPath:
      case PaintOpType::DrawPosText:
      case PaintOpType::DrawRRect:
      case PaintOpType::DrawText:
      case PaintOpType::DrawTextBlob:
      // Anything that has to do a save layer is probably not solid. As it will
      // likely need more than one draw op.
      // TODO(vmpstr): We could investigate handling these.
      case PaintOpType::SaveLayer:
      case PaintOpType::SaveLayerAlpha:
      // Complex clips will probably result in non solid color as it might not
      // cover the canvas.
      // TODO(vmpstr): We could investigate handling these.
      case PaintOpType::ClipPath:
      case PaintOpType::ClipRRect:
        return base::nullopt;

      case PaintOpType::DrawRect: {
        if (++num_ops > max_ops_to_analyze)
          return base::nullopt;
        const DrawRectOp* rect_op = static_cast<const DrawRectOp*>(op);
        CheckIfSolidRect(canvas, rect_op->rect, rect_op->flags, &is_solid,
                         &is_transparent, &color);
        break;
      }
      case PaintOpType::DrawColor: {
        if (++num_ops > max_ops_to_analyze)
          return base::nullopt;
        const DrawColorOp* color_op = static_cast<const DrawColorOp*>(op);
        CheckIfSolidColor(canvas, color_op->color, color_op->mode, &is_solid,
                          &is_transparent, &color);
        break;
      }

      // The rest of the ops should only affect our state canvas.
      case PaintOpType::Annotate:
      case PaintOpType::ClipRect:
      case PaintOpType::Concat:
      case PaintOpType::Scale:
      case PaintOpType::SetMatrix:
      case PaintOpType::Restore:
      case PaintOpType::Rotate:
      case PaintOpType::Save:
      case PaintOpType::Translate:
      case PaintOpType::Noop:
        op->Raster(&canvas, original_ctm);
        break;
    }
    ++frame.iter;
  }

  if (is_transparent)
    return SK_ColorTRANSPARENT;
  if (is_solid)
    return color;
  return base::nullopt;
}

}  // namespace cc
