// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/paint_shader.h"

#include "base/memory/ptr_util.h"
#include "cc/paint/paint_record.h"
#include "third_party/skia/include/effects/SkGradientShader.h"

namespace cc {

std::unique_ptr<PaintShader> PaintShader::MakeColor(SkColor color) {
  return base::WrapUnique(new PaintShader(nullptr, color));
}

std::unique_ptr<PaintShader> PaintShader::MakeLinearGradient(
    const SkPoint points[],
    const SkColor colors[],
    const SkScalar pos[],
    int count,
    SkShader::TileMode mode,
    uint32_t flags,
    const SkMatrix* local_matrix,
    SkColor fallback_color) {
  return base::WrapUnique(
      new PaintShader(SkGradientShader::MakeLinear(points, colors, pos, count,
                                                   mode, flags, local_matrix),
                      fallback_color));
}

std::unique_ptr<PaintShader> PaintShader::MakeRadialGradient(
    const SkPoint& center,
    SkScalar radius,
    const SkColor colors[],
    const SkScalar pos[],
    int color_count,
    SkShader::TileMode mode,
    uint32_t flags,
    const SkMatrix* local_matrix,
    SkColor fallback_color) {
  return base::WrapUnique(new PaintShader(
      SkGradientShader::MakeRadial(center, radius, colors, pos, color_count,
                                   mode, flags, local_matrix),
      fallback_color));
}

std::unique_ptr<PaintShader> PaintShader::MakeTwoPointConicalGradient(
    const SkPoint& start,
    SkScalar start_radius,
    const SkPoint& end,
    SkScalar end_radius,
    const SkColor colors[],
    const SkScalar pos[],
    int color_count,
    SkShader::TileMode mode,
    uint32_t flags,
    const SkMatrix* local_matrix,
    SkColor fallback_color) {
  return base::WrapUnique(
      new PaintShader(SkGradientShader::MakeTwoPointConical(
                          start, start_radius, end, end_radius, colors, pos,
                          color_count, mode, flags, local_matrix),
                      fallback_color));
}

std::unique_ptr<PaintShader> PaintShader::MakeSweepGradient(
    SkScalar cx,
    SkScalar cy,
    const SkColor colors[],
    const SkScalar pos[],
    int color_count,
    uint32_t flags,
    const SkMatrix* local_matrix,
    SkColor fallback_color) {
  return base::WrapUnique(new PaintShader(
      SkGradientShader::MakeSweep(cx, cy, colors, pos, color_count, flags,
                                  local_matrix),
      fallback_color));
}

std::unique_ptr<PaintShader> PaintShader::MakeImage(
    sk_sp<const SkImage> image,
    SkShader::TileMode tx,
    SkShader::TileMode ty,
    const SkMatrix* local_matrix) {
  return base::WrapUnique(new PaintShader(
      image->makeShader(tx, ty, local_matrix), SK_ColorTRANSPARENT));
}

std::unique_ptr<PaintShader> PaintShader::MakePaintRecord(
    sk_sp<PaintRecord> record,
    const SkRect& tile,
    SkShader::TileMode tx,
    SkShader::TileMode ty,
    const SkMatrix* local_matrix) {
  return base::WrapUnique(new PaintShader(
      SkShader::MakePictureShader(ToSkPicture(std::move(record), tile), tx, ty,
                                  local_matrix, nullptr),
      SK_ColorTRANSPARENT));
}

PaintShader::PaintShader(sk_sp<SkShader> shader, SkColor fallback_color)
    : sk_shader_(shader ? std::move(shader)
                        : SkShader::MakeColorShader(fallback_color)) {
  DCHECK(sk_shader_);
}
PaintShader::PaintShader(const PaintShader& other) = default;
PaintShader::PaintShader(PaintShader&& other) = default;
PaintShader::~PaintShader() = default;

PaintShader& PaintShader::operator=(const PaintShader& other) = default;
PaintShader& PaintShader::operator=(PaintShader&& other) = default;

}  // namespace cc
