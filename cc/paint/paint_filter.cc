// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/paint_filter.h"

#include "cc/paint/filter_operations.h"
#include "cc/paint/paint_record.h"
#include "third_party/skia/include/core/SkColorFilter.h"
#include "third_party/skia/include/core/SkMath.h"
#include "third_party/skia/include/effects/SkAlphaThresholdFilter.h"
#include "third_party/skia/include/effects/SkColorFilterImageFilter.h"
#include "third_party/skia/include/effects/SkComposeImageFilter.h"
#include "third_party/skia/include/effects/SkImageSource.h"
#include "third_party/skia/include/effects/SkLightingImageFilter.h"
#include "third_party/skia/include/effects/SkMagnifierImageFilter.h"
#include "third_party/skia/include/effects/SkMergeImageFilter.h"
#include "third_party/skia/include/effects/SkMorphologyImageFilter.h"
#include "third_party/skia/include/effects/SkOffsetImageFilter.h"
#include "third_party/skia/include/effects/SkPaintImageFilter.h"
#include "third_party/skia/include/effects/SkPerlinNoiseShader.h"
#include "third_party/skia/include/effects/SkPictureImageFilter.h"
#include "third_party/skia/include/effects/SkTileImageFilter.h"
#include "third_party/skia/include/effects/SkXfermodeImageFilter.h"

