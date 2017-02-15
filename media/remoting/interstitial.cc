// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/remoting/interstitial.h"

#include <algorithm>  // for std::max()

#include "media/base/localized_strings.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "third_party/skia/include/effects/SkBlurImageFilter.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/skbitmap_operations.h"
#include "ui/vector_icons/vector_icons.h"

namespace media {
namespace remoting {

namespace {

// The interstitial frame size when |background_image| is empty or has low
// resolution. The frame height may be adjusted according to the aspect ratio of
// the |background_image|.
constexpr int kDefaultFrameWidth = 1280;
constexpr int kDefaultFrameHeight = 720;

SkBitmap ResizeImage(const SkBitmap& image, const gfx::Size& scaled_size) {
  DCHECK(!scaled_size.IsEmpty());

  if (image.width() == scaled_size.width() &&
      image.height() == scaled_size.height())
    return image;

  return skia::ImageOperations::Resize(
      image, skia::ImageOperations::RESIZE_BEST, scaled_size.width(),
      scaled_size.height());
}

void RenderCastMessage(const gfx::Size& canvas_size,
                       InterstitialType type,
                       SkCanvas* canvas) {
  DCHECK(canvas);
  if (type == InterstitialType::BETWEEN_SESSIONS)
    return;

  // Blur the background image.
  constexpr SkScalar kSigma = SkIntToScalar(10);
  SkPaint paint_blur;
  paint_blur.setImageFilter(SkBlurImageFilter::Make(kSigma, kSigma, nullptr));
  canvas->saveLayer(0, &paint_blur);
  canvas->restore();

  // Create SkPaint for text and icon bitmap.
  // After |paint| draws, the new canvas should look like this:
  //       _________________________________
  //       |                                |
  //       |                                |
  //       |             [   ]              |
  //       |        Casting Video...        |
  //       |                                |
  //       |________________________________|
  //
  // Both text and icon are centered horizontally. Together, they are
  // centered vertically.
  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setFilterQuality(kHigh_SkFilterQuality);
  paint.setColor(SK_ColorLTGRAY);
  paint.setTypeface(SkTypeface::MakeFromName(
      "sans", SkFontStyle::FromOldStyle(SkTypeface::kNormal)));
  const SkScalar text_height = SkIntToScalar(canvas_size.height() / 18);
  paint.setTextSize(text_height);

  // Draw the appropriate text.
  const std::string message =
      (type == InterstitialType::IN_SESSION
           ? GetLocalizedStringUTF8(MEDIA_REMOTING_CASTING_VIDEO_TEXT)
           : GetLocalizedStringUTF8(MEDIA_REMOTING_CAST_ERROR_TEXT));
  SkScalar display_text_width =
      paint.measureText(message.data(), message.size());
  SkScalar sk_text_offset_x =
      SkScalarFloorToScalar((canvas_size.width() - display_text_width) / 2.0);
  SkScalar sk_text_offset_y =
      SkScalarFloorToScalar((canvas_size.height() / 2.0) + text_height);
  canvas->drawText(message.data(), message.size(), sk_text_offset_x,
                   sk_text_offset_y, paint);

  // Draw the appropriate Cast icon.
  const gfx::VectorIcon& current_icon = type == InterstitialType::IN_SESSION
                                            ? ui::kMediaRouterActiveIcon
                                            : ui::kMediaRouterWarningIcon;
  gfx::ImageSkia icon_image = gfx::CreateVectorIcon(
      current_icon, canvas_size.height() / 6, SK_ColorLTGRAY);
  const SkBitmap* icon_bitmap = icon_image.bitmap();
  SkScalar sk_image_offset_x = (canvas_size.width() - icon_image.width()) / 2.0;
  SkScalar sk_image_offset_y =
      (canvas_size.height() / 2.0) - icon_image.height();
  canvas->drawBitmap(*icon_bitmap, sk_image_offset_x, sk_image_offset_y,
                     &paint);
}

gfx::Size GetCanvasSize(const gfx::Size& image_size,
                        const gfx::Size& natural_size) {
  if (natural_size.IsEmpty())
    return gfx::Size(kDefaultFrameWidth, kDefaultFrameHeight);
  int width = std::max(image_size.width(), kDefaultFrameWidth) & ~1;
  base::CheckedNumeric<int> height = width;
  height *= natural_size.height();
  height /= natural_size.width();
  height &= ~1;
  gfx::Size result = gfx::Size(width, height.ValueOrDefault(0));
  return result.IsEmpty() ? gfx::Size(kDefaultFrameWidth, kDefaultFrameHeight)
                          : result;
}

}  // namespace

scoped_refptr<VideoFrame> RenderInterstitialFrame(const SkBitmap& image,
                                                  const gfx::Size& natural_size,
                                                  InterstitialType type) {
  gfx::Size canvas_size =
      GetCanvasSize(gfx::Size(image.width(), image.height()), natural_size);
  SkBitmap canvas_bitmap;
  canvas_bitmap.allocN32Pixels(canvas_size.width(), canvas_size.height());
  canvas_bitmap.eraseColor(SK_ColorBLACK);
  SkCanvas canvas(canvas_bitmap);

  // Draw background image on the canvas.
  if (!image.drawsNothing()) {
    gfx::Rect centered_rect = ComputeLetterboxRegion(
        gfx::Rect(canvas_size), gfx::Size(image.width(), image.height()));
    SkBitmap processed_image = ResizeImage(image, centered_rect.size());
    if (type != InterstitialType::BETWEEN_SESSIONS) {
      color_utils::HSL shift = {-1, 0, 0.2};  // Make monochromatic.
      processed_image =
          SkBitmapOperations::CreateHSLShiftedBitmap(processed_image, shift);
    }
    canvas.writePixels(processed_image, centered_rect.x(), centered_rect.y());
  }

  RenderCastMessage(canvas_size, type, &canvas);

  // Create a new VideoFrame, copy the bitmap, then return it.
  scoped_refptr<VideoFrame> video_frame = VideoFrame::CreateFrame(
      PIXEL_FORMAT_I420, canvas_size, gfx::Rect(canvas_size), canvas_size,
      base::TimeDelta());
  if (video_frame) {
    SkAutoLockPixels pixel_lock(canvas_bitmap);
    CopyRGBToVideoFrame(reinterpret_cast<uint8_t*>(canvas_bitmap.getPixels()),
                        canvas_bitmap.rowBytes(),
                        gfx::Rect(canvas_size.width(), canvas_size.height()),
                        video_frame.get());
  }
  return video_frame;
}

}  // namespace remoting
}  // namespace media
