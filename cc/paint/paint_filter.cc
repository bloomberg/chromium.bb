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
namespace {
bool AreFiltersEqual(const PaintFilter* one, const PaintFilter* two) {
  if (!one || !two)
    return !one && !two;
  return *one == *two;
}

bool AreScalarsEqual(SkScalar one, SkScalar two) {
  return PaintOp::AreEqualEvenIfNaN(one, two);
}
}  // namespace

PaintFilter::PaintFilter(Type type, const CropRect* crop_rect) : type_(type) {
  if (crop_rect)
    crop_rect_.emplace(*crop_rect);
}

PaintFilter::~PaintFilter() = default;

// static
std::string PaintFilter::TypeToString(Type type) {
  switch (type) {
    case Type::kNullFilter:
      return "kNullFilter";
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

bool PaintFilter::operator==(const PaintFilter& other) const {
  if (type_ != other.type_)
    return false;
  if (!!crop_rect_ != !!other.crop_rect_)
    return false;
  if (crop_rect_) {
    if (crop_rect_->flags() != other.crop_rect_->flags() ||
        !PaintOp::AreSkRectsEqual(crop_rect_->rect(),
                                  other.crop_rect_->rect())) {
      return false;
    }
  }

  switch (type_) {
    case Type::kNullFilter:
      return true;
    case Type::kColorFilter:
      return *static_cast<const ColorFilterPaintFilter*>(this) ==
             static_cast<const ColorFilterPaintFilter&>(other);
    case Type::kBlur:
      return *static_cast<const BlurPaintFilter*>(this) ==
             static_cast<const BlurPaintFilter&>(other);
    case Type::kDropShadow:
      return *static_cast<const DropShadowPaintFilter*>(this) ==
             static_cast<const DropShadowPaintFilter&>(other);
    case Type::kMagnifier:
      return *static_cast<const MagnifierPaintFilter*>(this) ==
             static_cast<const MagnifierPaintFilter&>(other);
    case Type::kCompose:
      return *static_cast<const ComposePaintFilter*>(this) ==
             static_cast<const ComposePaintFilter&>(other);
    case Type::kAlphaThreshold:
      return *static_cast<const AlphaThresholdPaintFilter*>(this) ==
             static_cast<const AlphaThresholdPaintFilter&>(other);
    case Type::kXfermode:
      return *static_cast<const XfermodePaintFilter*>(this) ==
             static_cast<const XfermodePaintFilter&>(other);
    case Type::kArithmetic:
      return *static_cast<const ArithmeticPaintFilter*>(this) ==
             static_cast<const ArithmeticPaintFilter&>(other);
    case Type::kMatrixConvolution:
      return *static_cast<const MatrixConvolutionPaintFilter*>(this) ==
             static_cast<const MatrixConvolutionPaintFilter&>(other);
    case Type::kDisplacementMapEffect:
      return *static_cast<const DisplacementMapEffectPaintFilter*>(this) ==
             static_cast<const DisplacementMapEffectPaintFilter&>(other);
    case Type::kImage:
      return *static_cast<const ImagePaintFilter*>(this) ==
             static_cast<const ImagePaintFilter&>(other);
    case Type::kPaintRecord:
      return *static_cast<const RecordPaintFilter*>(this) ==
             static_cast<const RecordPaintFilter&>(other);
    case Type::kMerge:
      return *static_cast<const MergePaintFilter*>(this) ==
             static_cast<const MergePaintFilter&>(other);
    case Type::kMorphology:
      return *static_cast<const MorphologyPaintFilter*>(this) ==
             static_cast<const MorphologyPaintFilter&>(other);
    case Type::kOffset:
      return *static_cast<const OffsetPaintFilter*>(this) ==
             static_cast<const OffsetPaintFilter&>(other);
    case Type::kTile:
      return *static_cast<const TilePaintFilter*>(this) ==
             static_cast<const TilePaintFilter&>(other);
    case Type::kTurbulence:
      return *static_cast<const TurbulencePaintFilter*>(this) ==
             static_cast<const TurbulencePaintFilter&>(other);
    case Type::kPaintFlags:
      return *static_cast<const PaintFlagsPaintFilter*>(this) ==
             static_cast<const PaintFlagsPaintFilter&>(other);
    case Type::kMatrix:
      return *static_cast<const MatrixPaintFilter*>(this) ==
             static_cast<const MatrixPaintFilter&>(other);
    case Type::kLightingDistant:
      return *static_cast<const LightingDistantPaintFilter*>(this) ==
             static_cast<const LightingDistantPaintFilter&>(other);
    case Type::kLightingPoint:
      return *static_cast<const LightingPointPaintFilter*>(this) ==
             static_cast<const LightingPointPaintFilter&>(other);
    case Type::kLightingSpot:
      return *static_cast<const LightingSpotPaintFilter*>(this) ==
             static_cast<const LightingSpotPaintFilter&>(other);
  }
  NOTREACHED();
  return true;
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

bool ColorFilterPaintFilter::operator==(
    const ColorFilterPaintFilter& other) const {
  return PaintOp::AreSkFlattenablesEqual(color_filter_.get(),
                                         other.color_filter_.get()) &&
         AreFiltersEqual(input_.get(), other.input_.get());
}

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

bool BlurPaintFilter::operator==(const BlurPaintFilter& other) const {
  return PaintOp::AreEqualEvenIfNaN(sigma_x_, other.sigma_x_) &&
         PaintOp::AreEqualEvenIfNaN(sigma_y_, other.sigma_y_) &&
         tile_mode_ == other.tile_mode_ &&
         AreFiltersEqual(input_.get(), other.input_.get());
}

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

bool DropShadowPaintFilter::operator==(
    const DropShadowPaintFilter& other) const {
  return PaintOp::AreEqualEvenIfNaN(dx_, other.dx_) &&
         PaintOp::AreEqualEvenIfNaN(dy_, other.dy_) &&
         PaintOp::AreEqualEvenIfNaN(sigma_x_, other.sigma_x_) &&
         PaintOp::AreEqualEvenIfNaN(sigma_y_, other.sigma_y_) &&
         color_ == other.color_ && shadow_mode_ == other.shadow_mode_ &&
         AreFiltersEqual(input_.get(), other.input_.get());
}

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

bool MagnifierPaintFilter::operator==(const MagnifierPaintFilter& other) const {
  return PaintOp::AreSkRectsEqual(src_rect_, other.src_rect_) &&
         PaintOp::AreEqualEvenIfNaN(inset_, other.inset_) &&
         AreFiltersEqual(input_.get(), other.input_.get());
}

ComposePaintFilter::ComposePaintFilter(sk_sp<PaintFilter> outer,
                                       sk_sp<PaintFilter> inner)
    : PaintFilter(Type::kCompose, nullptr),
      outer_(std::move(outer)),
      inner_(std::move(inner)) {
  cached_sk_filter_ = SkComposeImageFilter::Make(GetSkFilter(outer_.get()),
                                                 GetSkFilter(inner_.get()));
}

ComposePaintFilter::~ComposePaintFilter() = default;

bool ComposePaintFilter::operator==(const ComposePaintFilter& other) const {
  return AreFiltersEqual(outer_.get(), other.outer_.get()) &&
         AreFiltersEqual(inner_.get(), other.inner_.get());
}

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

bool AlphaThresholdPaintFilter::operator==(
    const AlphaThresholdPaintFilter& other) const {
  return region_ == other.region_ &&
         PaintOp::AreEqualEvenIfNaN(inner_min_, other.inner_min_) &&
         PaintOp::AreEqualEvenIfNaN(outer_max_, other.outer_max_) &&
         AreFiltersEqual(input_.get(), other.input_.get());
}

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

bool XfermodePaintFilter::operator==(const XfermodePaintFilter& other) const {
  return blend_mode_ == other.blend_mode_ &&
         AreFiltersEqual(background_.get(), other.background_.get()) &&
         AreFiltersEqual(foreground_.get(), other.foreground_.get());
}

ArithmeticPaintFilter::ArithmeticPaintFilter(float k1,
                                             float k2,
                                             float k3,
                                             float k4,
                                             bool enforce_pm_color,
                                             sk_sp<PaintFilter> background,
                                             sk_sp<PaintFilter> foreground,
                                             const CropRect* crop_rect)
    : PaintFilter(kType, crop_rect),
      k1_(k1),
      k2_(k2),
      k3_(k3),
      k4_(k4),
      enforce_pm_color_(enforce_pm_color),
      background_(std::move(background)),
      foreground_(std::move(foreground)) {
  cached_sk_filter_ = SkArithmeticImageFilter::Make(
      k1_, k2_, k3_, k4_, enforce_pm_color_, GetSkFilter(background_.get()),
      GetSkFilter(foreground_.get()), crop_rect);
}

ArithmeticPaintFilter::~ArithmeticPaintFilter() = default;

bool ArithmeticPaintFilter::operator==(
    const ArithmeticPaintFilter& other) const {
  return PaintOp::AreEqualEvenIfNaN(k1_, other.k1_) &&
         PaintOp::AreEqualEvenIfNaN(k2_, other.k2_) &&
         PaintOp::AreEqualEvenIfNaN(k3_, other.k3_) &&
         PaintOp::AreEqualEvenIfNaN(k4_, other.k4_) &&
         enforce_pm_color_ == other.enforce_pm_color_ &&
         AreFiltersEqual(background_.get(), other.background_.get()) &&
         AreFiltersEqual(foreground_.get(), other.foreground_.get());
}

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
  auto len = static_cast<size_t>(
      sk_64_mul(kernel_size_.width(), kernel_size_.height()));
  kernel_->reserve(len);
  for (size_t i = 0; i < len; ++i)
    kernel_->push_back(kernel[i]);

  cached_sk_filter_ = SkMatrixConvolutionImageFilter::Make(
      kernel_size_, kernel, gain_, bias_, kernel_offset_, tile_mode_,
      convolve_alpha_, GetSkFilter(input_.get()), crop_rect);
}

MatrixConvolutionPaintFilter::~MatrixConvolutionPaintFilter() = default;

bool MatrixConvolutionPaintFilter::operator==(
    const MatrixConvolutionPaintFilter& other) const {
  return kernel_size_ == other.kernel_size_ &&
         std::equal(kernel_.container().begin(), kernel_.container().end(),
                    other.kernel_.container().begin(), AreScalarsEqual) &&
         PaintOp::AreEqualEvenIfNaN(gain_, other.gain_) &&
         PaintOp::AreEqualEvenIfNaN(bias_, other.bias_) &&
         kernel_offset_ == other.kernel_offset_ &&
         tile_mode_ == other.tile_mode_ &&
         convolve_alpha_ == other.convolve_alpha_ &&
         AreFiltersEqual(input_.get(), other.input_.get());
}

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

bool DisplacementMapEffectPaintFilter::operator==(
    const DisplacementMapEffectPaintFilter& other) const {
  return channel_x_ == other.channel_x_ && channel_y_ == other.channel_y_ &&
         PaintOp::AreEqualEvenIfNaN(scale_, other.scale_) &&
         AreFiltersEqual(displacement_.get(), other.displacement_.get()) &&
         AreFiltersEqual(color_.get(), other.color_.get());
}

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

bool ImagePaintFilter::operator==(const ImagePaintFilter& other) const {
  return !!image_ == !!other.image_ &&
         PaintOp::AreSkRectsEqual(src_rect_, other.src_rect_) &&
         PaintOp::AreSkRectsEqual(dst_rect_, other.dst_rect_) &&
         filter_quality_ == other.filter_quality_;
}

RecordPaintFilter::RecordPaintFilter(sk_sp<PaintRecord> record,
                                     const SkRect& record_bounds)
    : PaintFilter(kType, nullptr),
      record_(std::move(record)),
      record_bounds_(record_bounds) {
  cached_sk_filter_ =
      SkPictureImageFilter::Make(ToSkPicture(record_, record_bounds_));
}

RecordPaintFilter::~RecordPaintFilter() = default;

bool RecordPaintFilter::operator==(const RecordPaintFilter& other) const {
  return !!record_ == !!other.record_ &&
         PaintOp::AreSkRectsEqual(record_bounds_, other.record_bounds_);
}

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

bool MergePaintFilter::operator==(const MergePaintFilter& other) const {
  if (inputs_->size() != other.inputs_->size())
    return false;
  for (size_t i = 0; i < inputs_->size(); ++i) {
    if (!AreFiltersEqual(inputs_[i].get(), other.inputs_[i].get()))
      return false;
  }
  return true;
}

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

bool MorphologyPaintFilter::operator==(
    const MorphologyPaintFilter& other) const {
  return morph_type_ == other.morph_type_ && radius_x_ == other.radius_x_ &&
         radius_y_ == other.radius_y_ &&
         AreFiltersEqual(input_.get(), other.input_.get());
}

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

bool OffsetPaintFilter::operator==(const OffsetPaintFilter& other) const {
  return PaintOp::AreEqualEvenIfNaN(dx_, other.dx_) &&
         PaintOp::AreEqualEvenIfNaN(dy_, other.dy_) &&
         AreFiltersEqual(input_.get(), other.input_.get());
}

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

bool TilePaintFilter::operator==(const TilePaintFilter& other) const {
  return PaintOp::AreSkRectsEqual(src_, other.src_) &&
         PaintOp::AreSkRectsEqual(dst_, other.dst_) &&
         AreFiltersEqual(input_.get(), other.input_.get());
}

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

bool TurbulencePaintFilter::operator==(
    const TurbulencePaintFilter& other) const {
  return turbulence_type_ == other.turbulence_type_ &&
         PaintOp::AreEqualEvenIfNaN(base_frequency_x_,
                                    other.base_frequency_x_) &&
         PaintOp::AreEqualEvenIfNaN(base_frequency_y_,
                                    other.base_frequency_y_) &&
         num_octaves_ == other.num_octaves_ &&
         PaintOp::AreEqualEvenIfNaN(seed_, other.seed_) &&
         tile_size_ == other.tile_size_;
}

PaintFlagsPaintFilter::PaintFlagsPaintFilter(PaintFlags flags,
                                             const CropRect* crop_rect)
    : PaintFilter(kType, crop_rect), flags_(std::move(flags)) {
  cached_sk_filter_ = SkPaintImageFilter::Make(flags_.ToSkPaint(), crop_rect);
}

PaintFlagsPaintFilter::~PaintFlagsPaintFilter() = default;

bool PaintFlagsPaintFilter::operator==(
    const PaintFlagsPaintFilter& other) const {
  return flags_ == other.flags_;
}

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

bool MatrixPaintFilter::operator==(const MatrixPaintFilter& other) const {
  return PaintOp::AreSkMatricesEqual(matrix_, other.matrix_) &&
         filter_quality_ == other.filter_quality_ &&
         AreFiltersEqual(input_.get(), other.input_.get());
}

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

bool LightingDistantPaintFilter::operator==(
    const LightingDistantPaintFilter& other) const {
  return lighting_type_ == other.lighting_type_ &&
         PaintOp::AreSkPoint3sEqual(direction_, other.direction_) &&
         light_color_ == other.light_color_ &&
         PaintOp::AreEqualEvenIfNaN(surface_scale_, other.surface_scale_) &&
         PaintOp::AreEqualEvenIfNaN(kconstant_, other.kconstant_) &&
         PaintOp::AreEqualEvenIfNaN(shininess_, other.shininess_) &&
         AreFiltersEqual(input_.get(), other.input_.get());
}

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

bool LightingPointPaintFilter::operator==(
    const LightingPointPaintFilter& other) const {
  return lighting_type_ == other.lighting_type_ &&
         PaintOp::AreSkPoint3sEqual(location_, other.location_) &&
         light_color_ == other.light_color_ &&
         PaintOp::AreEqualEvenIfNaN(surface_scale_, other.surface_scale_) &&
         PaintOp::AreEqualEvenIfNaN(kconstant_, other.kconstant_) &&
         PaintOp::AreEqualEvenIfNaN(shininess_, other.shininess_) &&
         AreFiltersEqual(input_.get(), other.input_.get());
}

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

bool LightingSpotPaintFilter::operator==(
    const LightingSpotPaintFilter& other) const {
  return lighting_type_ == other.lighting_type_ &&
         PaintOp::AreSkPoint3sEqual(location_, other.location_) &&
         PaintOp::AreSkPoint3sEqual(target_, other.target_) &&
         PaintOp::AreEqualEvenIfNaN(specular_exponent_,
                                    other.specular_exponent_) &&
         PaintOp::AreEqualEvenIfNaN(cutoff_angle_, other.cutoff_angle_) &&
         light_color_ == other.light_color_ &&
         PaintOp::AreEqualEvenIfNaN(surface_scale_, other.surface_scale_) &&
         PaintOp::AreEqualEvenIfNaN(kconstant_, other.kconstant_) &&
         PaintOp::AreEqualEvenIfNaN(shininess_, other.shininess_) &&
         AreFiltersEqual(input_.get(), other.input_.get());
}

}  // namespace cc
