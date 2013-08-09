// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/values.h"
#include "cc/base/math_util.h"
#include "cc/output/filter_operation.h"
#include "third_party/skia/include/core/SkMath.h"

namespace cc {

bool FilterOperation::operator==(const FilterOperation& other) const {
  if (type_ != other.type_)
    return false;
  if (type_ == COLOR_MATRIX)
    return !memcmp(matrix_, other.matrix_, sizeof(matrix_));
  if (type_ == DROP_SHADOW) {
    return amount_ == other.amount_ &&
           drop_shadow_offset_ == other.drop_shadow_offset_ &&
           drop_shadow_color_ == other.drop_shadow_color_;
  }
  return amount_ == other.amount_;
}

FilterOperation::FilterOperation(FilterType type, float amount)
    : type_(type),
      amount_(amount),
      drop_shadow_offset_(0, 0),
      drop_shadow_color_(0),
      zoom_inset_(0) {
  DCHECK_NE(type_, DROP_SHADOW);
  DCHECK_NE(type_, COLOR_MATRIX);
  memset(matrix_, 0, sizeof(matrix_));
}

FilterOperation::FilterOperation(FilterType type,
                                 gfx::Point offset,
                                 float stdDeviation,
                                 SkColor color)
    : type_(type),
      amount_(stdDeviation),
      drop_shadow_offset_(offset),
      drop_shadow_color_(color),
      zoom_inset_(0) {
  DCHECK_EQ(type_, DROP_SHADOW);
  memset(matrix_, 0, sizeof(matrix_));
}

FilterOperation::FilterOperation(FilterType type, SkScalar matrix[20])
    : type_(type),
      amount_(0),
      drop_shadow_offset_(0, 0),
      drop_shadow_color_(0),
      zoom_inset_(0) {
  DCHECK_EQ(type_, COLOR_MATRIX);
  memcpy(matrix_, matrix, sizeof(matrix_));
}

FilterOperation::FilterOperation(FilterType type, float amount, int inset)
    : type_(type),
      amount_(amount),
      drop_shadow_offset_(0, 0),
      drop_shadow_color_(0),
      zoom_inset_(inset) {
  DCHECK_EQ(type_, ZOOM);
  memset(matrix_, 0, sizeof(matrix_));
}

// TODO(ajuma): Define a version of ui::Tween::ValueBetween for floats, and use
// that instead.
static float BlendFloats(float from, float to, double progress) {
  return from * (1.0 - progress) + to * progress;
}

static int BlendInts(int from, int to, double progress) {
  return static_cast<int>(
      MathUtil::Round(from * (1.0 - progress) + to * progress));
}

static uint8_t  BlendColorComponents(uint8_t from,
                                     uint8_t to,
                                     uint8_t from_alpha,
                                     uint8_t to_alpha,
                                     uint8_t blended_alpha,
                                     double progress) {
  // Since progress can be outside [0, 1], blending can produce a value outside
  // [0, 255].
  int blended_premultiplied = BlendInts(SkMulDiv255Round(from, from_alpha),
                                        SkMulDiv255Round(to, to_alpha),
                                        progress);
  int blended = static_cast<int>(
      MathUtil::Round(blended_premultiplied * 255.f / blended_alpha));
  return static_cast<uint8_t>(MathUtil::ClampToRange(blended, 0, 255));
}

static SkColor BlendSkColors(SkColor from, SkColor to, double progress) {
  int from_a = SkColorGetA(from);
  int to_a = SkColorGetA(to);
  int blended_a = BlendInts(from_a, to_a, progress);
  if (blended_a <= 0)
    return SkColorSetARGB(0, 0, 0, 0);
  blended_a = std::min(blended_a, 255);

  // TODO(ajuma): Use SkFourByteInterp once http://crbug.com/260369 is fixed.
  uint8_t blended_r = BlendColorComponents(
      SkColorGetR(from), SkColorGetR(to), from_a, to_a, blended_a, progress);
  uint8_t blended_g = BlendColorComponents(
      SkColorGetG(from), SkColorGetG(to), from_a, to_a, blended_a, progress);
  uint8_t blended_b = BlendColorComponents(
      SkColorGetB(from), SkColorGetB(to), from_a, to_a, blended_a, progress);

  return SkColorSetARGB(blended_a, blended_r, blended_g, blended_b);
}

static FilterOperation CreateNoOpFilter(FilterOperation::FilterType type) {
  switch (type) {
    case FilterOperation::GRAYSCALE:
      return FilterOperation::CreateGrayscaleFilter(0.f);
    case FilterOperation::SEPIA:
      return FilterOperation::CreateSepiaFilter(0.f);
    case FilterOperation::SATURATE:
      return FilterOperation::CreateSaturateFilter(1.f);
    case FilterOperation::HUE_ROTATE:
      return FilterOperation::CreateHueRotateFilter(0.f);
    case FilterOperation::INVERT:
      return FilterOperation::CreateInvertFilter(0.f);
    case FilterOperation::BRIGHTNESS:
      return FilterOperation::CreateBrightnessFilter(1.f);
    case FilterOperation::CONTRAST:
      return FilterOperation::CreateContrastFilter(1.f);
    case FilterOperation::OPACITY:
      return FilterOperation::CreateOpacityFilter(1.f);
    case FilterOperation::BLUR:
      return FilterOperation::CreateBlurFilter(0.f);
    case FilterOperation::DROP_SHADOW:
      return FilterOperation::CreateDropShadowFilter(
          gfx::Point(0, 0), 0.f, SK_ColorTRANSPARENT);
    case FilterOperation::COLOR_MATRIX: {
      SkScalar matrix[20];
      memset(matrix, 0, 20 * sizeof(SkScalar));
      matrix[0] = matrix[6] = matrix[12] = matrix[18] = 1.f;
      return FilterOperation::CreateColorMatrixFilter(matrix);
    }
    case FilterOperation::ZOOM:
      return FilterOperation::CreateZoomFilter(1.f, 0);
    case FilterOperation::SATURATING_BRIGHTNESS:
      return FilterOperation::CreateSaturatingBrightnessFilter(0.f);
    default:
      NOTREACHED();
      return FilterOperation::CreateEmptyFilter();
  }
}

static float ClampAmountForFilterType(float amount,
                                      FilterOperation::FilterType type) {
  switch (type) {
    case FilterOperation::GRAYSCALE:
    case FilterOperation::SEPIA:
    case FilterOperation::INVERT:
    case FilterOperation::OPACITY:
      return MathUtil::ClampToRange(amount, 0.f, 1.f);
    case FilterOperation::SATURATE:
    case FilterOperation::BRIGHTNESS:
    case FilterOperation::CONTRAST:
    case FilterOperation::BLUR:
    case FilterOperation::DROP_SHADOW:
      return std::max(amount, 0.f);
    case FilterOperation::ZOOM:
      return std::max(amount, 1.f);
    case FilterOperation::HUE_ROTATE:
    case FilterOperation::SATURATING_BRIGHTNESS:
      return amount;
    case FilterOperation::COLOR_MATRIX:
    default:
      NOTREACHED();
      return amount;
  }
}

// static
FilterOperation FilterOperation::Blend(const FilterOperation* from,
                                       const FilterOperation* to,
                                       double progress) {
  FilterOperation blended_filter = FilterOperation::CreateEmptyFilter();

  if (!from && !to)
    return blended_filter;

  const FilterOperation& from_op = from ? *from : CreateNoOpFilter(to->type());
  const FilterOperation& to_op = to ? *to : CreateNoOpFilter(from->type());

  if (from_op.type() != to_op.type())
    return blended_filter;

  DCHECK(to_op.type() != FilterOperation::COLOR_MATRIX);
  blended_filter.set_type(to_op.type());

  blended_filter.set_amount(ClampAmountForFilterType(
      BlendFloats(from_op.amount(), to_op.amount(), progress), to_op.type()));

  if (to_op.type() == FilterOperation::DROP_SHADOW) {
    gfx::Point blended_offset(BlendInts(from_op.drop_shadow_offset().x(),
                                        to_op.drop_shadow_offset().x(),
                                        progress),
                              BlendInts(from_op.drop_shadow_offset().y(),
                                        to_op.drop_shadow_offset().y(),
                                        progress));
    blended_filter.set_drop_shadow_offset(blended_offset);
    blended_filter.set_drop_shadow_color(BlendSkColors(
        from_op.drop_shadow_color(), to_op.drop_shadow_color(), progress));
  } else if (to_op.type() == FilterOperation::ZOOM) {
    blended_filter.set_zoom_inset(std::max(
        BlendInts(from_op.zoom_inset(), to_op.zoom_inset(), progress), 0));
  }

  return blended_filter;
}

scoped_ptr<base::Value> FilterOperation::AsValue() const {
  scoped_ptr<base::DictionaryValue> value(new DictionaryValue);
  value->SetInteger("type", type_);
  switch (type_) {
    case FilterOperation::GRAYSCALE:
    case FilterOperation::SEPIA:
    case FilterOperation::SATURATE:
    case FilterOperation::HUE_ROTATE:
    case FilterOperation::INVERT:
    case FilterOperation::BRIGHTNESS:
    case FilterOperation::CONTRAST:
    case FilterOperation::OPACITY:
    case FilterOperation::BLUR:
    case FilterOperation::SATURATING_BRIGHTNESS:
      value->SetDouble("amount", amount_);
      break;
    case FilterOperation::DROP_SHADOW:
      value->SetDouble("std_deviation", amount_);
      value->Set("offset", MathUtil::AsValue(drop_shadow_offset_).release());
      value->SetInteger("color", drop_shadow_color_);
      break;
    case FilterOperation::COLOR_MATRIX: {
      scoped_ptr<ListValue> matrix(new ListValue);
      for (size_t i = 0; i < arraysize(matrix_); ++i)
        matrix->AppendDouble(matrix_[i]);
      value->Set("matrix", matrix.release());
      break;
    }
    case FilterOperation::ZOOM:
      value->SetDouble("amount", amount_);
      value->SetDouble("inset", zoom_inset_);
      break;
  }
  return value.PassAs<base::Value>();
}

}  // namespace cc
