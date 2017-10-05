// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/paint_shader.h"

#include "base/memory/ptr_util.h"
#include "cc/paint/paint_record.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "third_party/skia/include/effects/SkGradientShader.h"

namespace cc {
namespace {

sk_sp<SkPicture> ToSkPicture(sk_sp<PaintRecord> record,
                             const SkRect& bounds,
                             const SkMatrix* matrix,
                             ImageProvider* image_provider) {
  SkPictureRecorder recorder;
  SkCanvas* canvas = recorder.beginRecording(bounds);
  if (matrix)
    canvas->setMatrix(*matrix);
  record->Playback(canvas, image_provider);
  return recorder.finishRecordingAsPicture();
}

}  // namespace

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

bool PaintShader::GetRasterizationTileRect(const SkMatrix& ctm,
                                           SkRect* tile_rect) const {
  DCHECK_EQ(shader_type_, Type::kPaintRecord);

  // If we are using a fixed scale, the record is rasterized with the original
  // tile size and scaling is applied to the generated output.
  if (scaling_behavior_ == ScalingBehavior::kFixedScale) {
    *tile_rect = tile_;
    return true;
  }

  SkMatrix matrix = ctm;
  if (local_matrix_.has_value())
    matrix.preConcat(local_matrix_.value());

  SkSize scale;
  if (!matrix.decomposeScale(&scale)) {
    // Decomposition failed, use an approximation.
    scale.set(SkScalarSqrt(matrix.getScaleX() * matrix.getScaleX() +
                           matrix.getSkewX() * matrix.getSkewX()),
              SkScalarSqrt(matrix.getScaleY() * matrix.getScaleY() +
                           matrix.getSkewY() * matrix.getSkewY()));
  }
  SkSize scaled_size =
      SkSize::Make(SkScalarAbs(scale.width() * tile_.width()),
                   SkScalarAbs(scale.height() * tile_.height()));

  // Clamp the tile size to about 4M pixels.
  // TODO(khushalsagar): We need to consider the max texture size as well.
  static const SkScalar kMaxTileArea = 2048 * 2048;
  SkScalar tile_area = scaled_size.width() * scaled_size.height();
  if (tile_area > kMaxTileArea) {
    SkScalar clamp_scale = SkScalarSqrt(kMaxTileArea / tile_area);
    scaled_size.set(scaled_size.width() * clamp_scale,
                    scaled_size.height() * clamp_scale);
  }

  scaled_size = scaled_size.toCeil();
  if (scaled_size.isEmpty())
    return false;

  *tile_rect = SkRect::MakeWH(scaled_size.width(), scaled_size.height());
  return true;
}

sk_sp<PaintShader> PaintShader::CreateDecodedPaintRecord(
    const SkMatrix& ctm,
    ImageProvider* image_provider) const {
  DCHECK_EQ(shader_type_, Type::kPaintRecord);

  // For creating a decoded PaintRecord shader, we need to do the following:
  // 1) Figure out the scale at which the record should be rasterization given
  //    the ctm and local_matrix on the shader.
  // 2) Transform this record to an SkPicture with this scale and replace
  //    encoded images in this record with decodes from the ImageProvider. This
  //    is done by setting the raster_matrix_ for this shader to be used
  //    in GetSkShader.
  // 3) Since the SkShader will use a scaled SkPicture, we use a kFixedScale for
  //    the decoded shader which creates an SkPicture backed SkImage for
  //    creating the decoded SkShader.
  // Note that the scaling logic here is replicated from
  // SkPictureShader::refBitmapShader.
  SkRect tile_rect;
  if (!GetRasterizationTileRect(ctm, &tile_rect))
    return nullptr;

  sk_sp<PaintShader> shader(new PaintShader(Type::kPaintRecord));
  shader->record_ = record_;
  shader->tile_ = tile_rect;
  // Use a fixed scale since we have already scaled the tile rect and fixed the
  // raster scale.
  shader->scaling_behavior_ = ScalingBehavior::kFixedScale;
  shader->tx_ = tx_;
  shader->ty_ = ty_;

  const SkSize tile_scale =
      SkSize::Make(SkIntToScalar(tile_rect.width()) / tile_.width(),
                   SkIntToScalar(tile_rect.height()) / tile_.height());
  shader->local_matrix_ = GetLocalMatrix();
  shader->local_matrix_->preScale(1 / tile_scale.width(),
                                  1 / tile_scale.height());

  SkMatrix raster_matrix =
      SkMatrix::MakeRectToRect(tile_, tile_rect, SkMatrix::kFill_ScaleToFit);
  shader->CreateSkShader(image_provider, &raster_matrix);
  return shader;
}

sk_sp<SkShader> PaintShader::GetSkShader() const {
  return cached_shader_;
}

void PaintShader::CreateSkShader(ImageProvider* image_provider,
                                 const SkMatrix* raster_matrix) {
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
      // Create a recording at the desired scale if this record has images which
      // have been decoded before raster.
      auto picture = ToSkPicture(record_, tile_, raster_matrix, image_provider);

      switch (scaling_behavior_) {
        // For raster scale, we create a picture shader directly.
        case ScalingBehavior::kRasterAtScale:
          cached_shader_ = SkShader::MakePictureShader(
              std::move(picture), tx_, ty_,
              local_matrix_ ? &*local_matrix_ : nullptr, nullptr);
          break;
        // For fixed scale, we create an image shader with an image backed by
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
