// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/base/render_surface_filters.h"

#include <stddef.h>

#include <algorithm>

#include "cc/base/filter_operation.h"
#include "cc/base/filter_operations.h"
#include "third_party/skia/include/core/SkImageFilter.h"
#include "third_party/skia/include/effects/SkAlphaThresholdFilter.h"
#include "third_party/skia/include/effects/SkColorFilterImageFilter.h"
#include "third_party/skia/include/effects/SkColorMatrixFilter.h"
#include "third_party/skia/include/effects/SkComposeImageFilter.h"
#include "third_party/skia/include/effects/SkDropShadowImageFilter.h"
#include "third_party/skia/include/effects/SkMagnifierImageFilter.h"
#include "ui/gfx/geometry/size_f.h"

namespace cc {

namespace {

void GetBrightnessMatrix(float amount, SkScalar matrix[20]) {
  // Spec implementation
  // (http://dvcs.w3.org/hg/FXTF/raw-file/tip/filters/index.html#brightnessEquivalent)
  // <feFunc[R|G|B] type="linear" slope="[amount]">
  memset(matrix, 0, 20 * sizeof(SkScalar));
  matrix[0] = matrix[6] = matrix[12] = amount;
  matrix[18] = 1.f;
}

void GetSaturatingBrightnessMatrix(float amount, SkScalar matrix[20]) {
  // Legacy implementation used by internal clients.
  // <feFunc[R|G|B] type="linear" intercept="[amount]"/>
  memset(matrix, 0, 20 * sizeof(SkScalar));
  matrix[0] = matrix[6] = matrix[12] = matrix[18] = 1.f;
  matrix[4] = matrix[9] = matrix[14] = amount * 255.f;
}

void GetContrastMatrix(float amount, SkScalar matrix[20]) {
  memset(matrix, 0, 20 * sizeof(SkScalar));
  matrix[0] = matrix[6] = matrix[12] = amount;
  matrix[4] = matrix[9] = matrix[14] = (-0.5f * amount + 0.5f) * 255.f;
  matrix[18] = 1.f;
}

void GetSaturateMatrix(float amount, SkScalar matrix[20]) {
  // Note, these values are computed to ensure MatrixNeedsClamping is false
  // for amount in [0..1]
  matrix[0] = 0.213f + 0.787f * amount;
  matrix[1] = 0.715f - 0.715f * amount;
  matrix[2] = 1.f - (matrix[0] + matrix[1]);
  matrix[3] = matrix[4] = 0.f;
  matrix[5] = 0.213f - 0.213f * amount;
  matrix[6] = 0.715f + 0.285f * amount;
  matrix[7] = 1.f - (matrix[5] + matrix[6]);
  matrix[8] = matrix[9] = 0.f;
  matrix[10] = 0.213f - 0.213f * amount;
  matrix[11] = 0.715f - 0.715f * amount;
  matrix[12] = 1.f - (matrix[10] + matrix[11]);
  matrix[13] = matrix[14] = 0.f;
  matrix[15] = matrix[16] = matrix[17] = matrix[19] = 0.f;
  matrix[18] = 1.f;
}

void GetHueRotateMatrix(float hue, SkScalar matrix[20]) {
  const float kPi = 3.1415926535897932384626433832795f;

  float cos_hue = cosf(hue * kPi / 180.f);
  float sin_hue = sinf(hue * kPi / 180.f);
  matrix[0] = 0.213f + cos_hue * 0.787f - sin_hue * 0.213f;
  matrix[1] = 0.715f - cos_hue * 0.715f - sin_hue * 0.715f;
  matrix[2] = 0.072f - cos_hue * 0.072f + sin_hue * 0.928f;
  matrix[3] = matrix[4] = 0.f;
  matrix[5] = 0.213f - cos_hue * 0.213f + sin_hue * 0.143f;
  matrix[6] = 0.715f + cos_hue * 0.285f + sin_hue * 0.140f;
  matrix[7] = 0.072f - cos_hue * 0.072f - sin_hue * 0.283f;
  matrix[8] = matrix[9] = 0.f;
  matrix[10] = 0.213f - cos_hue * 0.213f - sin_hue * 0.787f;
  matrix[11] = 0.715f - cos_hue * 0.715f + sin_hue * 0.715f;
  matrix[12] = 0.072f + cos_hue * 0.928f + sin_hue * 0.072f;
  matrix[13] = matrix[14] = 0.f;
  matrix[15] = matrix[16] = matrix[17] = 0.f;
  matrix[18] = 1.f;
  matrix[19] = 0.f;
}

void GetInvertMatrix(float amount, SkScalar matrix[20]) {
  memset(matrix, 0, 20 * sizeof(SkScalar));
  matrix[0] = matrix[6] = matrix[12] = 1.f - 2.f * amount;
  matrix[4] = matrix[9] = matrix[14] = amount * 255.f;
  matrix[18] = 1.f;
}

void GetOpacityMatrix(float amount, SkScalar matrix[20]) {
  memset(matrix, 0, 20 * sizeof(SkScalar));
  matrix[0] = matrix[6] = matrix[12] = 1.f;
  matrix[18] = amount;
}

void GetGrayscaleMatrix(float amount, SkScalar matrix[20]) {
  // Note, these values are computed to ensure MatrixNeedsClamping is false
  // for amount in [0..1]
  matrix[0] = 0.2126f + 0.7874f * amount;
  matrix[1] = 0.7152f - 0.7152f * amount;
  matrix[2] = 1.f - (matrix[0] + matrix[1]);
  matrix[3] = matrix[4] = 0.f;

  matrix[5] = 0.2126f - 0.2126f * amount;
  matrix[6] = 0.7152f + 0.2848f * amount;
  matrix[7] = 1.f - (matrix[5] + matrix[6]);
  matrix[8] = matrix[9] = 0.f;

  matrix[10] = 0.2126f - 0.2126f * amount;
  matrix[11] = 0.7152f - 0.7152f * amount;
  matrix[12] = 1.f - (matrix[10] + matrix[11]);
  matrix[13] = matrix[14] = 0.f;

  matrix[15] = matrix[16] = matrix[17] = matrix[19] = 0.f;
  matrix[18] = 1.f;
}

void GetSepiaMatrix(float amount, SkScalar matrix[20]) {
  matrix[0] = 0.393f + 0.607f * amount;
  matrix[1] = 0.769f - 0.769f * amount;
  matrix[2] = 0.189f - 0.189f * amount;
  matrix[3] = matrix[4] = 0.f;

  matrix[5] = 0.349f - 0.349f * amount;
  matrix[6] = 0.686f + 0.314f * amount;
  matrix[7] = 0.168f - 0.168f * amount;
  matrix[8] = matrix[9] = 0.f;

  matrix[10] = 0.272f - 0.272f * amount;
  matrix[11] = 0.534f - 0.534f * amount;
  matrix[12] = 0.131f + 0.869f * amount;
  matrix[13] = matrix[14] = 0.f;

  matrix[15] = matrix[16] = matrix[17] = matrix[19] = 0.f;
  matrix[18] = 1.f;
}

sk_sp<SkImageFilter> CreateMatrixImageFilter(const SkScalar matrix[20],
                                             sk_sp<SkImageFilter> input) {
  return SkColorFilterImageFilter::Make(
      SkColorFilter::MakeMatrixFilterRowMajor255(matrix), std::move(input));
}

}  // namespace

sk_sp<SkImageFilter> RenderSurfaceFilters::BuildImageFilter(
    const FilterOperations& filters,
    const gfx::SizeF& size,
    const gfx::Vector2dF& offset) {
  sk_sp<SkImageFilter> image_filter;
  SkScalar matrix[20];
  for (size_t i = 0; i < filters.size(); ++i) {
    const FilterOperation& op = filters.at(i);
    switch (op.type()) {
      case FilterOperation::GRAYSCALE:
        GetGrayscaleMatrix(1.f - op.amount(), matrix);
        image_filter = CreateMatrixImageFilter(matrix, std::move(image_filter));
        break;
      case FilterOperation::SEPIA:
        GetSepiaMatrix(1.f - op.amount(), matrix);
        image_filter = CreateMatrixImageFilter(matrix, std::move(image_filter));
        break;
      case FilterOperation::SATURATE:
        GetSaturateMatrix(op.amount(), matrix);
        image_filter = CreateMatrixImageFilter(matrix, std::move(image_filter));
        break;
      case FilterOperation::HUE_ROTATE:
        GetHueRotateMatrix(op.amount(), matrix);
        image_filter = CreateMatrixImageFilter(matrix, std::move(image_filter));
        break;
      case FilterOperation::INVERT:
        GetInvertMatrix(op.amount(), matrix);
        image_filter = CreateMatrixImageFilter(matrix, std::move(image_filter));
        break;
      case FilterOperation::OPACITY:
        GetOpacityMatrix(op.amount(), matrix);
        image_filter = CreateMatrixImageFilter(matrix, std::move(image_filter));
        break;
      case FilterOperation::BRIGHTNESS:
        GetBrightnessMatrix(op.amount(), matrix);
        image_filter = CreateMatrixImageFilter(matrix, std::move(image_filter));
        break;
      case FilterOperation::CONTRAST:
        GetContrastMatrix(op.amount(), matrix);
        image_filter = CreateMatrixImageFilter(matrix, std::move(image_filter));
        break;
      case FilterOperation::BLUR:
        image_filter = SkBlurImageFilter::Make(op.amount(), op.amount(),
                                               std::move(image_filter), nullptr,
                                               op.blur_tile_mode());
        break;
      case FilterOperation::DROP_SHADOW:
        image_filter = SkDropShadowImageFilter::Make(
            SkIntToScalar(op.drop_shadow_offset().x()),
            SkIntToScalar(op.drop_shadow_offset().y()),
            SkIntToScalar(op.amount()), SkIntToScalar(op.amount()),
            op.drop_shadow_color(),
            SkDropShadowImageFilter::kDrawShadowAndForeground_ShadowMode,
            std::move(image_filter));
        break;
      case FilterOperation::COLOR_MATRIX:
        image_filter =
            CreateMatrixImageFilter(op.matrix(), std::move(image_filter));
        break;
      case FilterOperation::ZOOM: {
        // The center point, always the midpoint of the unclipped rectangle.
        // When we go to either edge of the screen, the width/height will shrink
        // at the same rate the offset changes. Use abs on the offset since we
        // do not care about the offset direction.
        gfx::Vector2dF center =
            gfx::Vector2dF((size.width() + std::abs(offset.x())) / 2,
                           (size.height() + std::abs(offset.y())) / 2);

        // The dimensions of the source content. This shrinks as the texture
        // rectangle gets clipped.
        gfx::Vector2d src_dimensions =
            gfx::Vector2d((size.width() + std::abs(offset.x())) / op.amount(),
                          (size.height() + std::abs(offset.y())) / op.amount());

        // When the magnifier goes to the left/top border of the screen, we need
        // to adjust the x/y position of the rect. The rate the position gets
        // updated currently only works properly for a 2x magnification.
        DCHECK_EQ(op.amount(), 2.f);
        gfx::Vector2dF center_offset = gfx::Vector2dF(0, 0);
        if (offset.x() >= 0)
          center_offset.set_x(-offset.x() / op.amount());
        if (offset.y() <= 0)
          center_offset.set_y(offset.y() / op.amount());

        sk_sp<SkImageFilter> zoom_filter(SkMagnifierImageFilter::Make(
            SkRect::MakeXYWH(
                (center.x() - src_dimensions.x() / 2.f) + center_offset.x(),
                (center.y() - src_dimensions.y() / 2.f) + center_offset.y(),
                size.width() / op.amount(), size.height() / op.amount()),
            op.zoom_inset(), nullptr));
        if (image_filter) {
          // TODO(ajuma): When there's a 1-input version of
          // SkMagnifierImageFilter, use that to handle the input filter
          // instead of using an SkComposeImageFilter.
          image_filter = SkComposeImageFilter::Make(std::move(zoom_filter),
                                                    std::move(image_filter));
        } else {
          image_filter = std::move(zoom_filter);
        }
        break;
      }
      case FilterOperation::SATURATING_BRIGHTNESS:
        GetSaturatingBrightnessMatrix(op.amount(), matrix);
        image_filter = CreateMatrixImageFilter(matrix, std::move(image_filter));
        break;
      case FilterOperation::REFERENCE: {
        if (!op.image_filter())
          break;

        sk_sp<SkColorFilter> cf;

        {
          SkColorFilter* colorfilter_rawptr = NULL;
          op.image_filter()->asColorFilter(&colorfilter_rawptr);
          cf.reset(colorfilter_rawptr);
        }

        if (cf && cf->asColorMatrix(matrix) &&
            !op.image_filter()->getInput(0)) {
          image_filter =
              CreateMatrixImageFilter(matrix, std::move(image_filter));
        } else if (image_filter) {
          image_filter = SkComposeImageFilter::Make(op.image_filter(),
                                                    std::move(image_filter));
        } else {
          image_filter = op.image_filter();
        }
        break;
      }
      case FilterOperation::ALPHA_THRESHOLD: {
        sk_sp<SkImageFilter> alpha_filter = SkAlphaThresholdFilter::Make(
            op.region(), op.amount(), op.outer_threshold(), nullptr);
        if (image_filter) {
          image_filter = SkComposeImageFilter::Make(std::move(alpha_filter),
                                                    std::move(image_filter));
        } else {
          image_filter = std::move(alpha_filter);
        }
        break;
      }
    }
  }
  return image_filter;
}

}  // namespace cc
