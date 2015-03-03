// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/blink/skcanvas_video_renderer.h"

#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "media/base/video_frame.h"
#include "media/base/yuv_convert.h"
#include "skia/ext/refptr.h"
#include "third_party/libyuv/include/libyuv.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImageGenerator.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "third_party/skia/include/gpu/SkGrPixelRef.h"
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
    case VideoFrame::YV12HD:
    case VideoFrame::YV24:
    case VideoFrame::NV12:
      return true;
    case VideoFrame::UNKNOWN:
    case VideoFrame::NATIVE_TEXTURE:
#if defined(VIDEO_HOLE)
    case VideoFrame::HOLE:
#endif  // defined(VIDEO_HOLE)
    case VideoFrame::ARGB:
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
    case VideoFrame::YV12HD:
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
    case VideoFrame::ARGB:
      return false;
  }
  NOTREACHED() << "Invalid videoframe format provided: " << format;
  return false;
}

bool IsYUVOrNative(media::VideoFrame::Format format) {
  return IsYUV(format) || format == media::VideoFrame::NATIVE_TEXTURE;
}

bool IsSkBitmapProperlySizedTexture(const SkBitmap* bitmap,
                                    const gfx::Size& size) {
  return bitmap->getTexture() && bitmap->width() == size.width() &&
         bitmap->height() == size.height();
}

bool AllocateSkBitmapTexture(GrContext* gr,
                             SkBitmap* bitmap,
                             const gfx::Size& size) {
  DCHECK(gr);
  GrTextureDesc desc;
  // Use kRGBA_8888_GrPixelConfig, not kSkia8888_GrPixelConfig, to avoid
  // RGBA to BGRA conversion.
  desc.fConfig = kRGBA_8888_GrPixelConfig;
  desc.fFlags = kRenderTarget_GrTextureFlagBit | kNoStencil_GrTextureFlagBit;
  desc.fSampleCnt = 0;
  desc.fOrigin = kTopLeft_GrSurfaceOrigin;
  desc.fWidth = size.width();
  desc.fHeight = size.height();
  skia::RefPtr<GrTexture> texture = skia::AdoptRef(
      gr->refScratchTexture(desc, GrContext::kExact_ScratchTexMatch));
  if (!texture.get())
    return false;

  SkImageInfo info = SkImageInfo::MakeN32Premul(desc.fWidth, desc.fHeight);
  SkGrPixelRef* pixel_ref = SkNEW_ARGS(SkGrPixelRef, (info, texture.get()));
  if (!pixel_ref)
    return false;
  bitmap->setInfo(info);
  bitmap->setPixelRef(pixel_ref)->unref();
  return true;
}

bool CopyVideoFrameTextureToSkBitmapTexture(VideoFrame* video_frame,
                                            SkBitmap* bitmap,
                                            const Context3D& context_3d) {
  // Check if we could reuse existing texture based bitmap.
  // Otherwise, release existing texture based bitmap and allocate
  // a new one based on video size.
  if (!IsSkBitmapProperlySizedTexture(bitmap,
                                      video_frame->visible_rect().size())) {
    if (!AllocateSkBitmapTexture(context_3d.gr_context, bitmap,
                                 video_frame->visible_rect().size())) {
      return false;
    }
  }

  unsigned texture_id =
      static_cast<unsigned>((bitmap->getTexture())->getTextureHandle());
  // If CopyVideoFrameTextureToGLTexture() changes the state of the
  // |texture_id|, it's needed to invalidate the state cached in skia,
  // but currently the state isn't changed.
  SkCanvasVideoRenderer::CopyVideoFrameTextureToGLTexture(
      context_3d.gl, video_frame, texture_id, 0, GL_RGBA, GL_UNSIGNED_BYTE,
      true, false);
  bitmap->notifyPixelsChanged();
  return true;
}

