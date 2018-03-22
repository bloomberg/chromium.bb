// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/scoped_raster_flags.h"

#include "cc/paint/image_provider.h"
#include "cc/paint/paint_filter.h"
#include "cc/paint/paint_image_builder.h"

namespace cc {
ScopedRasterFlags::ScopedRasterFlags(const PaintFlags* flags,
                                     ImageProvider* image_provider,
                                     const SkMatrix& ctm,
                                     uint8_t alpha,
                                     bool create_skia_shader)
    : original_flags_(flags) {
  if (flags->HasDiscardableImages() && image_provider) {
    // TODO(khushalsagar): The decoding of images in PaintFlags here is a bit of
    // a mess. We decode image shaders at the correct scale but ignore that
    // during serialization and just use the original image.
    decode_stashing_image_provider_.emplace(image_provider);

    // We skip the op if any images fail to decode.
    DecodeImageShader(ctm);
    if (decode_failed_)
      return;
    DecodeRecordShader(ctm, create_skia_shader);
    if (decode_failed_)
      return;
    DecodeFilter();
    if (decode_failed_)
      return;
  }

  if (alpha != 255) {
    DCHECK(flags->SupportsFoldingAlpha());
    MutableFlags()->setAlpha(SkMulDiv255Round(flags->getAlpha(), alpha));
  }

  AdjustStrokeIfNeeded(ctm);
}

ScopedRasterFlags::~ScopedRasterFlags() = default;

void ScopedRasterFlags::DecodeImageShader(const SkMatrix& ctm) {
  if (!flags()->HasShader() ||
      flags()->getShader()->shader_type() != PaintShader::Type::kImage)
    return;

  const PaintImage& paint_image = flags()->getShader()->paint_image();
  SkMatrix matrix = flags()->getShader()->GetLocalMatrix();

  SkMatrix total_image_matrix = matrix;
  total_image_matrix.preConcat(ctm);
  SkRect src_rect = SkRect::MakeIWH(paint_image.width(), paint_image.height());
  SkIRect int_src_rect;
  src_rect.roundOut(&int_src_rect);
  DrawImage draw_image(paint_image, int_src_rect, flags()->getFilterQuality(),
                       total_image_matrix);
  auto decoded_draw_image =
      decode_stashing_image_provider_->GetDecodedDrawImage(draw_image);

  if (!decoded_draw_image) {
    decode_failed_ = true;
    return;
  }

  const auto& decoded_image = decoded_draw_image.decoded_image();
  // If this image is backed by a transfer cache entry id, then it's suitable
  // for serialization. We don't need to do anything here, because the image
  // provider (accessed via PaintOpWriter) will get the decode and serialize the
  // transfer cache id.
  // Note that if we replace this with the decoded paint image, the
  // serialization will fail, because a transfer cache backed image cannot on
  // its own construct an SkImage which is needed to create an underlying
  // SkShader.
  if (decoded_image.transfer_cache_entry_id()) {
    DCHECK(!decoded_image.image());
    return;
  }
  DCHECK(decoded_image.image());

  bool need_scale = !decoded_image.is_scale_adjustment_identity();
  if (need_scale) {
    matrix.preScale(1.f / decoded_image.scale_adjustment().width(),
                    1.f / decoded_image.scale_adjustment().height());
  }

  sk_sp<SkImage> sk_image =
      sk_ref_sp<SkImage>(const_cast<SkImage*>(decoded_image.image().get()));
  PaintImage decoded_paint_image =
      PaintImageBuilder::WithDefault()
          .set_id(paint_image.stable_id())
          .set_image(std::move(sk_image), PaintImage::kNonLazyStableId)
          .TakePaintImage();
  MutableFlags()->setFilterQuality(decoded_image.filter_quality());
  MutableFlags()->setShader(
      PaintShader::MakeImage(decoded_paint_image, flags()->getShader()->tx(),
                             flags()->getShader()->ty(), &matrix));
}

void ScopedRasterFlags::DecodeRecordShader(const SkMatrix& ctm,
                                           bool create_skia_shader) {
  if (!flags()->HasShader() ||
      flags()->getShader()->shader_type() != PaintShader::Type::kPaintRecord)
    return;

  // TODO(khushalsagar): For OOP, we have to decode everything during
  // serialization. This will force us to use original sized decodes.
  if (flags()->getShader()->image_analysis_state() !=
      ImageAnalysisState::kAnimatedImages)
    return;

  auto decoded_shader = flags()->getShader()->CreateDecodedPaintRecord(
      ctm, &*decode_stashing_image_provider_);
  if (!decoded_shader) {
    decode_failed_ = true;
    return;
  }

  if (create_skia_shader)
    decoded_shader->CreateSkShader(&*decode_stashing_image_provider_);
  MutableFlags()->setShader(std::move(decoded_shader));
}

void ScopedRasterFlags::DecodeFilter() {
  if (!flags()->getImageFilter() ||
      !flags()->getImageFilter()->has_discardable_images() ||
      flags()->getImageFilter()->image_analysis_state() !=
          ImageAnalysisState::kAnimatedImages) {
    return;
  }

  MutableFlags()->setImageFilter(flags()->getImageFilter()->SnapshotWithImages(
      &*decode_stashing_image_provider_));
}

void ScopedRasterFlags::AdjustStrokeIfNeeded(const SkMatrix& ctm) {
  // With anti-aliasing turned off, strokes with a device space width in (0, 1)
  // may not raster at all.  To avoid this, we have two options:
  //
  // 1) force a hairline stroke (stroke-width == 0)
  // 2) force anti-aliasing on

  SkSize scale;
  if (flags()->isAntiAlias() ||                          // safe to raster
      flags()->getStyle() == PaintFlags::kFill_Style ||  // not a stroke
      !flags()->getStrokeWidth() ||                      // explicit hairline
      !ctm.decomposeScale(&scale)) {                     // cannot decompose
    return;
  }

  const auto stroke_vec =
      SkVector::Make(flags()->getStrokeWidth() * scale.width(),
                     flags()->getStrokeWidth() * scale.height());
  if (stroke_vec.x() >= 1.f && stroke_vec.y() >= 1.f)
    return;  // safe to raster

  const auto can_substitute_hairline =
      flags()->getStrokeCap() == PaintFlags::kDefault_Cap &&
      flags()->getStrokeJoin() == PaintFlags::kDefault_Join;
  if (can_substitute_hairline && stroke_vec.x() < 1.f && stroke_vec.y() < 1.f) {
    // Use modulated hairline when possible, as it is faster and produces
    // results closer to the original intent.
    MutableFlags()->setStrokeWidth(0);
    MutableFlags()->setAlpha(std::round(
        flags()->getAlpha() * std::sqrt(stroke_vec.x() * stroke_vec.y())));
    return;
  }

  // Fall back to anti-aliasing.
  MutableFlags()->setAntiAlias(true);
}

}  // namespace cc
