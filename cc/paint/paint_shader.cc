// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/paint_shader.h"

#include "base/memory/ptr_util.h"
#include "cc/paint/paint_record.h"
#include "third_party/skia/include/effects/SkGradientShader.h"

namespace cc {

sk_sp<PaintShader> PaintShader::MakeColor(SkColor color) {
  sk_sp<PaintShader> shader(new PaintShader(kColor));

  // Just one color. Store it in the fallback color. Easy.
  shader->fallback_color_ = color;

  return shader;
}

sk_sp<PaintShader> PaintShader::MakeLinearGradient(const SkPoint points[],
                                                   const SkColor colors[],
                                                   const SkScalar pos[],
                                                   int count,
                                                   SkShader::TileMode mode,
                                                   uint32_t flags,
                                                   const SkMatrix* local_matrix,
                                                   SkColor fallback_color) {
  sk_sp<PaintShader> shader(new PaintShader(kLinearGradient));

  // There are always two points, the start and the end.
  shader->start_point_ = points[0];
  shader->end_point_ = points[1];
  shader->SetColorsAndPositions(colors, pos, count);
  shader->SetMatrixAndTiling(local_matrix, mode, mode);
  shader->SetFlagsAndFallback(flags, fallback_color);

  return shader;
}

sk_sp<PaintShader> PaintShader::MakeRadialGradient(const SkPoint& center,
                                                   SkScalar radius,
                                                   const SkColor colors[],
                                                   const SkScalar pos[],
                                                   int count,
                                                   SkShader::TileMode mode,
                                                   uint32_t flags,
                                                   const SkMatrix* local_matrix,
                                                   SkColor fallback_color) {
  sk_sp<PaintShader> shader(new PaintShader(kRadialGradient));

  shader->center_ = center;
  shader->start_radius_ = shader->end_radius_ = radius;
  shader->SetColorsAndPositions(colors, pos, count);
  shader->SetMatrixAndTiling(local_matrix, mode, mode);
  shader->SetFlagsAndFallback(flags, fallback_color);

  return shader;
}

sk_sp<PaintShader> PaintShader::MakeTwoPointConicalGradient(
    const SkPoint& start,
    SkScalar start_radius,
    const SkPoint& end,
    SkScalar end_radius,
    const SkColor colors[],
    const SkScalar pos[],
    int count,
    SkShader::TileMode mode,
    uint32_t flags,
    const SkMatrix* local_matrix,
    SkColor fallback_color) {
  sk_sp<PaintShader> shader(new PaintShader(kTwoPointConicalGradient));

  shader->start_point_ = start;
  shader->end_point_ = end;
  shader->start_radius_ = start_radius;
  shader->end_radius_ = end_radius;
  shader->SetColorsAndPositions(colors, pos, count);
  shader->SetMatrixAndTiling(local_matrix, mode, mode);
  shader->SetFlagsAndFallback(flags, fallback_color);

  return shader;
}

sk_sp<PaintShader> PaintShader::MakeSweepGradient(SkScalar cx,
                                                  SkScalar cy,
                                                  const SkColor colors[],
                                                  const SkScalar pos[],
                                                  int color_count,
                                                  uint32_t flags,
                                                  const SkMatrix* local_matrix,
                                                  SkColor fallback_color) {
  sk_sp<PaintShader> shader(new PaintShader(kSweepGradient));

  shader->center_ = SkPoint::Make(cx, cy);
  shader->SetColorsAndPositions(colors, pos, color_count);
  shader->SetMatrixAndTiling(local_matrix, SkShader::kClamp_TileMode,
                             SkShader::kClamp_TileMode);
  shader->SetFlagsAndFallback(flags, fallback_color);

  return shader;
}

sk_sp<PaintShader> PaintShader::MakeImage(sk_sp<const SkImage> image,
                                          SkShader::TileMode tx,
                                          SkShader::TileMode ty,
                                          const SkMatrix* local_matrix) {
  sk_sp<PaintShader> shader(new PaintShader(kImage));

  shader->image_ = std::move(image);
  shader->SetMatrixAndTiling(local_matrix, tx, ty);

  return shader;
}

sk_sp<PaintShader> PaintShader::MakePaintRecord(sk_sp<PaintRecord> record,
                                                const SkRect& tile,
                                                SkShader::TileMode tx,
                                                SkShader::TileMode ty,
                                                const SkMatrix* local_matrix) {
  sk_sp<PaintShader> shader(new PaintShader(kPaintRecord));

  shader->record_ = std::move(record);
  shader->tile_ = tile;
  shader->SetMatrixAndTiling(local_matrix, tx, ty);

  return shader;
}

PaintShader::PaintShader(Type type) : shader_type_(type) {}
PaintShader::~PaintShader() = default;

sk_sp<SkShader> PaintShader::GetSkShader() const {
  if (cached_shader_)
    return cached_shader_;

  switch (shader_type_) {
    case kColor:
      // This will be handled by the fallback check below.
      break;
    case kLinearGradient: {
      SkPoint points[2] = {start_point_, end_point_};
      cached_shader_ = SkGradientShader::MakeLinear(
          points, colors_.data(),
          positions_.empty() ? nullptr : positions_.data(),
          static_cast<int>(colors_.size()), tx_, flags_,
          local_matrix_ ? &*local_matrix_ : nullptr);
      break;
    }
    case kRadialGradient:
      cached_shader_ = SkGradientShader::MakeRadial(
          center_, start_radius_, colors_.data(),
          positions_.empty() ? nullptr : positions_.data(),
          static_cast<int>(colors_.size()), tx_, flags_,
          local_matrix_ ? &*local_matrix_ : nullptr);
      break;
    case kTwoPointConicalGradient:
      cached_shader_ = SkGradientShader::MakeTwoPointConical(
          start_point_, start_radius_, end_point_, end_radius_, colors_.data(),
          positions_.empty() ? nullptr : positions_.data(),
          static_cast<int>(colors_.size()), tx_, flags_,
          local_matrix_ ? &*local_matrix_ : nullptr);
      break;
    case kSweepGradient:
      cached_shader_ = SkGradientShader::MakeSweep(
          center_.x(), center_.y(), colors_.data(),
          positions_.empty() ? nullptr : positions_.data(),
          static_cast<int>(colors_.size()), flags_,
          local_matrix_ ? &*local_matrix_ : nullptr);
      break;
    case kImage:
      cached_shader_ = image_->makeShader(
          tx_, ty_, local_matrix_ ? &*local_matrix_ : nullptr);
      break;
    case kPaintRecord:
      cached_shader_ = SkShader::MakePictureShader(
          ToSkPicture(record_, tile_), tx_, ty_,
          local_matrix_ ? &*local_matrix_ : nullptr, nullptr);
      break;
    case kShaderCount:
      NOTREACHED();
      break;
  }

  // If we didn't create a shader for whatever reason, create a fallback color
  // one.
  if (!cached_shader_)
    cached_shader_ = SkShader::MakeColorShader(fallback_color_);
  return cached_shader_;
}

void PaintShader::SetColorsAndPositions(const SkColor* colors,
                                        const SkScalar* positions,
                                        int count) {
  DCHECK_GE(count, 2);
  colors_.assign(colors, colors + count);
  if (positions)
    positions_.assign(positions, positions + count);
}

void PaintShader::SetMatrixAndTiling(const SkMatrix* matrix,
                                     SkShader::TileMode tx,
                                     SkShader::TileMode ty) {
  if (matrix)
    local_matrix_ = *matrix;
  tx_ = tx;
  ty_ = ty;
}

void PaintShader::SetFlagsAndFallback(uint32_t flags, SkColor fallback_color) {
  flags_ = flags;
  fallback_color_ = fallback_color;
}

bool PaintShader::IsOpaque() const {
  // TODO(enne): don't create a shader to answer this.
  return GetSkShader()->isOpaque();
}

}  // namespace cc
