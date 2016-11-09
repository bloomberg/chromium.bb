// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/remoting/remoting_interstitial_ui.h"

#include "media/base/media_resources.h"
#include "media/base/video_util.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "third_party/skia/include/effects/SkBlurImageFilter.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/skbitmap_operations.h"
#include "ui/gfx/vector_icons_public.h"

namespace media {

scoped_refptr<VideoFrame> GetInterstitial(SkCanvas* existing_frame_canvas,
                                          bool is_remoting_successful) {
  if (!existing_frame_canvas)
    return nullptr;

  // Initial bitmap to grab the existing frame image.
  SkBitmap bitmap;
  bitmap.setInfo(existing_frame_canvas->imageInfo());
  const SkISize frame_size = existing_frame_canvas->getBaseLayerSize();
  const gfx::Size canvas_size =
      gfx::Size(frame_size.width(), frame_size.height());

  // Second bitmap for manipulation.
  SkBitmap modified_bitmap;

  // Reads the pixels from the current frame into |modified_bitmap|. In the
  // case this failed, create and use a blank bitmap.
  if (existing_frame_canvas->readPixels(&bitmap, 0, 0)) {
    // Make monochromatic.
    color_utils::HSL shift = {-1, 0, 0.2};
    modified_bitmap = SkBitmapOperations::CreateHSLShiftedBitmap(bitmap, shift);
  } else {
    modified_bitmap.allocN32Pixels(canvas_size.width(), canvas_size.height());
  }

  SkCanvas canvas(modified_bitmap);

  // Blur the background image.
  SkScalar sigma = SkDoubleToScalar(10);
  SkPaint paint_blur;
  paint_blur.setImageFilter(SkBlurImageFilter::Make(sigma, sigma, nullptr));
  canvas.saveLayer(0, &paint_blur);
  canvas.restore();

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
  int text_size = SkIntToScalar(30);
  paint.setAntiAlias(true);
  paint.setFilterQuality(kHigh_SkFilterQuality);
  paint.setColor(SK_ColorLTGRAY);
  paint.setTypeface(SkTypeface::MakeFromName(
      "sans", SkFontStyle::FromOldStyle(SkTypeface::kNormal)));
  paint.setTextSize(text_size);

  // Draw the appropriate text.
  const std::string remote_playback_message =
      is_remoting_successful
          ? GetLocalizedStringUTF8(MEDIA_REMOTING_CASTING_VIDEO_TEXT)
          : GetLocalizedStringUTF8(MEDIA_REMOTING_CAST_ERROR_TEXT);
  size_t display_text_width = paint.measureText(remote_playback_message.data(),
                                                remote_playback_message.size());
  SkScalar sk_text_offset_x = (canvas_size.width() - display_text_width) / 2.0;
  SkScalar sk_text_offset_y = (canvas_size.height() / 2.0) + text_size;
  canvas.drawText(remote_playback_message.data(),
                  remote_playback_message.size(), sk_text_offset_x,
                  sk_text_offset_y, paint);

  // Draw the appropriate Cast icon.
  gfx::VectorIconId current_icon = is_remoting_successful
                                       ? gfx::VectorIconId::MEDIA_ROUTER_ACTIVE
                                       : gfx::VectorIconId::MEDIA_ROUTER_ERROR;
  gfx::ImageSkia icon_image =
      gfx::CreateVectorIcon(current_icon, 65, SK_ColorLTGRAY);
  const SkBitmap* icon_bitmap = icon_image.bitmap();
  SkScalar sk_image_offset_x = (canvas_size.width() - icon_image.width()) / 2.0;
  SkScalar sk_image_offset_y =
      (canvas_size.height() / 2.0) - icon_image.height();
  canvas.drawBitmap(*icon_bitmap, sk_image_offset_x, sk_image_offset_y, &paint);

  // Create a new VideoFrame, copy the bitmap, then return it.
  scoped_refptr<media::VideoFrame> video_frame = media::VideoFrame::CreateFrame(
      media::PIXEL_FORMAT_I420, canvas_size, gfx::Rect(canvas_size),
      canvas_size, base::TimeDelta());
  modified_bitmap.lockPixels();
  media::CopyRGBToVideoFrame(
      reinterpret_cast<uint8_t*>(modified_bitmap.getPixels()),
      modified_bitmap.rowBytes(),
      gfx::Rect(canvas_size.width(), canvas_size.height()), video_frame.get());
  modified_bitmap.unlockPixels();
  return video_frame;
}

}  // namespace media
