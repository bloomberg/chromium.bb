// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/raster/image_hijack_canvas.h"

#include "base/optional.h"
#include "base/trace_event/trace_event.h"
#include "cc/paint/discardable_image_map.h"
#include "cc/tiles/image_decode_cache.h"
#include "third_party/skia/include/core/SkPath.h"

namespace cc {
namespace {

SkIRect RoundOutRect(const SkRect& rect) {
  SkIRect result;
  rect.roundOut(&result);
  return result;
}

class ScopedDecodedImageLock {
 public:
  ScopedDecodedImageLock(ImageDecodeCache* image_decode_cache,
                         sk_sp<const SkImage> image,
                         const SkRect& src_rect,
                         const SkMatrix& matrix,
                         const SkPaint* paint,
                         const gfx::ColorSpace& target_color_space)
      : image_decode_cache_(image_decode_cache),
        draw_image_(std::move(image),
                    RoundOutRect(src_rect),
                    paint ? paint->getFilterQuality() : kNone_SkFilterQuality,
                    matrix,
                    target_color_space),
        decoded_draw_image_(
            image_decode_cache_->GetDecodedImageForDraw(draw_image_)) {
    DCHECK(draw_image_.image()->isLazyGenerated());
    if (paint) {
      decoded_paint_ = *paint;
      decoded_paint_->setFilterQuality(decoded_draw_image_.filter_quality());
    }
  }

  ScopedDecodedImageLock(ScopedDecodedImageLock&& from)
      : image_decode_cache_(from.image_decode_cache_),
        draw_image_(std::move(from.draw_image_)),
        decoded_draw_image_(std::move(from.decoded_draw_image_)),
        decoded_paint_(std::move(from.decoded_paint_)) {
    from.image_decode_cache_ = nullptr;
  }

  ~ScopedDecodedImageLock() {
    if (image_decode_cache_)
      image_decode_cache_->DrawWithImageFinished(draw_image_,
                                                 decoded_draw_image_);
  }

  const DecodedDrawImage& decoded_image() const { return decoded_draw_image_; }
  const SkPaint* decoded_paint() const {
    return decoded_paint_ ? &decoded_paint_.value() : nullptr;
  }

 private:
  ImageDecodeCache* image_decode_cache_;
  DrawImage draw_image_;
  DecodedDrawImage decoded_draw_image_;
  base::Optional<SkPaint> decoded_paint_;
};

// Encapsulates a ScopedDecodedImageLock and an SkPaint. Use of this class
// ensures that the ScopedDecodedImageLock outlives the dependent SkPaint.
class ScopedImagePaint {
 public:
  // Tries to create a ScopedImagePaint for the provided SkPaint. If a
  // the SkPaint does not contain an image that we support replacing,
  // an empty base::Optional will be returned.
  static base::Optional<ScopedImagePaint> TryCreate(
      ImageDecodeCache* image_decode_cache,
      const SkMatrix& ctm,
      const SkPaint& paint,
      const gfx::ColorSpace& target_color_space) {
    SkShader* shader = paint.getShader();
    if (!shader)
      return base::Optional<ScopedImagePaint>();

    SkMatrix matrix;
    SkShader::TileMode xy[2];
    SkImage* image = shader->isAImage(&matrix, xy);
    if (!image || !image->isLazyGenerated())
      return base::Optional<ScopedImagePaint>();

    SkMatrix total_image_matrix = matrix;
    total_image_matrix.preConcat(ctm);

    ScopedDecodedImageLock scoped_lock(
        image_decode_cache, sk_ref_sp(image),
        SkRect::MakeIWH(image->width(), image->height()), total_image_matrix,
        &paint, target_color_space);
    const DecodedDrawImage& decoded_image = scoped_lock.decoded_image();
    if (!decoded_image.image())
      return base::Optional<ScopedImagePaint>();

    bool need_scale = !decoded_image.is_scale_adjustment_identity();
    if (need_scale) {
      matrix.preScale(1.f / decoded_image.scale_adjustment().width(),
                      1.f / decoded_image.scale_adjustment().height());
    }
    SkPaint scratch_paint = paint;
    scratch_paint.setShader(
        decoded_image.image()->makeShader(xy[0], xy[1], &matrix));
    scratch_paint.setFilterQuality(decoded_image.filter_quality());
    return ScopedImagePaint(std::move(scoped_lock), std::move(scratch_paint));
  }

  const SkPaint& paint() { return paint_; }

 private:
  ScopedImagePaint(ScopedDecodedImageLock lock, SkPaint paint)
      : lock_(std::move(lock)), paint_(std::move(paint)) {}

