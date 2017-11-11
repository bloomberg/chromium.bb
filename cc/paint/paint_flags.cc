// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/paint_flags.h"

#include "cc/paint/paint_op_buffer.h"

namespace {

static bool affects_alpha(const SkColorFilter* cf) {
  return cf && !(cf->getFlags() & SkColorFilter::kAlphaUnchanged_Flag);
}

}  // namespace

namespace cc {

PaintFlags::PaintFlags() {
  // Match SkPaint defaults.
  bitfields_uint_ = 0u;
  bitfields_.cap_type_ = SkPaint::kDefault_Cap;
  bitfields_.join_type_ = SkPaint::kDefault_Join;
  bitfields_.style_ = SkPaint::kFill_Style;
  bitfields_.text_encoding_ = SkPaint::kUTF8_TextEncoding;
  bitfields_.hinting_ = SkPaint::kNormal_Hinting;
  bitfields_.filter_quality_ = SkFilterQuality::kNone_SkFilterQuality;

  static_assert(sizeof(bitfields_) <= sizeof(bitfields_uint_),
                "Too many bitfields");
}

PaintFlags::PaintFlags(const PaintFlags& flags) = default;

PaintFlags::~PaintFlags() = default;

bool PaintFlags::nothingToDraw() const {
  // Duplicated from SkPaint to avoid having to construct an SkPaint to
  // answer this question.
  if (getLooper())
    return false;

  switch (getBlendMode()) {
    case SkBlendMode::kSrcOver:
    case SkBlendMode::kSrcATop:
    case SkBlendMode::kDstOut:
    case SkBlendMode::kDstOver:
    case SkBlendMode::kPlus:
      if (getAlpha() == 0) {
        return !affects_alpha(color_filter_.get()) && !image_filter_;
      }
      break;
    case SkBlendMode::kDst:
      return true;
    default:
      break;
  }
  return false;
}

bool PaintFlags::getFillPath(const SkPath& src,
                             SkPath* dst,
                             const SkRect* cull_rect,
                             SkScalar res_scale) const {
  SkPaint paint = ToSkPaint();
  return paint.getFillPath(src, dst, cull_rect, res_scale);
}

bool PaintFlags::IsSimpleOpacity() const {
  uint32_t color = getColor();
  if (SK_ColorTRANSPARENT != SkColorSetA(color, SK_AlphaTRANSPARENT))
    return false;
  if (getBlendMode() != SkBlendMode::kSrcOver)
    return false;
  if (getLooper())
    return false;
  if (getPathEffect())
    return false;
  if (HasShader())
    return false;
  if (getMaskFilter())
    return false;
  if (getColorFilter())
    return false;
  if (getImageFilter())
    return false;
  return true;
}

bool PaintFlags::SupportsFoldingAlpha() const {
  if (getBlendMode() != SkBlendMode::kSrcOver)
    return false;
  if (getColorFilter())
    return false;
  if (getImageFilter())
    return false;
  if (getLooper())
    return false;
  return true;
}

SkPaint PaintFlags::ToSkPaint() const {
  SkPaint paint;
  paint.setTypeface(typeface_);
  paint.setPathEffect(path_effect_);
  if (shader_)
    paint.setShader(shader_->GetSkShader());
  paint.setMaskFilter(mask_filter_);
  paint.setColorFilter(color_filter_);
  paint.setDrawLooper(draw_looper_);
  paint.setImageFilter(image_filter_);
  paint.setTextSize(text_size_);
  paint.setColor(color_);
  paint.setStrokeWidth(width_);
  paint.setStrokeMiter(miter_limit_);
  paint.setBlendMode(getBlendMode());
  paint.setFlags(bitfields_.flags_);
  paint.setStrokeCap(static_cast<SkPaint::Cap>(getStrokeCap()));
  paint.setStrokeJoin(static_cast<SkPaint::Join>(getStrokeJoin()));
  paint.setStyle(static_cast<SkPaint::Style>(getStyle()));
  paint.setTextEncoding(static_cast<SkPaint::TextEncoding>(getTextEncoding()));
  paint.setHinting(static_cast<SkPaint::Hinting>(getHinting()));
  paint.setFilterQuality(getFilterQuality());
  return paint;
}

bool PaintFlags::IsValid() const {
  return PaintOp::IsValidPaintFlagsSkBlendMode(getBlendMode());
}

bool PaintFlags::HasDiscardableImages() const {
  if (!shader_)
    return false;
  else if (shader_->shader_type() == PaintShader::Type::kImage)
    return shader_->paint_image().IsLazyGenerated();
  else if (shader_->shader_type() == PaintShader::Type::kPaintRecord)
    return shader_->paint_record()->HasDiscardableImages();
  return false;
}

}  // namespace cc
