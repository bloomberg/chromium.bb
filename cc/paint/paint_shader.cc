// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/paint_shader.h"

#include "base/memory/ptr_util.h"
#include "cc/paint/paint_record.h"
#include "third_party/skia/include/effects/SkGradientShader.h"

namespace cc {

sk_sp<PaintShader> PaintShader::MakeColor(SkColor color) {
  sk_sp<PaintShader> shader(new PaintShader(Type::kColor));

  // Just one color. Store it in the fallback color. Easy.
  shader->fallback_color_ = color;

  shader->CreateSkShader();
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
  sk_sp<PaintShader> shader(new PaintShader(Type::kLinearGradient));

  // There are always two points, the start and the end.
  shader->start_point_ = points[0];
  shader->end_point_ = points[1];
  shader->SetColorsAndPositions(colors, pos, count);
  shader->SetMatrixAndTiling(local_matrix, mode, mode);
  shader->SetFlagsAndFallback(flags, fallback_color);

  shader->CreateSkShader();
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
  sk_sp<PaintShader> shader(new PaintShader(Type::kRadialGradient));

  shader->center_ = center;
  shader->start_radius_ = shader->end_radius_ = radius;
  shader->SetColorsAndPositions(colors, pos, count);
  shader->SetMatrixAndTiling(local_matrix, mode, mode);
  shader->SetFlagsAndFallback(flags, fallback_color);

  shader->CreateSkShader();
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
  sk_sp<PaintShader> shader(new PaintShader(Type::kTwoPointConicalGradient));

  shader->start_point_ = start;
  shader->end_point_ = end;
  shader->start_radius_ = start_radius;
  shader->end_radius_ = end_radius;
  shader->SetColorsAndPositions(colors, pos, count);
  shader->SetMatrixAndTiling(local_matrix, mode, mode);
  shader->SetFlagsAndFallback(flags, fallback_color);

  shader->CreateSkShader();
  return shader;
}

sk_sp<PaintShader> PaintShader::MakeSweepGradient(SkScalar cx,
                                                  SkScalar cy,
                                                  const SkColor colors[],
                                                  const SkScalar pos[],
                                                  int color_count,
                                                  SkShader::TileMode mode,
                                                  SkScalar start_degrees,
                                                  SkScalar end_degrees,
                                                  uint32_t flags,
                                                  const SkMatrix* local_matrix,
                                                  SkColor fallback_color) {
  sk_sp<PaintShader> shader(new PaintShader(Type::kSweepGradient));

  shader->center_ = SkPoint::Make(cx, cy);
  shader->start_degrees_ = start_degrees;
  shader->end_degrees_ = end_degrees;
  shader->SetColorsAndPositions(colors, pos, color_count);
  shader->SetMatrixAndTiling(local_matrix, mode, mode);
  shader->SetFlagsAndFallback(flags, fallback_color);

  shader->CreateSkShader();
  return shader;
}

sk_sp<PaintShader> PaintShader::MakeImage(const PaintImage& image,
                                          SkShader::TileMode tx,
                                          SkShader::TileMode ty,
                                          const SkMatrix* local_matrix) {
  sk_sp<PaintShader> shader(new PaintShader(Type::kImage));

  shader->image_ = image;
  shader->SetMatrixAndTiling(local_matrix, tx, ty);

  shader->CreateSkShader();
  return shader;
}

sk_sp<PaintShader> PaintShader::MakePaintRecord(
    sk_sp<PaintRecord> record,
    const SkRect& tile,
    SkShader::TileMode tx,
    SkShader::TileMode ty,
    const SkMatrix* local_matrix,
    ScalingBehavior scaling_behavior) {
  sk_sp<PaintShader> shader(new PaintShader(Type::kPaintRecord));

  shader->record_ = std::move(record);
  shader->tile_ = tile;
  shader->scaling_behavior_ = scaling_behavior;
  shader->SetMatrixAndTiling(local_matrix, tx, ty);

  shader->CreateSkShader();
  return shader;
}

PaintShader::PaintShader(Type type) : shader_type_(type) {}
PaintShader::~PaintShader() = default;

sk_sp<SkShader> PaintShader::GetSkShader() const {
  return cached_shader_;
}

void PaintShader::CreateSkShader() {
  DCHECK(!cached_shader_);

  switch (shader_type_) {
    case Type::kColor:
      // This will be handled by the fallback check below.
      break;
    case Type::kLinearGradient: {
      SkPoint points[2] = {start_point_, end_point_};
      cached_shader_ = SkGradientShader::MakeLinear(
          points, colors_.data(),
          positions_.empty() ? nullptr : positions_.data(),
          static_cast<int>(colors_.size()), tx_, flags_,
          local_matrix_ ? &*local_matrix_ : nullptr);
      break;
    }
    case Type::kRadialGradient:
      cached_shader_ = SkGradientShader::MakeRadial(
          center_, start_radius_, colors_.data(),
          positions_.empty() ? nullptr : positions_.data(),
          static_cast<int>(colors_.size()), tx_, flags_,
          local_matrix_ ? &*local_matrix_ : nullptr);
      break;
    case Type::kTwoPointConicalGradient:
      cached_shader_ = SkGradientShader::MakeTwoPointConical(
          start_point_, start_radius_, end_point_, end_radius_, colors_.data(),
          positions_.empty() ? nullptr : positions_.data(),
          static_cast<int>(colors_.size()), tx_, flags_,
          local_matrix_ ? &*local_matrix_ : nullptr);
      break;
    case Type::kSweepGradient:
      cached_shader_ = SkGradientShader::MakeSweep(
          center_.x(), center_.y(), colors_.data(),
          positions_.empty() ? nullptr : positions_.data(),
          static_cast<int>(colors_.size()), tx_, start_degrees_, end_degrees_,
          flags_, local_matrix_ ? &*local_matrix_ : nullptr);
      break;
    case Type::kImage:
      cached_shader_ = image_.GetSkImage()->makeShader(
          tx_, ty_, local_matrix_ ? &*local_matrix_ : nullptr);
      break;
    case Type::kPaintRecord: {
      auto picture = ToSkPicture(record_, tile_);

      switch (scaling_behavior_) {
        // For raster scale, we create a picture shader directly.
        case ScalingBehavior::kRasterAtScale:
          cached_shader_ = SkShader::MakePictureShader(
              std::move(picture), tx_, ty_,
              local_matrix_ ? &*local_matrix_ : nullptr, nullptr);
          break;
        // For fixed scale, we create an image shader with and image backed by
        // the picture.
        case ScalingBehavior::kFixedScale: {
          auto image = SkImage::MakeFromPicture(
              std::move(picture), SkISize::Make(tile_.width(), tile_.height()),
              nullptr, nullptr, SkImage::BitDepth::kU8,
              SkColorSpace::MakeSRGB());
          cached_shader_ = image->makeShader(
              tx_, ty_, local_matrix_ ? &*local_matrix_ : nullptr);
          break;
        }
      }
      break;
    }
    case Type::kShaderCount:
      NOTREACHED();
      break;
  }

  // If we didn't create a shader for whatever reason, create a fallback color
  // one.
  if (!cached_shader_)
    cached_shader_ = SkShader::MakeColorShader(fallback_color_);
}

void PaintShader::SetColorsAndPositions(const SkColor* colors,
                                        const SkScalar* positions,
                                        int count) {
#if DCHECK_IS_ON()
  static const int kMaxShaderColorsSupported = 10000;
  DCHECK_GE(count, 2);
  DCHECK_LE(count, kMaxShaderColorsSupported);
#endif
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

bool PaintShader::IsValid() const {
  // If we managed to create a shader already, then we should be valid.
  if (cached_shader_)
    return true;

  switch (shader_type_) {
    case Type::kColor:
      return true;
    case Type::kSweepGradient:
      if (!std::isfinite(start_degrees_) || !std::isfinite(end_degrees_) ||
          start_degrees_ >= end_degrees_) {
        return false;
      }
    // Fallthrough.
    case Type::kLinearGradient:
    case Type::kRadialGradient:
    case Type::kTwoPointConicalGradient:
      return colors_.size() >= 2 &&
             (positions_.empty() || positions_.size() == colors_.size());
    case Type::kImage:
      return !!image_;
    case Type::kPaintRecord:
      return !!record_;
    case Type::kShaderCount:
      return false;
  }
  return false;
}

}  // namespace cc
