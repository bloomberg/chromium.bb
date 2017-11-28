// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_PAINT_FILTER_H_
#define CC_PAINT_PAINT_FILTER_H_

#include "base/containers/stack_container.h"
#include "base/logging.h"
#include "base/optional.h"
#include "cc/paint/paint_export.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_image.h"
#include "third_party/skia/include/core/SkBlendMode.h"
#include "third_party/skia/include/core/SkImageFilter.h"
#include "third_party/skia/include/core/SkPoint3.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "third_party/skia/include/effects/SkBlurImageFilter.h"
#include "third_party/skia/include/effects/SkDisplacementMapEffect.h"
#include "third_party/skia/include/effects/SkDropShadowImageFilter.h"
#include "third_party/skia/include/effects/SkMatrixConvolutionImageFilter.h"

namespace mojo {
template <class M, class T>
struct StructTraits;
}  // namespace mojo

namespace IPC {
template <class P>
struct ParamTraits;
}  // namespace IPC

namespace viz {
namespace mojom {
class FilterOperationDataView;
}  // namespace mojom

class GLRenderer;
class SoftwareRenderer;
}  // namespace viz

namespace cc {
class FilterOperation;
class FilterOperations;

class CC_PAINT_EXPORT PaintFilter : public SkRefCnt {
 public:
  enum class Type : uint8_t {
    kColorFilter,
    kBlur,
    kDropShadow,
    kMagnifier,
    kCompose,
    kAlphaThreshold,
    // This is currently required to support serialization for
    // FilterOperation. It will be removed once PaintFilters can be
    // serialized directly. See crbug.com/777636.
    kSkImageFilter,
    kXfermode,
    kArithmetic,
    kMatrixConvolution,
    kDisplacementMapEffect,
    kImage,
    kPaintRecord,
    kMerge,
    kMorphology,
    kOffset,
    kTile,
    kTurbulence,
    kPaintFlags,
    kMatrix,
    kLightingDistant,
    kLightingPoint,
    kLightingSpot,
  };
  enum class LightingType : uint8_t {
    kDiffuse,
    kSpecular,
  };

  using MapDirection = SkImageFilter::MapDirection;
  using CropRect = SkImageFilter::CropRect;

  ~PaintFilter() override;

  static std::string TypeToString(Type type);

  Type type() const { return type_; }
  SkIRect filter_bounds(const SkIRect& src,
                        const SkMatrix& ctm,
                        MapDirection direction) const {
    return cached_sk_filter_->filterBounds(src, ctm, direction);
  }
  int count_inputs() const { return cached_sk_filter_->countInputs(); }
  std::string ToString() const {
    SkString str;
    cached_sk_filter_->toString(&str);
    return str.c_str();
  }
  const CropRect* crop_rect() const {
    return crop_rect_ ? &*crop_rect_ : nullptr;
  }

 protected:
  PaintFilter(Type type, const CropRect* crop_rect);

  static sk_sp<SkImageFilter> GetSkFilter(const PaintFilter* paint_filter) {
    return paint_filter ? paint_filter->cached_sk_filter_ : nullptr;
  }
  const sk_sp<SkImageFilter>& cached_sk_filter() const {
    return cached_sk_filter_;
  }

  // This should be created by each sub-class at construction time, to ensure
  // that subsequent access to the filter is thread-safe.
  sk_sp<SkImageFilter> cached_sk_filter_;

 private:
  // For cached skia filter access in SkPaint conversions. Mostly used during
  // raster.
  friend class PaintFlags;
  friend class viz::GLRenderer;
  friend class viz::SoftwareRenderer;

  // For cross-process transport of FilterOperations. To be removed once
  // PaintFilters can be serialized directly. See crbug.com/777636.
  friend struct IPC::ParamTraits<FilterOperation>;
  friend struct mojo::StructTraits<viz::mojom::FilterOperationDataView,
                                   FilterOperation>;