  ScopedDecodedImageLock lock_;
  SkPaint paint_;
};

const SkImage* GetImageInPaint(const SkPaint& paint) {
  SkShader* shader = paint.getShader();
  return shader ? shader->isAImage(nullptr, nullptr) : nullptr;
}

}  // namespace

ImageHijackCanvas::ImageHijackCanvas(int width,
                                     int height,
                                     ImageDecodeCache* image_decode_cache,
                                     const ImageIdFlatSet* images_to_skip,
                                     const gfx::ColorSpace& target_color_space)
    : SkNWayCanvas(width, height),
      image_decode_cache_(image_decode_cache),
      images_to_skip_(images_to_skip),
      target_color_space_(target_color_space) {}

void ImageHijackCanvas::onDrawPicture(const SkPicture* picture,
                                      const SkMatrix* matrix,
                                      const SkPaint* paint) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "ImageHijackCanvas::onDrawPicture");
  // Ensure that pictures are unpacked by this canvas, instead of being
  // forwarded to the raster canvas.
  SkCanvas::onDrawPicture(picture, matrix, paint);
}

void ImageHijackCanvas::onDrawImage(const SkImage* image,
                                    SkScalar x,
                                    SkScalar y,
                                    const SkPaint* paint) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "ImageHijackCanvas::onDrawImage");
  SkRect rect = SkRect::MakeXYWH(x, y, SkIntToScalar(image->width()),
                                 SkIntToScalar(image->height()));
  if (QuickRejectDraw(rect, paint))
    return;

  if (!image->isLazyGenerated()) {
    DCHECK(!ShouldSkipImage(image));
    SkNWayCanvas::onDrawImage(image, x, y, paint);
    return;
  }

  if (ShouldSkipImage(image))
    return;

  SkMatrix ctm = getTotalMatrix();

  ScopedDecodedImageLock scoped_lock(
      image_decode_cache_, sk_ref_sp(image),
      SkRect::MakeIWH(image->width(), image->height()), ctm, paint,
      target_color_space_);
  const DecodedDrawImage& decoded_image = scoped_lock.decoded_image();
  if (!decoded_image.image())
    return;

  DCHECK_EQ(0, static_cast<int>(decoded_image.src_rect_offset().width()));
  DCHECK_EQ(0, static_cast<int>(decoded_image.src_rect_offset().height()));
  const SkPaint* decoded_paint = scoped_lock.decoded_paint();

  bool need_scale = !decoded_image.is_scale_adjustment_identity();
  if (need_scale) {
    SkNWayCanvas::save();
    SkNWayCanvas::scale(1.f / (decoded_image.scale_adjustment().width()),
                        1.f / (decoded_image.scale_adjustment().height()));
  }
  SkNWayCanvas::onDrawImage(decoded_image.image().get(), x, y, decoded_paint);
  if (need_scale)
    SkNWayCanvas::restore();
}

void ImageHijackCanvas::onDrawImageRect(const SkImage* image,
                                        const SkRect* src,
                                        const SkRect& dst,
                                        const SkPaint* paint,
                                        SrcRectConstraint constraint) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "ImageHijackCanvas::onDrawImageRect");
  if (QuickRejectDraw(dst, paint))
    return;

  if (!image->isLazyGenerated()) {
    DCHECK(!ShouldSkipImage(image));
    SkNWayCanvas::onDrawImageRect(image, src, dst, paint, constraint);
    return;
  }

  if (ShouldSkipImage(image))
    return;

  SkRect src_storage;
  if (!src) {
    src_storage = SkRect::MakeIWH(image->width(), image->height());
    src = &src_storage;
  }
  SkMatrix matrix;
  matrix.setRectToRect(*src, dst, SkMatrix::kFill_ScaleToFit);
  matrix.postConcat(getTotalMatrix());

  ScopedDecodedImageLock scoped_lock(image_decode_cache_, sk_ref_sp(image),
                                     *src, matrix, paint, target_color_space_);
  const DecodedDrawImage& decoded_image = scoped_lock.decoded_image();
  if (!decoded_image.image())
    return;

  const SkPaint* decoded_paint = scoped_lock.decoded_paint();

  SkRect adjusted_src =
      src->makeOffset(decoded_image.src_rect_offset().width(),
                      decoded_image.src_rect_offset().height());
  if (!decoded_image.is_scale_adjustment_identity()) {
    float x_scale = decoded_image.scale_adjustment().width();
    float y_scale = decoded_image.scale_adjustment().height();
    adjusted_src = SkRect::MakeXYWH(
        adjusted_src.x() * x_scale, adjusted_src.y() * y_scale,
        adjusted_src.width() * x_scale, adjusted_src.height() * y_scale);
  }
  SkNWayCanvas::onDrawImageRect(decoded_image.image().get(), &adjusted_src, dst,
                                decoded_paint, constraint);
}

