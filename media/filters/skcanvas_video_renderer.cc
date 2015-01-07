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
#include "third_party/skia/include/gpu/GrContext.h"
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

namespace {

// This class keeps two temporary resources; software bitmap, hardware bitmap.
// If both bitmap are created and then only software bitmap is updated every
// frame, hardware bitmap outlives until the media player dies. So we delete
// a temporary resource if it is not used for 3 sec.
const int kTemporaryResourceDeletionDelay = 3;  // Seconds;

bool IsYUV(media::VideoFrame::Format format) {
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

bool IsJPEGColorSpace(media::VideoFrame::Format format) {
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

bool IsYUVOrNative(media::VideoFrame::Format format) {
  return IsYUV(format) || format == media::VideoFrame::NATIVE_TEXTURE;
}

// Converts a |video_frame| to raw |rgb_pixels|.
void ConvertVideoFrameToRGBPixels(
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

}  // anonymous namespace

// Generates an RGB image from a VideoFrame. Convert YUV to RGB plain on GPU.
class VideoImageGenerator : public SkImageGenerator {
 public:
  VideoImageGenerator(const scoped_refptr<VideoFrame>& frame) : frame_(frame) {
    DCHECK(frame_.get());
  }
  ~VideoImageGenerator() override {}

  void set_frame(const scoped_refptr<VideoFrame>& frame) { frame_ = frame; }

 protected:
  bool onGetInfo(SkImageInfo* info) override {
    info->fWidth = frame_->visible_rect().width();
    info->fHeight = frame_->visible_rect().height();
    info->fColorType = kN32_SkColorType;
    info->fAlphaType = kPremul_SkAlphaType;
    return true;
  }

  bool onGetPixels(const SkImageInfo& info,
                   void* pixels,
                   size_t row_bytes,
                   SkPMColor ctable[],
                   int* ctable_count) override {
    if (!frame_.get())
      return false;
    if (!pixels)
      return false;
    // If skia couldn't do the YUV conversion on GPU, we will on CPU.
    ConvertVideoFrameToRGBPixels(frame_, pixels, row_bytes);
    return true;
  }

  bool onGetYUV8Planes(SkISize sizes[3],
                       void* planes[3],
                       size_t row_bytes[3],
                       SkYUVColorSpace* color_space) override {
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
    return true;
  }

 private:
  scoped_refptr<VideoFrame> frame_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(VideoImageGenerator);
};

SkCanvasVideoRenderer::SkCanvasVideoRenderer()
    : last_frame_timestamp_(media::kNoTimestamp()),
      frame_deleting_timer_(
          FROM_HERE,
          base::TimeDelta::FromSeconds(kTemporaryResourceDeletionDelay),
          this,
          &SkCanvasVideoRenderer::ResetLastFrame),
      accelerated_generator_(NULL),
      accelerated_last_frame_timestamp_(media::kNoTimestamp()),
      accelerated_frame_deleting_timer_(
          FROM_HERE,
          base::TimeDelta::FromSeconds(kTemporaryResourceDeletionDelay),
          this,
          &SkCanvasVideoRenderer::ResetAcceleratedLastFrame) {
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
  if (!video_frame.get() || video_frame->natural_size().IsEmpty() ||
      !IsYUVOrNative(video_frame->format())) {
    canvas->drawRect(dest, paint);
    canvas->flush();
    return;
  }

  SkBitmap* target_frame = NULL;
  if (canvas->getGrContext()) {
    if (accelerated_last_frame_.isNull() ||
        video_frame->timestamp() != accelerated_last_frame_timestamp_) {
      accelerated_generator_ = new VideoImageGenerator(video_frame);

      // Note: This takes ownership of |accelerated_generator_|.
      if (!SkInstallDiscardablePixelRef(accelerated_generator_,
                                        &accelerated_last_frame_)) {
        NOTREACHED();
      }
      DCHECK(video_frame->visible_rect().width() ==
                 accelerated_last_frame_.width() &&
             video_frame->visible_rect().height() ==
                 accelerated_last_frame_.height());

      accelerated_last_frame_timestamp_ = video_frame->timestamp();
    } else {
      accelerated_generator_->set_frame(video_frame);
    }
    target_frame = &accelerated_last_frame_;
    accelerated_frame_deleting_timer_.Reset();
  } else {
    // Check if we should convert and update |last_frame_|.
    if (last_frame_.isNull() ||
        video_frame->timestamp() != last_frame_timestamp_) {
      // Check if |bitmap| needs to be (re)allocated.
      if (last_frame_.isNull() ||
          last_frame_.width() != video_frame->visible_rect().width() ||
          last_frame_.height() != video_frame->visible_rect().height()) {
        last_frame_.allocN32Pixels(video_frame->visible_rect().width(),
                                   video_frame->visible_rect().height());
        last_frame_.setIsVolatile(true);
      }
      last_frame_.lockPixels();
      ConvertVideoFrameToRGBPixels(
          video_frame, last_frame_.getPixels(), last_frame_.rowBytes());
      last_frame_.notifyPixelsChanged();
      last_frame_.unlockPixels();
      last_frame_timestamp_ = video_frame->timestamp();
    }
    target_frame = &last_frame_;
    frame_deleting_timer_.Reset();
  }

  paint.setXfermodeMode(mode);
  paint.setFilterLevel(SkPaint::kLow_FilterLevel);

  bool need_transform =
      video_rotation != VIDEO_ROTATION_0 ||
      dest_rect.size() != video_frame->visible_rect().size() ||
      !dest_rect.origin().IsOrigin();
  if (need_transform) {
    canvas->save();
    canvas->translate(
        SkFloatToScalar(dest_rect.x() + (dest_rect.width() * 0.5f)),
        SkFloatToScalar(dest_rect.y() + (dest_rect.height() * 0.5f)));
    SkScalar angle = SkFloatToScalar(0.0f);
    switch (video_rotation) {
      case VIDEO_ROTATION_0:
        break;
      case VIDEO_ROTATION_90:
        angle = SkFloatToScalar(90.0f);
        break;
      case VIDEO_ROTATION_180:
        angle = SkFloatToScalar(180.0f);
        break;
      case VIDEO_ROTATION_270:
        angle = SkFloatToScalar(270.0f);
        break;
    }
    canvas->rotate(angle);

    gfx::SizeF rotated_dest_size = dest_rect.size();
    if (video_rotation == VIDEO_ROTATION_90 ||
        video_rotation == VIDEO_ROTATION_270) {
      rotated_dest_size =
          gfx::SizeF(rotated_dest_size.height(), rotated_dest_size.width());
    }
    canvas->scale(
        SkFloatToScalar(rotated_dest_size.width() / target_frame->width()),
        SkFloatToScalar(rotated_dest_size.height() / target_frame->height()));
    canvas->translate(-SkFloatToScalar(target_frame->width() * 0.5f),
                      -SkFloatToScalar(target_frame->height() * 0.5f));
  }
  canvas->drawBitmap(*target_frame, 0, 0, &paint);
  if (need_transform)
    canvas->restore();
  canvas->flush();
  // SkCanvas::flush() causes the generator to generate SkImage, so delete
  // |video_frame| not to be outlived.
  if (canvas->getGrContext())
    accelerated_generator_->set_frame(NULL);
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

void SkCanvasVideoRenderer::ResetLastFrame() {
  last_frame_.reset();
  last_frame_timestamp_ = media::kNoTimestamp();
}

void SkCanvasVideoRenderer::ResetAcceleratedLastFrame() {
  accelerated_last_frame_.reset();
  accelerated_generator_ = nullptr;
  accelerated_last_frame_timestamp_ = media::kNoTimestamp();
}

}  // namespace media