  const Type type_;
  base::Optional<CropRect> crop_rect_;

  DISALLOW_COPY_AND_ASSIGN(PaintFilter);
};

class CC_PAINT_EXPORT ColorFilterPaintFilter final : public PaintFilter {
 public:
  static constexpr Type kType = Type::kColorFilter;
  ColorFilterPaintFilter(sk_sp<SkColorFilter> color_filter,
                         sk_sp<PaintFilter> input,
                         const CropRect* crop_rect = nullptr);
  ~ColorFilterPaintFilter() override;

  const sk_sp<SkColorFilter>& color_filter() const { return color_filter_; }
  const sk_sp<PaintFilter>& input() const { return input_; }

 private:
  sk_sp<SkColorFilter> color_filter_;
  sk_sp<PaintFilter> input_;
};

class CC_PAINT_EXPORT BlurPaintFilter final : public PaintFilter {
 public:
  using TileMode = SkBlurImageFilter::TileMode;
  static constexpr Type kType = Type::kBlur;
  BlurPaintFilter(SkScalar sigma_x,
                  SkScalar sigma_y,
                  TileMode tile_mode,
                  sk_sp<PaintFilter> input,
                  const CropRect* crop_rect = nullptr);
  ~BlurPaintFilter() override;

  const sk_sp<PaintFilter>& input() const { return input_; }

 private:
  SkScalar sigma_x_;
  SkScalar sigma_y_;
  TileMode tile_mode_;
  sk_sp<PaintFilter> input_;
};

class CC_PAINT_EXPORT DropShadowPaintFilter final : public PaintFilter {
 public:
  using ShadowMode = SkDropShadowImageFilter::ShadowMode;
  static constexpr Type kType = Type::kDropShadow;
  DropShadowPaintFilter(SkScalar dx,
                        SkScalar dy,
                        SkScalar sigma_x,
                        SkScalar sigma_y,
                        SkColor color,
                        ShadowMode shadow_mode,
                        sk_sp<PaintFilter> input,
                        const CropRect* crop_rect = nullptr);
  ~DropShadowPaintFilter() override;

 private:
  SkScalar dx_;
  SkScalar dy_;
  SkScalar sigma_x_;
  SkScalar sigma_y_;
  SkColor color_;
  ShadowMode shadow_mode_;
  sk_sp<PaintFilter> input_;
};

class CC_PAINT_EXPORT MagnifierPaintFilter final : public PaintFilter {
 public:
  static constexpr Type kType = Type::kMagnifier;
  MagnifierPaintFilter(const SkRect& src_rect,
                       SkScalar inset,
                       sk_sp<PaintFilter> input,
                       const CropRect* crop_rect = nullptr);
  ~MagnifierPaintFilter() override;

 private:
  SkRect src_rect_;
  SkScalar inset_;
  sk_sp<PaintFilter> input_;
};

class CC_PAINT_EXPORT ComposePaintFilter final : public PaintFilter {
 public:
  static constexpr Type kType = Type::kCompose;
  ComposePaintFilter(sk_sp<PaintFilter> outer, sk_sp<PaintFilter> inner);
  ~ComposePaintFilter() override;

 private:
  sk_sp<PaintFilter> outer_;
  sk_sp<PaintFilter> inner_;
};

class CC_PAINT_EXPORT AlphaThresholdPaintFilter final : public PaintFilter {
 public:
  static constexpr Type kType = Type::kAlphaThreshold;
  AlphaThresholdPaintFilter(const SkRegion& region,
                            SkScalar inner_min,
                            SkScalar outer_max,
                            sk_sp<PaintFilter> input,
                            const CropRect* crop_rect = nullptr);
  ~AlphaThresholdPaintFilter() override;

