// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/render_surface_filters.h"

#include <algorithm>

#include "base/logging.h"
#include "cc/output/filter_operation.h"
#include "cc/output/filter_operations.h"
#include "skia/ext/refptr.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/effects/SkBlurImageFilter.h"
#include "third_party/skia/include/effects/SkColorMatrixFilter.h"
#include "third_party/skia/include/effects/SkMagnifierImageFilter.h"
#include "third_party/skia/include/gpu/SkGpuDevice.h"
#include "third_party/skia/include/gpu/SkGrPixelRef.h"
#include "ui/gfx/size_f.h"

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

// The 5x4 matrix is really a "compressed" version of a 5x5 matrix that'd have
// (0 0 0 0 1) as a last row, and that would be applied to a 5-vector extended
// from the 4-vector color with a 1.
void MultColorMatrix(SkScalar a[20], SkScalar b[20], SkScalar out[20]) {
  for (int j = 0; j < 4; ++j) {
    for (int i = 0; i < 5; ++i) {
      out[i+j*5] = i == 4 ? a[4+j*5] : 0.f;
      for (int k = 0; k < 4; ++k)
        out[i+j*5] += a[k+j*5] * b[i+k*5];
    }
  }
}

// To detect if we need to apply clamping after applying a matrix, we check if
// any output component might go outside of [0, 255] for any combination of
// input components in [0..255].
// Each output component is an affine transformation of the input component, so
// the minimum and maximum values are for any combination of minimum or maximum
// values of input components (i.e. 0 or 255).
// E.g. if R' = x*R + y*G + z*B + w*A + t
// Then the maximum value will be for R=255 if x>0 or R=0 if x<0, and the
// minimum value will be for R=0 if x>0 or R=255 if x<0.
// Same goes for all components.
bool ComponentNeedsClamping(SkScalar row[5]) {
  SkScalar max_value = row[4] / 255.f;
  SkScalar min_value = row[4] / 255.f;
  for (int i = 0; i < 4; ++i) {
    if (row[i] > 0)
      max_value += row[i];
    else
      min_value += row[i];
  }
  return (max_value > 1.f) || (min_value < 0.f);
}

bool MatrixNeedsClamping(SkScalar matrix[20]) {
  return ComponentNeedsClamping(matrix)
      || ComponentNeedsClamping(matrix+5)
      || ComponentNeedsClamping(matrix+10)
      || ComponentNeedsClamping(matrix+15);
}

bool GetColorMatrix(const FilterOperation& op, SkScalar matrix[20]) {
  switch (op.type()) {
    case FilterOperation::BRIGHTNESS: {
      GetBrightnessMatrix(op.amount(), matrix);
      return true;
    }
    case FilterOperation::SATURATING_BRIGHTNESS: {
      GetSaturatingBrightnessMatrix(op.amount(), matrix);
      return true;
    }
    case FilterOperation::CONTRAST: {
      GetContrastMatrix(op.amount(), matrix);
      return true;
    }
    case FilterOperation::GRAYSCALE: {
      GetGrayscaleMatrix(1.f - op.amount(), matrix);
      return true;
    }
    case FilterOperation::SEPIA: {
      GetSepiaMatrix(1.f - op.amount(), matrix);
      return true;
    }
    case FilterOperation::SATURATE: {
      GetSaturateMatrix(op.amount(), matrix);
      return true;
    }
    case FilterOperation::HUE_ROTATE: {
      GetHueRotateMatrix(op.amount(), matrix);
      return true;
    }
    case FilterOperation::INVERT: {
      GetInvertMatrix(op.amount(), matrix);
      return true;
    }
    case FilterOperation::OPACITY: {
      GetOpacityMatrix(op.amount(), matrix);
      return true;
    }
    case FilterOperation::COLOR_MATRIX: {
      memcpy(matrix, op.matrix(), sizeof(SkScalar[20]));
      return true;
    }
    default:
      return false;
  }
}

