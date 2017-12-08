// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/scoped_raster_flags.h"

#include "cc/paint/image_provider.h"
#include "cc/paint/paint_image_builder.h"

namespace cc {
ScopedRasterFlags::DecodeStashingImageProvider::DecodeStashingImageProvider(
    ImageProvider* source_provider)
    : source_provider_(source_provider) {
  DCHECK(source_provider_);
}
ScopedRasterFlags::DecodeStashingImageProvider::~DecodeStashingImageProvider() =
    default;

ImageProvider::ScopedDecodedDrawImage
ScopedRasterFlags::DecodeStashingImageProvider::GetDecodedDrawImage(
    const DrawImage& draw_image) {
  auto decode = source_provider_->GetDecodedDrawImage(draw_image);
  if (!decode)
    return ScopedDecodedDrawImage();

  // No need to add any destruction callback to the returned image. The images
  // decoded here match the lifetime of this provider.
  auto image_to_return = ScopedDecodedDrawImage(decode.decoded_image());
  decoded_images_->push_back(std::move(decode));
  return image_to_return;
}

ScopedRasterFlags::ScopedRasterFlags(const PaintFlags* flags,
                                     ImageProvider* image_provider,
                                     const SkMatrix& ctm,
                                     uint8_t alpha)
    : original_flags_(flags) {
  if (flags->HasDiscardableImages() && image_provider) {
    DCHECK(flags->HasShader());

    decode_stashing_image_provider_.emplace(image_provider);
    if (flags->getShader()->shader_type() == PaintShader::Type::kImage) {
      DecodeImageShader(ctm);
    } else if (flags->getShader()->shader_type() ==
               PaintShader::Type::kPaintRecord) {
      DecodeRecordShader(ctm);
    } else {
      NOTREACHED();
    }

    // We skip the op if any images fail to decode.
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
  DCHECK(decoded_image.image());

  bool need_scale = !decoded_image.is_scale_adjustment_identity();
  if (need_scale) {
    matrix.preScale(1.f / decoded_image.scale_adjustment().width(),
                    1.f / decoded_image.scale_adjustment().height());
  }

  sk_sp<SkImage> sk_image =
      sk_ref_sp<SkImage>(const_cast<SkImage*>(decoded_image.image().get()));
  PaintImage decoded_paint_image = PaintImageBuilder::WithDefault()
                                       .set_id(paint_image.stable_id())
                                       .set_image(std::move(sk_image))
                                       .TakePaintImage();
  MutableFlags()->setFilterQuality(decoded_image.filter_quality());
  MutableFlags()->setShader(
      PaintShader::MakeImage(decoded_paint_image, flags()->getShader()->tx(),
                             flags()->getShader()->ty(), &matrix));
}

void ScopedRasterFlags::DecodeRecordShader(const SkMatrix& ctm) {
  auto decoded_shader = flags()->getShader()->CreateDecodedPaintRecord(
      ctm, &*decode_stashing_image_provider_);
  if (!decoded_shader) {
    decode_failed_ = true;
    return;
  }

  MutableFlags()->setShader(std::move(decoded_shader));
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