 private:
  SkRegion region_;
  SkScalar inner_min_;
  SkScalar outer_max_;
  sk_sp<PaintFilter> input_;
};

class CC_PAINT_EXPORT ImageFilterPaintFilter final : public PaintFilter {
 public:
  static constexpr Type kType = Type::kSkImageFilter;
  const SkImageFilter* sk_filter() const { return sk_filter_.get(); }

 private:
  // For cross-process transport of FilterOperations. To be removed once
  // PaintFilters can be serialized directly. See crbug.com/777636.
  friend struct IPC::ParamTraits<FilterOperation>;
  friend struct mojo::StructTraits<viz::mojom::FilterOperationDataView,
                                   FilterOperation>;

  explicit ImageFilterPaintFilter(sk_sp<SkImageFilter> sk_filter);
  ~ImageFilterPaintFilter() override;

  sk_sp<SkImageFilter> sk_filter_;
};

class CC_PAINT_EXPORT XfermodePaintFilter final : public PaintFilter {
 public:
  static constexpr Type kType = Type::kXfermode;
  XfermodePaintFilter(SkBlendMode blend_mode,
                      sk_sp<PaintFilter> background,
                      sk_sp<PaintFilter> foreground,
                      const CropRect* crop_rect = nullptr);
  ~XfermodePaintFilter() override;

  const sk_sp<PaintFilter>& background() const { return background_; }
  const sk_sp<PaintFilter>& foreground() const { return foreground_; }

 private:
  SkBlendMode blend_mode_;
  sk_sp<PaintFilter> background_;
  sk_sp<PaintFilter> foreground_;
};

class CC_PAINT_EXPORT ArithmeticPaintFilter final : public PaintFilter {
 public:
  static constexpr Type kType = Type::kArithmetic;
  ArithmeticPaintFilter(float k1,
                        float k2,
                        float k3,
                        float k4,
                        bool enforce_pm_color,
                        sk_sp<PaintFilter> background,
                        sk_sp<PaintFilter> foreground,
                        const CropRect* crop_rect);
  ~ArithmeticPaintFilter() override;

 private:
  float floats_[4];
  bool enforce_pm_color_;
  sk_sp<PaintFilter> background_;
  sk_sp<PaintFilter> foreground_;
};

class CC_PAINT_EXPORT MatrixConvolutionPaintFilter final : public PaintFilter {
 public:
  using TileMode = SkMatrixConvolutionImageFilter::TileMode;
  static constexpr Type kType = Type::kMatrixConvolution;
  MatrixConvolutionPaintFilter(const SkISize& kernel_size,
                               const SkScalar* kernel,
                               SkScalar gain,
                               SkScalar bias,
                               const SkIPoint& kernel_offset,
                               TileMode tile_mode,
                               bool convolve_alpha,
                               sk_sp<PaintFilter> input,
                               const CropRect* crop_rect = nullptr);
  ~MatrixConvolutionPaintFilter() override;

 private:
  SkISize kernel_size_;
  base::StackVector<SkScalar, 3> kernel_;
  SkScalar gain_;
  SkScalar bias_;
  SkIPoint kernel_offset_;
  TileMode tile_mode_;
  bool convolve_alpha_;
  sk_sp<PaintFilter> input_;
};

class CC_PAINT_EXPORT DisplacementMapEffectPaintFilter final
    : public PaintFilter {
 public:
  using ChannelSelectorType = SkDisplacementMapEffect::ChannelSelectorType;
  static constexpr Type kType = Type::kDisplacementMapEffect;
  DisplacementMapEffectPaintFilter(ChannelSelectorType channel_x,
                                   ChannelSelectorType channel_y,
                                   SkScalar scale,
                                   sk_sp<PaintFilter> displacement,
                                   sk_sp<PaintFilter> color,
                                   const CropRect* crop_rect = nullptr);
  ~DisplacementMapEffectPaintFilter() override;

 private:
  ChannelSelectorType channel_x_;
  ChannelSelectorType channel_y_;
  SkScalar scale_;
  sk_sp<PaintFilter> displacement_;
  sk_sp<PaintFilter> color_;
};

class CC_PAINT_EXPORT ImagePaintFilter final : public PaintFilter {
 public:
  static constexpr Type kType = Type::kImage;
  ImagePaintFilter(PaintImage image,
                   const SkRect& src_rect,
                   const SkRect& dst_rect,
                   SkFilterQuality filter_quality);
  ~ImagePaintFilter() override;