class FilterBufferState {
 public:
  FilterBufferState(GrContext* gr_context,
                    gfx::SizeF size,
                    unsigned texture_id)
      : gr_context_(gr_context),
        current_texture_(0) {
    // Wrap the source texture in a Ganesh platform texture.
    GrBackendTextureDesc backend_texture_description;
    backend_texture_description.fWidth = size.width();
    backend_texture_description.fHeight = size.height();
    backend_texture_description.fConfig = kSkia8888_GrPixelConfig;
    backend_texture_description.fTextureHandle = texture_id;
    skia::RefPtr<GrTexture> texture = skia::AdoptRef(
        gr_context->wrapBackendTexture(backend_texture_description));
    // Place the platform texture inside an SkBitmap.
    source_.setConfig(SkBitmap::kARGB_8888_Config, size.width(), size.height());
    skia::RefPtr<SkGrPixelRef> pixel_ref =
        skia::AdoptRef(new SkGrPixelRef(texture.get()));
    source_.setPixelRef(pixel_ref.get());
  }

  ~FilterBufferState() {}

  bool Init(int filter_count) {
    int scratch_count = std::min(2, filter_count);
    GrTextureDesc desc;
    desc.fFlags = kRenderTarget_GrTextureFlagBit | kNoStencil_GrTextureFlagBit;
    desc.fSampleCnt = 0;
    desc.fWidth = source_.width();
    desc.fHeight = source_.height();
    desc.fConfig = kSkia8888_GrPixelConfig;
    for (int i = 0; i < scratch_count; ++i) {
      GrAutoScratchTexture scratch_texture(
          gr_context_, desc, GrContext::kExact_ScratchTexMatch);
      scratch_textures_[i] = skia::AdoptRef(scratch_texture.detach());
      if (!scratch_textures_[i])
        return false;
    }
    return true;
  }

  SkCanvas* Canvas() {
    if (!canvas_)
      CreateCanvas();
    return canvas_.get();
  }

  const SkBitmap& Source() { return source_; }

  void Swap() {
    canvas_->flush();
    canvas_.clear();
    device_.clear();

    skia::RefPtr<SkGrPixelRef> pixel_ref = skia::AdoptRef(
        new SkGrPixelRef(scratch_textures_[current_texture_].get()));
    source_.setPixelRef(pixel_ref.get());
    current_texture_ = 1 - current_texture_;
  }

 private:
  void CreateCanvas() {
    DCHECK(scratch_textures_[current_texture_].get());
    device_ = skia::AdoptRef(new SkGpuDevice(
        gr_context_, scratch_textures_[current_texture_].get()));
    canvas_ = skia::AdoptRef(new SkCanvas(device_.get()));
    canvas_->clear(0x0);
  }

  GrContext* gr_context_;
  SkBitmap source_;
  skia::RefPtr<GrTexture> scratch_textures_[2];
  int current_texture_;
  skia::RefPtr<SkGpuDevice> device_;
  skia::RefPtr<SkCanvas> canvas_;
};

}  // namespace

FilterOperations RenderSurfaceFilters::Optimize(
    const FilterOperations& filters) {
  FilterOperations new_list;

  SkScalar accumulated_color_matrix[20];
  bool have_accumulated_color_matrix = false;
  for (unsigned i = 0; i < filters.size(); ++i) {
    const FilterOperation& op = filters.at(i);

    // If the filter is a color matrix, we may be able to combine it with
    // following Filter(s) that also are color matrices.
    SkScalar matrix[20];
    if (GetColorMatrix(op, matrix)) {
      if (have_accumulated_color_matrix) {
        SkScalar new_matrix[20];
        MultColorMatrix(matrix, accumulated_color_matrix, new_matrix);
        memcpy(accumulated_color_matrix,
               new_matrix,
               sizeof(accumulated_color_matrix));
      } else {
        memcpy(accumulated_color_matrix,
               matrix,
               sizeof(accumulated_color_matrix));
        have_accumulated_color_matrix = true;
      }

      // We can only combine matrices if clamping of color components
      // would have no effect.
      if (!MatrixNeedsClamping(accumulated_color_matrix))
        continue;
    }

    if (have_accumulated_color_matrix) {
      new_list.Append(FilterOperation::CreateColorMatrixFilter(
          accumulated_color_matrix));
    }
    have_accumulated_color_matrix = false;

    switch (op.type()) {
      case FilterOperation::BLUR:
      case FilterOperation::DROP_SHADOW:
      case FilterOperation::ZOOM:
        new_list.Append(op);
        break;
      case FilterOperation::BRIGHTNESS:
      case FilterOperation::SATURATING_BRIGHTNESS:
      case FilterOperation::CONTRAST:
      case FilterOperation::GRAYSCALE:
      case FilterOperation::SEPIA:
      case FilterOperation::SATURATE:
      case FilterOperation::HUE_ROTATE:
      case FilterOperation::INVERT:
      case FilterOperation::OPACITY:
      case FilterOperation::COLOR_MATRIX:
        break;
    }
  }
  if (have_accumulated_color_matrix) {
    new_list.Append(FilterOperation::CreateColorMatrixFilter(
        accumulated_color_matrix));
  }
  return new_list;
}

