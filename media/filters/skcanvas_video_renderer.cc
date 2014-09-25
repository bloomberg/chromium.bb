// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/skcanvas_video_renderer.h"

#include "base/logging.h"
#include "media/base/video_frame.h"
#include "media/base/yuv_convert.h"
#include "third_party/libyuv/include/libyuv.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImageGenerator.h"
#include "ui/gfx/skbitmap_operations.h"

// Skia internal format depends on a platform. On Android it is ABGR, on others
// it is ARGB.
#if SK_B32_SHIFT == 0 && SK_G32_SHIFT == 8 && SK_R32_SHIFT == 16 && \
    SK_A32_SHIFT == 24
#define LIBYUV_I420_TO_ARGB libyuv::I420ToARGB
#define LIBYUV_I422_TO_ARGB libyuv::I422ToARGB
#elif SK_R32_SHIFT == 0 && SK_G32_SHIFT == 8 && SK_B32_SHIFT == 16 && \
    SK_A32_SHIFT == 24
#define LIBYUV_I420_TO_ARGB libyuv::I420ToABGR
#define LIBYUV_I422_TO_ARGB libyuv::I422ToABGR
#else
#error Unexpected Skia ARGB_8888 layout!
#endif

namespace media {

static bool IsYUV(media::VideoFrame::Format format) {
  switch (format) {
    case VideoFrame::YV12:
    case VideoFrame::YV16:
    case VideoFrame::I420:
    case VideoFrame::YV12A:
    case VideoFrame::YV12J:
    case VideoFrame::YV24:
    case VideoFrame::NV12:
      return true;
    case VideoFrame::UNKNOWN:
    case VideoFrame::NATIVE_TEXTURE:
#if defined(VIDEO_HOLE)
    case VideoFrame::HOLE:
#endif  // defined(VIDEO_HOLE)
      return false;
  }
  NOTREACHED() << "Invalid videoframe format provided: " << format;
  return false;
}

static bool IsJPEGColorSpace(media::VideoFrame::Format format) {
  switch (format) {
    case VideoFrame::YV12J:
      return true;
    case VideoFrame::YV12:
    case VideoFrame::YV16:
    case VideoFrame::I420:
    case VideoFrame::YV12A:
    case VideoFrame::YV24:
    case VideoFrame::NV12:
    case VideoFrame::UNKNOWN:
    case VideoFrame::NATIVE_TEXTURE:
#if defined(VIDEO_HOLE)
    case VideoFrame::HOLE:
#endif  // defined(VIDEO_HOLE)
      return false;
  }
  NOTREACHED() << "Invalid videoframe format provided: " << format;
  return false;
}

static bool IsYUVOrNative(media::VideoFrame::Format format) {
  return IsYUV(format) || format == media::VideoFrame::NATIVE_TEXTURE;
}

// Converts a |video_frame| to raw |rgb_pixels|.
static void ConvertVideoFrameToRGBPixels(
    const scoped_refptr<media::VideoFrame>& video_frame,
    void* rgb_pixels,
    size_t row_bytes) {
  DCHECK(IsYUVOrNative(video_frame->format()))
      << video_frame->format();
  if (IsYUV(video_frame->format())) {
    DCHECK_EQ(video_frame->stride(media::VideoFrame::kUPlane),
              video_frame->stride(media::VideoFrame::kVPlane));
  }

  size_t y_offset = 0;
  size_t uv_offset = 0;
  if (IsYUV(video_frame->format())) {
    int y_shift = (video_frame->format() == media::VideoFrame::YV16) ? 0 : 1;
    // Use the "left" and "top" of the destination rect to locate the offset
    // in Y, U and V planes.
    y_offset = (video_frame->stride(media::VideoFrame::kYPlane) *
                video_frame->visible_rect().y()) +
                video_frame->visible_rect().x();
    // For format YV12, there is one U, V value per 2x2 block.
    // For format YV16, there is one U, V value per 2x1 block.
    uv_offset = (video_frame->stride(media::VideoFrame::kUPlane) *
                (video_frame->visible_rect().y() >> y_shift)) +
                (video_frame->visible_rect().x() >> 1);
  }

  switch (video_frame->format()) {
    case media::VideoFrame::YV12:
    case media::VideoFrame::I420:
      LIBYUV_I420_TO_ARGB(
          video_frame->data(media::VideoFrame::kYPlane) + y_offset,
          video_frame->stride(media::VideoFrame::kYPlane),
          video_frame->data(media::VideoFrame::kUPlane) + uv_offset,
          video_frame->stride(media::VideoFrame::kUPlane),
          video_frame->data(media::VideoFrame::kVPlane) + uv_offset,
          video_frame->stride(media::VideoFrame::kVPlane),
          static_cast<uint8*>(rgb_pixels),
          row_bytes,
          video_frame->visible_rect().width(),
          video_frame->visible_rect().height());
      break;

    case media::VideoFrame::YV12J:
      media::ConvertYUVToRGB32(
          video_frame->data(media::VideoFrame::kYPlane) + y_offset,
          video_frame->data(media::VideoFrame::kUPlane) + uv_offset,
          video_frame->data(media::VideoFrame::kVPlane) + uv_offset,
          static_cast<uint8*>(rgb_pixels),
          video_frame->visible_rect().width(),
          video_frame->visible_rect().height(),
          video_frame->stride(media::VideoFrame::kYPlane),
          video_frame->stride(media::VideoFrame::kUPlane),
          row_bytes,
          media::YV12J);
      break;

    case media::VideoFrame::YV16:
      LIBYUV_I422_TO_ARGB(
          video_frame->data(media::VideoFrame::kYPlane) + y_offset,
          video_frame->stride(media::VideoFrame::kYPlane),
          video_frame->data(media::VideoFrame::kUPlane) + uv_offset,
          video_frame->stride(media::VideoFrame::kUPlane),
          video_frame->data(media::VideoFrame::kVPlane) + uv_offset,
          video_frame->stride(media::VideoFrame::kVPlane),
          static_cast<uint8*>(rgb_pixels),
          row_bytes,
          video_frame->visible_rect().width(),
          video_frame->visible_rect().height());
      break;

    case media::VideoFrame::YV12A:
      // Since libyuv doesn't support YUVA, fallback to media, which is not ARM
      // optimized.
      // TODO(fbarchard, mtomasz): Use libyuv, then copy the alpha channel.
      media::ConvertYUVAToARGB(
          video_frame->data(media::VideoFrame::kYPlane) + y_offset,
          video_frame->data(media::VideoFrame::kUPlane) + uv_offset,
          video_frame->data(media::VideoFrame::kVPlane) + uv_offset,
          video_frame->data(media::VideoFrame::kAPlane),
          static_cast<uint8*>(rgb_pixels),
          video_frame->visible_rect().width(),
          video_frame->visible_rect().height(),
          video_frame->stride(media::VideoFrame::kYPlane),
          video_frame->stride(media::VideoFrame::kUPlane),
          video_frame->stride(media::VideoFrame::kAPlane),
          row_bytes,
          media::YV12);
      break;

    case media::VideoFrame::YV24:
      libyuv::I444ToARGB(
          video_frame->data(media::VideoFrame::kYPlane) + y_offset,
          video_frame->stride(media::VideoFrame::kYPlane),
          video_frame->data(media::VideoFrame::kUPlane) + uv_offset,
          video_frame->stride(media::VideoFrame::kUPlane),
          video_frame->data(media::VideoFrame::kVPlane) + uv_offset,
          video_frame->stride(media::VideoFrame::kVPlane),
          static_cast<uint8*>(rgb_pixels),
          row_bytes,
          video_frame->visible_rect().width(),
          video_frame->visible_rect().height());
#if SK_R32_SHIFT == 0 && SK_G32_SHIFT == 8 && SK_B32_SHIFT == 16 && \
    SK_A32_SHIFT == 24
      libyuv::ARGBToABGR(static_cast<uint8*>(rgb_pixels),
                         row_bytes,
                         static_cast<uint8*>(rgb_pixels),
                         row_bytes,
                         video_frame->visible_rect().width(),
                         video_frame->visible_rect().height());
#endif
      break;

    case media::VideoFrame::NATIVE_TEXTURE: {
      DCHECK_EQ(video_frame->format(), media::VideoFrame::NATIVE_TEXTURE);
      SkBitmap tmp;
      tmp.installPixels(
          SkImageInfo::MakeN32Premul(video_frame->visible_rect().width(),
                                     video_frame->visible_rect().height()),
          rgb_pixels,
          row_bytes);
      video_frame->ReadPixelsFromNativeTexture(tmp);
      break;
    }
    default:
      NOTREACHED();
      break;
  }
}

// Generates an RGB image from a VideoFrame.
class VideoImageGenerator : public SkImageGenerator {
 public:
  VideoImageGenerator(const scoped_refptr<VideoFrame>& frame) : frame_(frame) {}
  virtual ~VideoImageGenerator() {}