 private:
  PaintImage image_;
  SkRect src_rect_;
  SkRect dst_rect_;
  SkFilterQuality filter_quality_;
};

class CC_PAINT_EXPORT RecordPaintFilter final : public PaintFilter {
 public:
  static constexpr Type kType = Type::kPaintRecord;
  RecordPaintFilter(sk_sp<PaintRecord> record, const SkRect& record_bounds);
  ~RecordPaintFilter() override;

 private:
  sk_sp<PaintRecord> record_;
  SkRect record_bounds_;
};

class CC_PAINT_EXPORT MergePaintFilter final : public PaintFilter {
 public:
  static constexpr Type kType = Type::kMerge;
  MergePaintFilter(sk_sp<PaintFilter>* const filters,
                   int count,
                   const CropRect* crop_rect = nullptr);
  ~MergePaintFilter() override;

  size_t input_count() const { return inputs_->size(); }
  const PaintFilter* input_at(size_t i) const {
    DCHECK_LT(i, input_count());
    return inputs_[i].get();
  }

 private:
  base::StackVector<sk_sp<PaintFilter>, 2> inputs_;
};

class CC_PAINT_EXPORT MorphologyPaintFilter final : public PaintFilter {
 public:
  enum class MorphType : uint8_t { kDilate, kErode };
  static constexpr Type kType = Type::kMorphology;
  MorphologyPaintFilter(MorphType morph_type,
                        int radius_x,
                        int radius_y,
                        sk_sp<PaintFilter> input,
                        const CropRect* crop_rect = nullptr);
  ~MorphologyPaintFilter() override;

 private:
  MorphType morph_type_;
  int radius_x_;
  int radius_y_;
  sk_sp<PaintFilter> input_;
};

class CC_PAINT_EXPORT OffsetPaintFilter final : public PaintFilter {
 public:
  static constexpr Type kType = Type::kOffset;
  OffsetPaintFilter(SkScalar dx,
                    SkScalar dy,
                    sk_sp<PaintFilter> input,
                    const CropRect* crop_rect = nullptr);
  ~OffsetPaintFilter() override;

 private:
  SkScalar dx_;
  SkScalar dy_;
  sk_sp<PaintFilter> input_;
};

class CC_PAINT_EXPORT TilePaintFilter final : public PaintFilter {
 public:
  static constexpr Type kType = Type::kTile;
  TilePaintFilter(const SkRect& src,
                  const SkRect& dst,
                  sk_sp<PaintFilter> input);
  ~TilePaintFilter() override;

 private:
  SkRect src_;
  SkRect dst_;
  sk_sp<PaintFilter> input_;
};

class CC_PAINT_EXPORT TurbulencePaintFilter final : public PaintFilter {
 public:
  static constexpr Type kType = Type::kTurbulence;
  enum class TurbulenceType : uint8_t { kTurbulence, kFractalNoise };
  TurbulencePaintFilter(TurbulenceType turbulence_type,
                        SkScalar base_frequency_x,
                        SkScalar base_frequency_y,
                        int num_octaves,
                        SkScalar seed,
                        const SkISize* tile_size,
                        const CropRect* crop_rect = nullptr);
  ~TurbulencePaintFilter() override;

 private:
  TurbulenceType turbulence_type_;
  SkScalar base_frequency_x_;
  SkScalar base_frequency_y_;
  int num_octaves_;
  SkScalar seed_;
  SkISize tile_size_;
};

class CC_PAINT_EXPORT PaintFlagsPaintFilter final : public PaintFilter {
 public:
  static constexpr Type kType = Type::kPaintFlags;
  explicit PaintFlagsPaintFilter(PaintFlags flags,
                                 const CropRect* crop_rect = nullptr);
  ~PaintFlagsPaintFilter() override;