namespace cc {

PaintFilter::PaintFilter(Type type, const CropRect* crop_rect) : type_(type) {
  if (crop_rect)
    crop_rect_.emplace(*crop_rect);
}

PaintFilter::~PaintFilter() = default;

// static
std::string PaintFilter::TypeToString(Type type) {
  switch (type) {
    case Type::kColorFilter:
      return "kColorFilter";
    case Type::kBlur:
      return "kBlur";
    case Type::kDropShadow:
      return "kDropShadow";
    case Type::kMagnifier:
      return "kMagnifier";
    case Type::kCompose:
      return "kCompose";
    case Type::kAlphaThreshold:
      return "kAlphaThreshold";
    case Type::kSkImageFilter:
      return "kSkImageFilter";
    case Type::kXfermode:
      return "kXfermode";
    case Type::kArithmetic:
      return "kArithmetic";
    case Type::kMatrixConvolution:
      return "kMatrixConvolution";
    case Type::kDisplacementMapEffect:
      return "kDisplacementMapEffect";
    case Type::kImage:
      return "kImage";
    case Type::kPaintRecord:
      return "kPaintRecord";
    case Type::kMerge:
      return "kMerge";
    case Type::kMorphology:
      return "kMorphology";
    case Type::kOffset:
      return "kOffset";
    case Type::kTile:
      return "kTile";
    case Type::kTurbulence:
      return "kTurbulence";
    case Type::kPaintFlags:
      return "kPaintFlags";
    case Type::kMatrix:
      return "kMatrix";
    case Type::kLightingDistant:
      return "kLightingDistant";
    case Type::kLightingPoint:
      return "kLightingPoint";
    case Type::kLightingSpot:
      return "kLightingSpot";
  }
  NOTREACHED();
  return "Unknown";
}

ColorFilterPaintFilter::ColorFilterPaintFilter(
    sk_sp<SkColorFilter> color_filter,
    sk_sp<PaintFilter> input,
    const CropRect* crop_rect)
    : PaintFilter(kType, crop_rect),
      color_filter_(std::move(color_filter)),
      input_(std::move(input)) {
  DCHECK(color_filter_);
  cached_sk_filter_ = SkColorFilterImageFilter::Make(
      color_filter_, GetSkFilter(input_.get()), crop_rect);
}

ColorFilterPaintFilter::~ColorFilterPaintFilter() = default;

BlurPaintFilter::BlurPaintFilter(SkScalar sigma_x,
                                 SkScalar sigma_y,
                                 TileMode tile_mode,
                                 sk_sp<PaintFilter> input,
                                 const CropRect* crop_rect)
    : PaintFilter(kType, crop_rect),
      sigma_x_(sigma_x),
      sigma_y_(sigma_y),
      tile_mode_(tile_mode),
      input_(std::move(input)) {
  cached_sk_filter_ = SkBlurImageFilter::Make(
      sigma_x_, sigma_y_, GetSkFilter(input_.get()), crop_rect, tile_mode_);
}

BlurPaintFilter::~BlurPaintFilter() = default;

DropShadowPaintFilter::DropShadowPaintFilter(SkScalar dx,
                                             SkScalar dy,
                                             SkScalar sigma_x,
                                             SkScalar sigma_y,
                                             SkColor color,
                                             ShadowMode shadow_mode,
                                             sk_sp<PaintFilter> input,
                                             const CropRect* crop_rect)
    : PaintFilter(kType, crop_rect),
      dx_(dx),
      dy_(dy),
      sigma_x_(sigma_x),
      sigma_y_(sigma_y),
      color_(color),
      shadow_mode_(shadow_mode),
      input_(std::move(input)) {
  cached_sk_filter_ = SkDropShadowImageFilter::Make(
      dx_, dy_, sigma_x_, sigma_y_, color_, shadow_mode_,
      GetSkFilter(input_.get()), crop_rect);
}

DropShadowPaintFilter::~DropShadowPaintFilter() = default;

MagnifierPaintFilter::MagnifierPaintFilter(const SkRect& src_rect,
                                           SkScalar inset,
                                           sk_sp<PaintFilter> input,
                                           const CropRect* crop_rect)
    : PaintFilter(kType, crop_rect),
      src_rect_(src_rect),
      inset_(inset),
      input_(std::move(input)) {
  cached_sk_filter_ = SkMagnifierImageFilter::Make(
      src_rect_, inset_, GetSkFilter(input_.get()), crop_rect);
}

MagnifierPaintFilter::~MagnifierPaintFilter() = default;

ComposePaintFilter::ComposePaintFilter(sk_sp<PaintFilter> outer,
                                       sk_sp<PaintFilter> inner)
    : PaintFilter(Type::kCompose, nullptr),
      outer_(std::move(outer)),
      inner_(std::move(inner)) {
  cached_sk_filter_ = SkComposeImageFilter::Make(GetSkFilter(outer_.get()),
                                                 GetSkFilter(inner_.get()));
}

ComposePaintFilter::~ComposePaintFilter() = default;

AlphaThresholdPaintFilter::AlphaThresholdPaintFilter(const SkRegion& region,
                                                     SkScalar inner_min,
                                                     SkScalar outer_max,
                                                     sk_sp<PaintFilter> input,
                                                     const CropRect* crop_rect)
    : PaintFilter(kType, crop_rect),
      region_(region),
      inner_min_(inner_min),
      outer_max_(outer_max),
      input_(std::move(input)) {
  cached_sk_filter_ = SkAlphaThresholdFilter::Make(
      region_, inner_min_, outer_max_, GetSkFilter(input_.get()), crop_rect);
}

AlphaThresholdPaintFilter::~AlphaThresholdPaintFilter() = default;

ImageFilterPaintFilter::ImageFilterPaintFilter(sk_sp<SkImageFilter> sk_filter)
    : PaintFilter(kType, nullptr), sk_filter_(std::move(sk_filter)) {
  cached_sk_filter_ = sk_filter_;
}

ImageFilterPaintFilter::~ImageFilterPaintFilter() = default;

XfermodePaintFilter::XfermodePaintFilter(SkBlendMode blend_mode,
                                         sk_sp<PaintFilter> background,
                                         sk_sp<PaintFilter> foreground,
                                         const CropRect* crop_rect)
    : PaintFilter(kType, crop_rect),
      blend_mode_(blend_mode),
      background_(std::move(background)),
      foreground_(std::move(foreground)) {
  cached_sk_filter_ =
      SkXfermodeImageFilter::Make(blend_mode_, GetSkFilter(background_.get()),
                                  GetSkFilter(foreground_.get()), crop_rect);
}

XfermodePaintFilter::~XfermodePaintFilter() = default;

ArithmeticPaintFilter::ArithmeticPaintFilter(float k1,
                                             float k2,
                                             float k3,
                                             float k4,
                                             bool enforce_pm_color,
                                             sk_sp<PaintFilter> background,
                                             sk_sp<PaintFilter> foreground,
                                             const CropRect* crop_rect)
    : PaintFilter(kType, crop_rect),
      floats_{k1, k2, k3, k4},
      enforce_pm_color_(enforce_pm_color),
      background_(std::move(background)),
      foreground_(std::move(foreground)) {
  cached_sk_filter_ = SkArithmeticImageFilter::Make(
      floats_[0], floats_[1], floats_[2], floats_[3], enforce_pm_color_,
      GetSkFilter(background_.get()), GetSkFilter(foreground_.get()),
      crop_rect);
}

ArithmeticPaintFilter::~ArithmeticPaintFilter() = default;

MatrixConvolutionPaintFilter::MatrixConvolutionPaintFilter(
    const SkISize& kernel_size,
    const SkScalar* kernel,
    SkScalar gain,
    SkScalar bias,
    const SkIPoint& kernel_offset,
    TileMode tile_mode,
    bool convolve_alpha,
    sk_sp<PaintFilter> input,
    const CropRect* crop_rect)
    : PaintFilter(kType, crop_rect),
      kernel_size_(kernel_size),
      gain_(gain),
      bias_(bias),
      kernel_offset_(kernel_offset),
      tile_mode_(tile_mode),
      convolve_alpha_(convolve_alpha),
      input_(std::move(input)) {
  auto len = sk_64_mul(kernel_size_.width(), kernel_size_.height());
  for (int i = 0; i < len; ++i)
    kernel_->push_back(kernel[i]);

  cached_sk_filter_ = SkMatrixConvolutionImageFilter::Make(
      kernel_size_, kernel, gain_, bias_, kernel_offset_, tile_mode_,
      convolve_alpha_, GetSkFilter(input_.get()), crop_rect);
}

MatrixConvolutionPaintFilter::~MatrixConvolutionPaintFilter() = default;

DisplacementMapEffectPaintFilter::DisplacementMapEffectPaintFilter(
    ChannelSelectorType channel_x,
    ChannelSelectorType channel_y,
    SkScalar scale,
    sk_sp<PaintFilter> displacement,
    sk_sp<PaintFilter> color,
    const CropRect* crop_rect)
    : PaintFilter(kType, crop_rect),
      channel_x_(channel_x),
      channel_y_(channel_y),
      scale_(scale),
      displacement_(std::move(displacement)),
      color_(std::move(color)) {
  cached_sk_filter_ = SkDisplacementMapEffect::Make(
      channel_x_, channel_y_, scale_, GetSkFilter(displacement_.get()),
      GetSkFilter(color_.get()), crop_rect);
}

DisplacementMapEffectPaintFilter::~DisplacementMapEffectPaintFilter() = default;

ImagePaintFilter::ImagePaintFilter(PaintImage image,
                                   const SkRect& src_rect,
                                   const SkRect& dst_rect,
                                   SkFilterQuality filter_quality)
    : PaintFilter(kType, nullptr),
      image_(std::move(image)),
      src_rect_(src_rect),
      dst_rect_(dst_rect),
      filter_quality_(filter_quality) {
  cached_sk_filter_ = SkImageSource::Make(image_.GetSkImage(), src_rect_,
                                          dst_rect_, filter_quality_);
}

ImagePaintFilter::~ImagePaintFilter() = default;

RecordPaintFilter::RecordPaintFilter(sk_sp<PaintRecord> record,
                                     const SkRect& record_bounds)
    : PaintFilter(kType, nullptr),
      record_(std::move(record)),
      record_bounds_(record_bounds) {
  cached_sk_filter_ =
      SkPictureImageFilter::Make(ToSkPicture(record_, record_bounds_));
}

RecordPaintFilter::~RecordPaintFilter() = default;

MergePaintFilter::MergePaintFilter(sk_sp<PaintFilter>* const filters,
                                   int count,
                                   const CropRect* crop_rect)
    : PaintFilter(kType, crop_rect) {
  std::vector<sk_sp<SkImageFilter>> sk_filters;
  sk_filters.reserve(count);

  for (int i = 0; i < count; ++i) {
    inputs_->push_back(filters[i]);
    sk_filters.push_back(GetSkFilter(filters[i].get()));
  }

  cached_sk_filter_ = SkMergeImageFilter::Make(
      static_cast<sk_sp<SkImageFilter>*>(sk_filters.data()), count, crop_rect);
}

MergePaintFilter::~MergePaintFilter() = default;

MorphologyPaintFilter::MorphologyPaintFilter(MorphType morph_type,
                                             int radius_x,
                                             int radius_y,
                                             sk_sp<PaintFilter> input,
                                             const CropRect* crop_rect)
    : PaintFilter(kType, crop_rect),
      morph_type_(morph_type),
      radius_x_(radius_x),
      radius_y_(radius_y),
      input_(std::move(input)) {
  switch (morph_type_) {
    case MorphType::kDilate:
      cached_sk_filter_ = SkDilateImageFilter::Make(
          radius_x_, radius_y_, GetSkFilter(input_.get()), crop_rect);
      break;
    case MorphType::kErode:
      cached_sk_filter_ = SkErodeImageFilter::Make(
          radius_x_, radius_y_, GetSkFilter(input_.get()), crop_rect);
      break;
  }
}

MorphologyPaintFilter::~MorphologyPaintFilter() = default;

OffsetPaintFilter::OffsetPaintFilter(SkScalar dx,
                                     SkScalar dy,
                                     sk_sp<PaintFilter> input,
                                     const CropRect* crop_rect)
    : PaintFilter(kType, crop_rect),
      dx_(dx),
      dy_(dy),
      input_(std::move(input)) {
  cached_sk_filter_ =
      SkOffsetImageFilter::Make(dx_, dy_, GetSkFilter(input_.get()), crop_rect);
}

OffsetPaintFilter::~OffsetPaintFilter() = default;

TilePaintFilter::TilePaintFilter(const SkRect& src,
                                 const SkRect& dst,
                                 sk_sp<PaintFilter> input)
    : PaintFilter(kType, nullptr),
      src_(src),
      dst_(dst),
      input_(std::move(input)) {
  cached_sk_filter_ =
      SkTileImageFilter::Make(src_, dst_, GetSkFilter(input_.get()));
}

TilePaintFilter::~TilePaintFilter() = default;

TurbulencePaintFilter::TurbulencePaintFilter(TurbulenceType turbulence_type,
                                             SkScalar base_frequency_x,
                                             SkScalar base_frequency_y,
                                             int num_octaves,
                                             SkScalar seed,
                                             const SkISize* tile_size,
                                             const CropRect* crop_rect)
    : PaintFilter(kType, crop_rect),
      turbulence_type_(turbulence_type),
      base_frequency_x_(base_frequency_x),
      base_frequency_y_(base_frequency_y),
      num_octaves_(num_octaves),
      seed_(seed),
      tile_size_(tile_size ? *tile_size : SkISize::MakeEmpty()) {
  sk_sp<SkShader> shader;
  switch (turbulence_type_) {
    case TurbulenceType::kTurbulence:
      shader = SkPerlinNoiseShader::MakeTurbulence(
          base_frequency_x_, base_frequency_y_, num_octaves_, seed_,
          &tile_size_);
      break;
    case TurbulenceType::kFractalNoise:
      shader = SkPerlinNoiseShader::MakeFractalNoise(
          base_frequency_x_, base_frequency_y_, num_octaves_, seed_,
          &tile_size_);
      break;
  }

  SkPaint paint;
  paint.setShader(std::move(shader));
  cached_sk_filter_ = SkPaintImageFilter::Make(paint, crop_rect);
}

TurbulencePaintFilter::~TurbulencePaintFilter() = default;

PaintFlagsPaintFilter::PaintFlagsPaintFilter(PaintFlags flags,
                                             const CropRect* crop_rect)
    : PaintFilter(kType, crop_rect), flags_(std::move(flags)) {
  cached_sk_filter_ = SkPaintImageFilter::Make(flags_.ToSkPaint(), crop_rect);
}

PaintFlagsPaintFilter::~PaintFlagsPaintFilter() = default;

MatrixPaintFilter::MatrixPaintFilter(const SkMatrix& matrix,
                                     SkFilterQuality filter_quality,
                                     sk_sp<PaintFilter> input)
    : PaintFilter(Type::kMatrix, nullptr),
      matrix_(matrix),
      filter_quality_(filter_quality),
      input_(std::move(input)) {
  cached_sk_filter_ = SkImageFilter::MakeMatrixFilter(
      matrix_, filter_quality_, GetSkFilter(input_.get()));
}

MatrixPaintFilter::~MatrixPaintFilter() = default;

LightingDistantPaintFilter::LightingDistantPaintFilter(
    LightingType lighting_type,
    const SkPoint3& direction,
    SkColor light_color,
    SkScalar surface_scale,
    SkScalar kconstant,
    SkScalar shininess,
    sk_sp<PaintFilter> input,
    const CropRect* crop_rect)
    : PaintFilter(kType, crop_rect),
      lighting_type_(lighting_type),
      direction_(direction),
      light_color_(light_color),
      surface_scale_(surface_scale),
      kconstant_(kconstant),
      shininess_(shininess),
      input_(std::move(input)) {
  switch (lighting_type_) {
    case LightingType::kDiffuse:
      cached_sk_filter_ = SkLightingImageFilter::MakeDistantLitDiffuse(
          direction_, light_color_, surface_scale_, kconstant_,
          GetSkFilter(input_.get()), crop_rect);
      break;
    case LightingType::kSpecular:
      cached_sk_filter_ = SkLightingImageFilter::MakeDistantLitSpecular(
          direction_, light_color_, surface_scale_, kconstant_, shininess_,
          GetSkFilter(input_.get()), crop_rect);
      break;
  }
}

LightingDistantPaintFilter::~LightingDistantPaintFilter() = default;

LightingPointPaintFilter::LightingPointPaintFilter(LightingType lighting_type,
                                                   const SkPoint3& location,
                                                   SkColor light_color,
                                                   SkScalar surface_scale,
                                                   SkScalar kconstant,
                                                   SkScalar shininess,
                                                   sk_sp<PaintFilter> input,
                                                   const CropRect* crop_rect)
    : PaintFilter(kType, crop_rect),
      lighting_type_(lighting_type),
      location_(location),
      light_color_(light_color),
      surface_scale_(surface_scale),
      kconstant_(kconstant),
      shininess_(shininess),
      input_(std::move(input)) {
  switch (lighting_type_) {
    case LightingType::kDiffuse:
      cached_sk_filter_ = SkLightingImageFilter::MakePointLitDiffuse(
          location_, light_color_, surface_scale_, kconstant_,
          GetSkFilter(input_.get()), crop_rect);
      break;
    case LightingType::kSpecular:
      cached_sk_filter_ = SkLightingImageFilter::MakePointLitSpecular(
          location_, light_color_, surface_scale_, kconstant_, shininess_,
          GetSkFilter(input_.get()), crop_rect);
      break;
  }
}

LightingPointPaintFilter::~LightingPointPaintFilter() = default;

LightingSpotPaintFilter::LightingSpotPaintFilter(LightingType lighting_type,
                                                 const SkPoint3& location,
                                                 const SkPoint3& target,
                                                 SkScalar specular_exponent,
                                                 SkScalar cutoff_angle,
                                                 SkColor light_color,
                                                 SkScalar surface_scale,
                                                 SkScalar kconstant,
                                                 SkScalar shininess,
                                                 sk_sp<PaintFilter> input,
                                                 const CropRect* crop_rect)
    : PaintFilter(kType, crop_rect),
      lighting_type_(lighting_type),
      location_(location),
      target_(target),
      specular_exponent_(specular_exponent),
      cutoff_angle_(cutoff_angle),
      light_color_(light_color),
      surface_scale_(surface_scale),
      kconstant_(kconstant),
      shininess_(shininess),
      input_(std::move(input)) {
  switch (lighting_type_) {
    case LightingType::kDiffuse:
      cached_sk_filter_ = SkLightingImageFilter::MakeSpotLitDiffuse(
          location_, target_, specular_exponent_, cutoff_angle_, light_color_,
          surface_scale_, kconstant_, GetSkFilter(input_.get()), crop_rect);
      break;
    case LightingType::kSpecular:
      cached_sk_filter_ = SkLightingImageFilter::MakeSpotLitSpecular(
          location_, target_, specular_exponent_, cutoff_angle_, light_color_,
          surface_scale_, kconstant_, shininess_, GetSkFilter(input_.get()),
          crop_rect);
      break;
  }
}

LightingSpotPaintFilter::~LightingSpotPaintFilter() = default;

}  // namespace cc