  void set_frame(const scoped_refptr<VideoFrame>& frame) { frame_ = frame; }

 protected:
  virtual bool onGetInfo(SkImageInfo* info) OVERRIDE {
    info->fWidth = frame_->visible_rect().width();
    info->fHeight = frame_->visible_rect().height();
    info->fColorType = kN32_SkColorType;
    info->fAlphaType = kPremul_SkAlphaType;
    return true;
  }

  virtual bool onGetPixels(const SkImageInfo& info,
                           void* pixels,
                           size_t row_bytes,
                           SkPMColor ctable[],
                           int* ctable_count) OVERRIDE {
    if (!frame_.get())
      return false;
    if (!pixels)
      return true;
    // If skia couldn't do the YUV conversion, we will.
    ConvertVideoFrameToRGBPixels(frame_, pixels, row_bytes);
    frame_ = NULL;
    return true;
  }

  virtual bool onGetYUV8Planes(SkISize sizes[3],
                               void* planes[3],
                               size_t row_bytes[3],
                               SkYUVColorSpace* color_space) OVERRIDE {
    if (!frame_.get() || !IsYUV(frame_->format()))
      return false;

    if (color_space) {
      if (IsJPEGColorSpace(frame_->format()))
        *color_space = kJPEG_SkYUVColorSpace;
      else
        *color_space = kRec601_SkYUVColorSpace;
    }

    for (int plane = VideoFrame::kYPlane; plane <= VideoFrame::kVPlane;
         ++plane) {
      if (sizes) {
        gfx::Size size;
        size =
            VideoFrame::PlaneSize(frame_->format(),
                                  plane,
                                  gfx::Size(frame_->visible_rect().width(),
                                            frame_->visible_rect().height()));
        sizes[plane].set(size.width(), size.height());
      }
      if (row_bytes && planes) {
        size_t offset;
        int y_shift = (frame_->format() == media::VideoFrame::YV16) ? 0 : 1;
        if (plane == media::VideoFrame::kYPlane) {
          offset = (frame_->stride(media::VideoFrame::kYPlane) *
                    frame_->visible_rect().y()) +
                   frame_->visible_rect().x();
        } else {
          offset = (frame_->stride(media::VideoFrame::kUPlane) *
                    (frame_->visible_rect().y() >> y_shift)) +
                   (frame_->visible_rect().x() >> 1);
        }
        row_bytes[plane] = static_cast<size_t>(frame_->stride(plane));
        planes[plane] = frame_->data(plane) + offset;
      }
    }
    if (planes && row_bytes)
      frame_ = NULL;
    return true;
  }