class SyncPointClientImpl : public VideoFrame::SyncPointClient {
 public:
  explicit SyncPointClientImpl(gpu::gles2::GLES2Interface* gl) : gl_(gl) {}
  ~SyncPointClientImpl() override {}
  uint32 InsertSyncPoint() override { return gl_->InsertSyncPointCHROMIUM(); }
  void WaitSyncPoint(uint32 sync_point) override {
    gl_->WaitSyncPointCHROMIUM(sync_point);
  }

 private:
  gpu::gles2::GLES2Interface* gl_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(SyncPointClientImpl);
};

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

  Result onGetPixels(const SkImageInfo& info,
                     void* pixels,
                     size_t row_bytes,
                     SkPMColor ctable[],
                     int* ctable_count) override {
    if (!frame_.get())
      return kInvalidInput;
    // If skia couldn't do the YUV conversion on GPU, we will on CPU.
    SkCanvasVideoRenderer::ConvertVideoFrameToRGBPixels(
        frame_, pixels, row_bytes);
    return kSuccess;
  }

  bool onGetYUV8Planes(SkISize sizes[3],
                       void* planes[3],
                       size_t row_bytes[3],
                       SkYUVColorSpace* color_space) override {
    if (!frame_.get() || !IsYUV(frame_->format()) ||
        // TODO(rileya): Skia currently doesn't support Rec709 YUV conversion,
        // or YUVA conversion. Remove this case once it does. As-is we will
        // fall back on the pure-software path in this case.
        frame_->format() == VideoFrame::YV12HD ||
        frame_->format() == VideoFrame::YV12A) {
      return false;
    }

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

        // Copy the frame to the supplied memory.
        // TODO: Find a way (API change?) to avoid this copy.
        char* out_line = static_cast<char*>(planes[plane]);
        int out_line_stride = row_bytes[plane];
        uint8* in_line = frame_->data(plane) + offset;
        int in_line_stride = frame_->stride(plane);
        int plane_height = sizes[plane].height();
        if (in_line_stride == out_line_stride) {
          memcpy(out_line, in_line, plane_height * in_line_stride);
        } else {
          // Different line padding so need to copy one line at a time.
          int bytes_to_copy_per_line = out_line_stride < in_line_stride
                                           ? out_line_stride
                                           : in_line_stride;
          for (int line_no = 0; line_no < plane_height; line_no++) {
            memcpy(out_line, in_line, bytes_to_copy_per_line);
            in_line += in_line_stride;
            out_line += out_line_stride;
          }
        }
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
      accelerated_generator_(nullptr),
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
                                  VideoRotation video_rotation,
                                  const Context3D& context_3d) {
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

  SkBitmap* target_frame = nullptr;

  if (video_frame->format() == VideoFrame::NATIVE_TEXTURE) {
    // Draw HW Video on both SW and HW Canvas.
    // In SW Canvas case, rely on skia drawing Ganesh SkBitmap on SW SkCanvas.
    if (accelerated_last_frame_.isNull() ||
        video_frame->timestamp() != accelerated_last_frame_timestamp_) {
      DCHECK(context_3d.gl);
      DCHECK(context_3d.gr_context);
      if (accelerated_generator_) {
        // Reset SkBitmap used in SWVideo-to-HWCanvas path.
        accelerated_last_frame_.reset();
        accelerated_generator_ = nullptr;
      }
      if (!CopyVideoFrameTextureToSkBitmapTexture(
              video_frame.get(), &accelerated_last_frame_, context_3d)) {
        NOTREACHED();
        return;
      }
      DCHECK(video_frame->visible_rect().width() ==
                 accelerated_last_frame_.width() &&
             video_frame->visible_rect().height() ==
                 accelerated_last_frame_.height());

      accelerated_last_frame_timestamp_ = video_frame->timestamp();
    }
    target_frame = &accelerated_last_frame_;
    accelerated_frame_deleting_timer_.Reset();
  } else if (canvas->getGrContext()) {
    DCHECK(video_frame->format() != VideoFrame::NATIVE_TEXTURE);
    if (accelerated_last_frame_.isNull() ||
        video_frame->timestamp() != accelerated_last_frame_timestamp_) {
      // Draw SW Video on HW Canvas.
      if (!accelerated_generator_ && !accelerated_last_frame_.isNull()) {
        // Reset SkBitmap used in HWVideo-to-HWCanvas path.
        accelerated_last_frame_.reset();
      }
      accelerated_generator_ = new VideoImageGenerator(video_frame);

      // Note: This takes ownership of |accelerated_generator_|.
      if (!SkInstallDiscardablePixelRef(accelerated_generator_,
                                        &accelerated_last_frame_)) {
        NOTREACHED();
        return;
      }
      DCHECK(video_frame->visible_rect().width() ==
                 accelerated_last_frame_.width() &&
             video_frame->visible_rect().height() ==
                 accelerated_last_frame_.height());

      accelerated_last_frame_timestamp_ = video_frame->timestamp();
    } else if (accelerated_generator_) {
      accelerated_generator_->set_frame(video_frame);
    }
    target_frame = &accelerated_last_frame_;
    accelerated_frame_deleting_timer_.Reset();
  } else {
    // Draw SW Video on SW Canvas.
    DCHECK(video_frame->format() != VideoFrame::NATIVE_TEXTURE);
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
  if (canvas->getGrContext() && accelerated_generator_)
    accelerated_generator_->set_frame(nullptr);
}

void SkCanvasVideoRenderer::Copy(const scoped_refptr<VideoFrame>& video_frame,
                                 SkCanvas* canvas,
                                 const Context3D& context_3d) {
  Paint(video_frame, canvas, video_frame->visible_rect(), 0xff,
        SkXfermode::kSrc_Mode, media::VIDEO_ROTATION_0, context_3d);
}

// static
void SkCanvasVideoRenderer::ConvertVideoFrameToRGBPixels(
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
    case VideoFrame::YV12:
    case VideoFrame::I420:
      LIBYUV_I420_TO_ARGB(
          video_frame->data(VideoFrame::kYPlane) + y_offset,
          video_frame->stride(VideoFrame::kYPlane),
          video_frame->data(VideoFrame::kUPlane) + uv_offset,
          video_frame->stride(VideoFrame::kUPlane),
          video_frame->data(VideoFrame::kVPlane) + uv_offset,
          video_frame->stride(VideoFrame::kVPlane),
          static_cast<uint8*>(rgb_pixels),
          row_bytes,
          video_frame->visible_rect().width(),
          video_frame->visible_rect().height());
      break;

    case VideoFrame::YV12J:
      ConvertYUVToRGB32(
          video_frame->data(VideoFrame::kYPlane) + y_offset,
          video_frame->data(VideoFrame::kUPlane) + uv_offset,
          video_frame->data(VideoFrame::kVPlane) + uv_offset,
          static_cast<uint8*>(rgb_pixels),
          video_frame->visible_rect().width(),
          video_frame->visible_rect().height(),
          video_frame->stride(VideoFrame::kYPlane),
          video_frame->stride(VideoFrame::kUPlane),
          row_bytes,
          YV12J);
      break;

    case VideoFrame::YV12HD:
      ConvertYUVToRGB32(
          video_frame->data(VideoFrame::kYPlane) + y_offset,
          video_frame->data(VideoFrame::kUPlane) + uv_offset,
          video_frame->data(VideoFrame::kVPlane) + uv_offset,
          static_cast<uint8*>(rgb_pixels),
          video_frame->visible_rect().width(),
          video_frame->visible_rect().height(),
          video_frame->stride(VideoFrame::kYPlane),
          video_frame->stride(VideoFrame::kUPlane),
          row_bytes,
          YV12HD);
      break;

    case VideoFrame::YV16:
      LIBYUV_I422_TO_ARGB(
          video_frame->data(VideoFrame::kYPlane) + y_offset,
          video_frame->stride(VideoFrame::kYPlane),
          video_frame->data(VideoFrame::kUPlane) + uv_offset,
          video_frame->stride(VideoFrame::kUPlane),
          video_frame->data(VideoFrame::kVPlane) + uv_offset,
          video_frame->stride(VideoFrame::kVPlane),
          static_cast<uint8*>(rgb_pixels),
          row_bytes,
          video_frame->visible_rect().width(),
          video_frame->visible_rect().height());
      break;

    case VideoFrame::YV12A:
      // Since libyuv doesn't support YUVA, fallback to media, which is not ARM
      // optimized.
      // TODO(fbarchard, mtomasz): Use libyuv, then copy the alpha channel.
      ConvertYUVAToARGB(
          video_frame->data(VideoFrame::kYPlane) + y_offset,
          video_frame->data(VideoFrame::kUPlane) + uv_offset,
          video_frame->data(VideoFrame::kVPlane) + uv_offset,
          video_frame->data(VideoFrame::kAPlane),
          static_cast<uint8*>(rgb_pixels),
          video_frame->visible_rect().width(),
          video_frame->visible_rect().height(),
          video_frame->stride(VideoFrame::kYPlane),
          video_frame->stride(VideoFrame::kUPlane),
          video_frame->stride(VideoFrame::kAPlane),
          row_bytes,
          YV12);
      break;

    case VideoFrame::YV24:
      libyuv::I444ToARGB(
          video_frame->data(VideoFrame::kYPlane) + y_offset,
          video_frame->stride(VideoFrame::kYPlane),
          video_frame->data(VideoFrame::kUPlane) + uv_offset,
          video_frame->stride(VideoFrame::kUPlane),
          video_frame->data(VideoFrame::kVPlane) + uv_offset,
          video_frame->stride(VideoFrame::kVPlane),
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

    case VideoFrame::NATIVE_TEXTURE:
      NOTREACHED();
      break;
#if defined(VIDEO_HOLE)
    case VideoFrame::HOLE:
#endif  // defined(VIDEO_HOLE)
    case VideoFrame::ARGB:
    case VideoFrame::UNKNOWN:
    case VideoFrame::NV12:
      NOTREACHED();
  }
}

// static
void SkCanvasVideoRenderer::CopyVideoFrameTextureToGLTexture(
    gpu::gles2::GLES2Interface* gl,
    VideoFrame* video_frame,
    unsigned int texture,
    unsigned int level,
    unsigned int internal_format,
    unsigned int type,
    bool premultiply_alpha,
    bool flip_y) {
  DCHECK(video_frame && video_frame->format() == VideoFrame::NATIVE_TEXTURE);
  const gpu::MailboxHolder* mailbox_holder = video_frame->mailbox_holder();
  DCHECK(mailbox_holder->texture_target == GL_TEXTURE_2D ||
         mailbox_holder->texture_target == GL_TEXTURE_RECTANGLE_ARB ||
         mailbox_holder->texture_target == GL_TEXTURE_EXTERNAL_OES);

  gl->WaitSyncPointCHROMIUM(mailbox_holder->sync_point);
  uint32 source_texture = gl->CreateAndConsumeTextureCHROMIUM(
      mailbox_holder->texture_target, mailbox_holder->mailbox.name);

  // The video is stored in a unmultiplied format, so premultiply
  // if necessary.
  gl->PixelStorei(GL_UNPACK_PREMULTIPLY_ALPHA_CHROMIUM, premultiply_alpha);
  // Application itself needs to take care of setting the right |flip_y|
  // value down to get the expected result.
  // "flip_y == true" means to reverse the video orientation while
  // "flip_y == false" means to keep the intrinsic orientation.
  gl->PixelStorei(GL_UNPACK_FLIP_Y_CHROMIUM, flip_y);
  gl->CopyTextureCHROMIUM(GL_TEXTURE_2D, source_texture, texture, level,
                          internal_format, type);
  gl->PixelStorei(GL_UNPACK_FLIP_Y_CHROMIUM, false);
  gl->PixelStorei(GL_UNPACK_PREMULTIPLY_ALPHA_CHROMIUM, false);

  gl->DeleteTextures(1, &source_texture);
  gl->Flush();

  SyncPointClientImpl client(gl);
  video_frame->UpdateReleaseSyncPoint(&client);
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
