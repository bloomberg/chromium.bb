// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/scoped_image_flags.h"

#include "cc/paint/image_provider.h"
#include "cc/paint/paint_image_builder.h"

namespace cc {
namespace {
SkIRect RoundOutRect(const SkRect& rect) {
  SkIRect result;
  rect.roundOut(&result);
  return result;
}
}  // namespace

ScopedImageFlags::DecodeStashingImageProvider::DecodeStashingImageProvider(
    ImageProvider* source_provider)
    : source_provider_(source_provider) {
  DCHECK(source_provider_);
}
ScopedImageFlags::DecodeStashingImageProvider::~DecodeStashingImageProvider() =
    default;

ImageProvider::ScopedDecodedDrawImage
ScopedImageFlags::DecodeStashingImageProvider::GetDecodedDrawImage(
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

ScopedImageFlags::ScopedImageFlags(ImageProvider* image_provider,
                                   const PaintFlags* flags,
                                   const SkMatrix& ctm,
                                   uint8_t alpha) {
  DCHECK(flags->HasDiscardableImages() || alpha != 255);
  if (flags->HasDiscardableImages() && image_provider) {
    DCHECK(flags->HasShader());

    decode_stashing_image_provider_.emplace(image_provider);
    modified_flags_.emplace(*flags);
    if (flags->getShader()->shader_type() == PaintShader::Type::kImage) {
      DecodeImageShader(flags, ctm);
    } else if (flags->getShader()->shader_type() ==
               PaintShader::Type::kPaintRecord) {
      DecodeRecordShader(flags, ctm);
    } else {
      NOTREACHED();
    }

    // We skip the op if any images fail to decode.
    if (decode_failed_)
      return;
  }

  if (alpha != 255) {
    DCHECK(flags->SupportsFoldingAlpha());
    if (!modified_flags_)
      modified_flags_.emplace(*flags);
    modified_flags_->setAlpha(SkMulDiv255Round(flags->getAlpha(), alpha));
  }

  DCHECK(modified_flags_);
}

ScopedImageFlags::~ScopedImageFlags() = default;

void ScopedImageFlags::DecodeImageShader(const PaintFlags* original,
                                         const SkMatrix& ctm) {
  const PaintImage& paint_image = original->getShader()->paint_image();
  SkMatrix matrix = original->getShader()->GetLocalMatrix();

  SkMatrix total_image_matrix = matrix;
  total_image_matrix.preConcat(ctm);
  SkRect src_rect = SkRect::MakeIWH(paint_image.width(), paint_image.height());
  DrawImage draw_image(paint_image, RoundOutRect(src_rect),
                       original->getFilterQuality(), total_image_matrix);
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
  modified_flags_->setFilterQuality(decoded_image.filter_quality());
  modified_flags_->setShader(
      PaintShader::MakeImage(decoded_paint_image, original->getShader()->tx(),
                             original->getShader()->ty(), &matrix));
}

void ScopedImageFlags::DecodeRecordShader(const PaintFlags* original,
                                          const SkMatrix& ctm) {
  auto decoded_shader = original->getShader()->CreateDecodedPaintRecord(
      ctm, &*decode_stashing_image_provider_);
  if (!decoded_shader) {
    decode_failed_ = true;
    return;
  }

  modified_flags_->setShader(std::move(decoded_shader));
}

}  // namespace cc