 private:
  scoped_refptr<VideoFrame> frame_;
};

SkCanvasVideoRenderer::SkCanvasVideoRenderer()
    : generator_(NULL), last_frame_timestamp_(media::kNoTimestamp()) {
  last_frame_.setIsVolatile(true);
}

SkCanvasVideoRenderer::~SkCanvasVideoRenderer() {}

void SkCanvasVideoRenderer::Paint(const scoped_refptr<VideoFrame>& video_frame,
                                  SkCanvas* canvas,
                                  const gfx::RectF& dest_rect,
                                  uint8 alpha,
                                  SkXfermode::Mode mode,
                                  VideoRotation video_rotation) {
  if (alpha == 0) {
    return;
  }

  SkRect dest;
  dest.set(dest_rect.x(), dest_rect.y(), dest_rect.right(), dest_rect.bottom());

  SkPaint paint;
  paint.setAlpha(alpha);

  // Paint black rectangle if there isn't a frame available or the
  // frame has an unexpected format.
  if (!video_frame.get() || !IsYUVOrNative(video_frame->format())) {
    canvas->drawRect(dest, paint);
    canvas->flush();
    return;
  }

  // Check if we should convert and update |last_frame_|.
  if (last_frame_.isNull() ||
      video_frame->timestamp() != last_frame_timestamp_) {
    generator_ = new VideoImageGenerator(video_frame);

    // Note: This takes ownership of |generator_|.
    if (!SkInstallDiscardablePixelRef(generator_, &last_frame_)) {
      NOTREACHED();
    }

    // TODO(rileya): Perform this rotation on the canvas, rather than allocating
    // a new bitmap and copying.
    switch (video_rotation) {
      case VIDEO_ROTATION_0:
        break;
      case VIDEO_ROTATION_90:
        last_frame_ = SkBitmapOperations::Rotate(
            last_frame_, SkBitmapOperations::ROTATION_90_CW);
        break;
      case VIDEO_ROTATION_180:
        last_frame_ = SkBitmapOperations::Rotate(
            last_frame_, SkBitmapOperations::ROTATION_180_CW);
        break;
      case VIDEO_ROTATION_270:
        last_frame_ = SkBitmapOperations::Rotate(
            last_frame_, SkBitmapOperations::ROTATION_270_CW);
        break;
    }

    // We copied the frame into a new bitmap and threw out the old one, so we
    // no longer have a |generator_| around. This should be removed when the
    // above TODO is addressed.
    if (video_rotation != VIDEO_ROTATION_0)
      generator_ = NULL;

    last_frame_timestamp_ = video_frame->timestamp();
  } else if (generator_) {
    generator_->set_frame(video_frame);
  }

  paint.setXfermodeMode(mode);

  // Paint using |last_frame_|.
  paint.setFilterLevel(SkPaint::kLow_FilterLevel);
  canvas->drawBitmapRect(last_frame_, NULL, dest, &paint);
  canvas->flush();
}

void SkCanvasVideoRenderer::Copy(const scoped_refptr<VideoFrame>& video_frame,
                                 SkCanvas* canvas) {
  Paint(video_frame,
        canvas,
        video_frame->visible_rect(),
        0xff,
        SkXfermode::kSrc_Mode,
        media::VIDEO_ROTATION_0);
}

}  // namespace media