 private:
  PaintFlags flags_;
};

class CC_PAINT_EXPORT MatrixPaintFilter final : public PaintFilter {
 public:
  static constexpr Type kType = Type::kMatrix;
  MatrixPaintFilter(const SkMatrix& matrix,
                    SkFilterQuality filter_quality,
                    sk_sp<PaintFilter> input);
  ~MatrixPaintFilter() override;

 private:
  SkMatrix matrix_;
  SkFilterQuality filter_quality_;
  sk_sp<PaintFilter> input_;
};

class CC_PAINT_EXPORT LightingDistantPaintFilter final : public PaintFilter {
 public:
  static constexpr Type kType = Type::kLightingDistant;
  // kConstant refers to the kd (diffuse) or ks (specular) depending on the
  // LightingType.
  // For specular lighting type only, shininess denotes the specular exponent.
  LightingDistantPaintFilter(LightingType lighting_type,
                             const SkPoint3& direction,
                             SkColor light_color,
                             SkScalar surface_scale,
                             SkScalar kconstant,
                             SkScalar shininess,
                             sk_sp<PaintFilter> input,
                             const CropRect* crop_rect = nullptr);
  ~LightingDistantPaintFilter() override;

 private:
  LightingType lighting_type_;
  SkPoint3 direction_;
  SkColor light_color_;
  SkScalar surface_scale_;
  SkScalar kconstant_;
  SkScalar shininess_;
  sk_sp<PaintFilter> input_;
};

class CC_PAINT_EXPORT LightingPointPaintFilter final : public PaintFilter {
 public:
  static constexpr Type kType = Type::kLightingPoint;
  // kConstant refers to the kd (diffuse) or ks (specular) depending on the
  // LightingType.
  // For specular lighting type only, shininess denotes the specular exponent.
  LightingPointPaintFilter(LightingType lighting_type,
                           const SkPoint3& location,
                           SkColor light_color,
                           SkScalar surface_scale,
                           SkScalar kconstant,
                           SkScalar shininess,
                           sk_sp<PaintFilter> input,
                           const CropRect* crop_rect = nullptr);
  ~LightingPointPaintFilter() override;

 private:
  LightingType lighting_type_;
  SkPoint3 location_;
  SkColor light_color_;
  SkScalar surface_scale_;
  SkScalar kconstant_;
  SkScalar shininess_;
  sk_sp<PaintFilter> input_;
};

class CC_PAINT_EXPORT LightingSpotPaintFilter final : public PaintFilter {
 public:
  static constexpr Type kType = Type::kLightingSpot;
  // kConstant refers to the kd (diffuse) or ks (specular) depending on the
  // LightingType.
  // For specular lighting type only, shininess denotes the specular exponent.
  LightingSpotPaintFilter(LightingType lighting_type,
                          const SkPoint3& location,
                          const SkPoint3& target,
                          SkScalar specular_exponent,
                          SkScalar cutoff_angle,
                          SkColor light_color,
                          SkScalar surface_scale,
                          SkScalar kconstant,
                          SkScalar shininess,
                          sk_sp<PaintFilter> input,
                          const CropRect* crop_rect = nullptr);
  ~LightingSpotPaintFilter() override;

 private:
  LightingType lighting_type_;
  SkPoint3 location_;
  SkPoint3 target_;
  SkScalar specular_exponent_;
  SkScalar cutoff_angle_;
  SkColor light_color_;
  SkScalar surface_scale_;
  SkScalar kconstant_;
  SkScalar shininess_;
  sk_sp<PaintFilter> input_;
};

}  // namespace cc

#endif  // CC_PAINT_PAINT_FILTER_H_