SkBitmap RenderSurfaceFilters::Apply(const FilterOperations& filters,
                                     unsigned texture_id,
                                     gfx::SizeF size,
                                     GrContext* gr_context) {
  DCHECK(gr_context);

  FilterBufferState state(gr_context, size, texture_id);
  if (!state.Init(filters.size()))
    return SkBitmap();

  for (unsigned i = 0; i < filters.size(); ++i) {
    const FilterOperation& op = filters.at(i);
    SkCanvas* canvas = state.Canvas();
    switch (op.type()) {
      case FilterOperation::COLOR_MATRIX: {
        SkPaint paint;
        skia::RefPtr<SkColorMatrixFilter> filter =
            skia::AdoptRef(new SkColorMatrixFilter(op.matrix()));
        paint.setColorFilter(filter.get());
        canvas->drawBitmap(state.Source(), 0, 0, &paint);
        break;
      }
      case FilterOperation::BLUR: {
        float std_deviation = op.amount();
        skia::RefPtr<SkImageFilter> filter =
            skia::AdoptRef(new SkBlurImageFilter(std_deviation, std_deviation));
        SkPaint paint;
        paint.setImageFilter(filter.get());
        canvas->drawSprite(state.Source(), 0, 0, &paint);
        break;
      }
      case FilterOperation::DROP_SHADOW: {
        skia::RefPtr<SkImageFilter> blur_filter =
            skia::AdoptRef(new SkBlurImageFilter(op.amount(), op.amount()));
        skia::RefPtr<SkColorFilter> color_filter =
            skia::AdoptRef(SkColorFilter::CreateModeFilter(
                op.drop_shadow_color(), SkXfermode::kSrcIn_Mode));
        SkPaint paint;
        paint.setImageFilter(blur_filter.get());
        paint.setColorFilter(color_filter.get());
        paint.setXfermodeMode(SkXfermode::kSrcOver_Mode);
        canvas->saveLayer(NULL, &paint);
        canvas->drawBitmap(state.Source(),
                           op.drop_shadow_offset().x(),
                           op.drop_shadow_offset().y());
        canvas->restore();
        canvas->drawBitmap(state.Source(), 0, 0);
        break;
      }
      case FilterOperation::ZOOM: {
        SkPaint paint;
        int width = state.Source().width();
        int height = state.Source().height();
        skia::RefPtr<SkImageFilter> zoom_filter = skia::AdoptRef(
            new SkMagnifierImageFilter(
                SkRect::MakeXYWH(
                    (width - (width / op.amount())) / 2.f,
                    (height - (height / op.amount())) / 2.f,
                    width / op.amount(),
                    height / op.amount()),
                op.zoom_inset()));
        paint.setImageFilter(zoom_filter.get());
        canvas->saveLayer(NULL, &paint);
        canvas->drawBitmap(state.Source(), 0, 0);
        canvas->restore();
        break;
      }
      case FilterOperation::BRIGHTNESS:
      case FilterOperation::SATURATING_BRIGHTNESS:
      case FilterOperation::CONTRAST:
      case FilterOperation::GRAYSCALE:
      case FilterOperation::SEPIA:
      case FilterOperation::SATURATE:
      case FilterOperation::HUE_ROTATE:
      case FilterOperation::INVERT:
      case FilterOperation::OPACITY:
        NOTREACHED();
        break;
    }
    state.Swap();
  }
  return state.Source();
}

}  // namespace cc