void ImageHijackCanvas::onDrawRect(const SkRect& r, const SkPaint& paint) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "ImageHijackCanvas::onDrawRect");
  if (QuickRejectDraw(r, &paint))
    return;

  if (ShouldSkipImageInPaint(paint))
    return;

  base::Optional<ScopedImagePaint> image_paint = ScopedImagePaint::TryCreate(
      image_decode_cache_, getTotalMatrix(), paint, target_color_space_);
  if (!image_paint.has_value()) {
    SkNWayCanvas::onDrawRect(r, paint);
    return;
  }
  SkNWayCanvas::onDrawRect(r, image_paint.value().paint());
}

void ImageHijackCanvas::onDrawPath(const SkPath& path, const SkPaint& paint) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "ImageHijackCanvas::onDrawPath");
  if (QuickRejectDraw(path.getBounds(), &paint))
    return;

  if (ShouldSkipImageInPaint(paint))
    return;

  base::Optional<ScopedImagePaint> image_paint = ScopedImagePaint::TryCreate(
      image_decode_cache_, getTotalMatrix(), paint, target_color_space_);
  if (!image_paint.has_value()) {
    SkNWayCanvas::onDrawPath(path, paint);
    return;
  }
  SkNWayCanvas::onDrawPath(path, image_paint.value().paint());
}

void ImageHijackCanvas::onDrawOval(const SkRect& r, const SkPaint& paint) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "ImageHijackCanvas::onDrawOval");
  if (QuickRejectDraw(r, &paint))
    return;

  if (ShouldSkipImageInPaint(paint))
    return;

  base::Optional<ScopedImagePaint> image_paint = ScopedImagePaint::TryCreate(
      image_decode_cache_, getTotalMatrix(), paint, target_color_space_);
  if (!image_paint.has_value()) {
    SkNWayCanvas::onDrawOval(r, paint);
    return;
  }
  SkNWayCanvas::onDrawOval(r, image_paint.value().paint());
}

void ImageHijackCanvas::onDrawArc(const SkRect& r,
                                  SkScalar start_angle,
                                  SkScalar sweep_angle,
                                  bool use_center,
                                  const SkPaint& paint) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "ImageHijackCanvas::onDrawArc");
  if (QuickRejectDraw(r, &paint))
    return;

  if (ShouldSkipImageInPaint(paint))
    return;

  base::Optional<ScopedImagePaint> image_paint = ScopedImagePaint::TryCreate(
      image_decode_cache_, getTotalMatrix(), paint, target_color_space_);
  if (!image_paint.has_value()) {
    SkNWayCanvas::onDrawArc(r, start_angle, sweep_angle, use_center, paint);
    return;
  }
  SkNWayCanvas::onDrawArc(r, start_angle, sweep_angle, use_center,
                          image_paint.value().paint());
}

void ImageHijackCanvas::onDrawRRect(const SkRRect& rr, const SkPaint& paint) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "ImageHijackCanvas::onDrawRRect");
  if (QuickRejectDraw(rr.rect(), &paint))
    return;

  if (ShouldSkipImageInPaint(paint))
    return;

  base::Optional<ScopedImagePaint> image_paint = ScopedImagePaint::TryCreate(
      image_decode_cache_, getTotalMatrix(), paint, target_color_space_);
  if (!image_paint.has_value()) {
    SkNWayCanvas::onDrawRRect(rr, paint);
    return;
  }
  SkNWayCanvas::onDrawRRect(rr, image_paint.value().paint());
}

void ImageHijackCanvas::onDrawImageNine(const SkImage* image,
                                        const SkIRect& center,
                                        const SkRect& dst,
                                        const SkPaint* paint) {
  // No cc embedder issues image nine calls.
  NOTREACHED();
}

bool ImageHijackCanvas::ShouldSkipImage(const SkImage* image) const {
  bool skip =
      images_to_skip_->find(image->uniqueID()) != images_to_skip_->end();
  TRACE_EVENT2(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "ImageHijackCanvas::ShouldSkipImage", "imageId",
               image->uniqueID(), "skip", skip);
  return skip;
}

bool ImageHijackCanvas::ShouldSkipImageInPaint(const SkPaint& paint) const {
  const SkImage* image = GetImageInPaint(paint);
  return image ? ShouldSkipImage(image) : false;
}

bool ImageHijackCanvas::QuickRejectDraw(const SkRect& rect,
                                        const SkPaint* paint) const {
  if (nullptr == paint || paint->canComputeFastBounds()) {
    SkRect tmp = rect;
    if (paint)
      paint->computeFastBounds(tmp, &tmp);
    return quickReject(tmp);
  }

  return false;
}

}  // namespace cc
