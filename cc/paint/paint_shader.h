// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_PAINT_SHADER_H_
#define CC_PAINT_PAINT_SHADER_H_

#include <memory>

#include "cc/paint/paint_export.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkScalar.h"
#include "third_party/skia/include/core/SkShader.h"

namespace cc {

class PaintOpBuffer;
using PaintRecord = PaintOpBuffer;

class CC_PAINT_EXPORT PaintShader {
 public:
  static std::unique_ptr<PaintShader> MakeColor(SkColor color);

  static std::unique_ptr<PaintShader> MakeLinearGradient(
      const SkPoint points[],
      const SkColor colors[],
      const SkScalar pos[],
      int count,
      SkShader::TileMode mode,
      uint32_t flags = 0,
      const SkMatrix* local_matrix = nullptr,
      SkColor fallback_color = SK_ColorTRANSPARENT);

  static std::unique_ptr<PaintShader> MakeRadialGradient(
      const SkPoint& center,
      SkScalar radius,
      const SkColor colors[],
      const SkScalar pos[],
      int color_count,
      SkShader::TileMode mode,
      uint32_t flags = 0,
      const SkMatrix* local_matrix = nullptr,
      SkColor fallback_color = SK_ColorTRANSPARENT);

  static std::unique_ptr<PaintShader> MakeTwoPointConicalGradient(
      const SkPoint& start,
      SkScalar start_radius,
      const SkPoint& end,
      SkScalar end_radius,
      const SkColor colors[],
      const SkScalar pos[],
      int color_count,
      SkShader::TileMode mode,
      uint32_t flags = 0,
      const SkMatrix* local_matrix = nullptr,
      SkColor fallback_color = SK_ColorTRANSPARENT);

  static std::unique_ptr<PaintShader> MakeSweepGradient(
      SkScalar cx,
      SkScalar cy,
      const SkColor colors[],
      const SkScalar pos[],
      int color_count,
      uint32_t flags = 0,
      const SkMatrix* local_matrix = nullptr,
      SkColor fallback_color = SK_ColorTRANSPARENT);

  static std::unique_ptr<PaintShader> MakeImage(sk_sp<const SkImage> image,
                                                SkShader::TileMode tx,
                                                SkShader::TileMode ty,
                                                const SkMatrix* local_matrix);

  static std::unique_ptr<PaintShader> MakePaintRecord(
      sk_sp<PaintRecord> record,
      const SkRect& tile,
      SkShader::TileMode tx,
      SkShader::TileMode ty,
      const SkMatrix* local_matrix);

  PaintShader(const PaintShader& other);
  PaintShader(PaintShader&& other);
  ~PaintShader();

  PaintShader& operator=(const PaintShader& other);
  PaintShader& operator=(PaintShader&& other);

  const sk_sp<SkShader>& sk_shader() const { return sk_shader_; }

 private:
  PaintShader(sk_sp<SkShader> shader, SkColor fallback_color);

  sk_sp<SkShader> sk_shader_;
};

}  // namespace cc

#endif  // CC_PAINT_PAINT_SHADER_H_
