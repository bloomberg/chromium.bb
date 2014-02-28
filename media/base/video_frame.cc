// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/video_frame.h"

#include <algorithm>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/aligned_memory.h"
#include "base/strings/string_piece.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "media/base/limits.h"
#include "media/base/video_util.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace media {

// static
scoped_refptr<VideoFrame> VideoFrame::CreateFrame(
    VideoFrame::Format format,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    base::TimeDelta timestamp) {
  DCHECK(IsValidConfig(format, coded_size, visible_rect, natural_size));
  scoped_refptr<VideoFrame> frame(new VideoFrame(
      format, coded_size, visible_rect, natural_size, timestamp, false));
  switch (format) {
    case VideoFrame::YV12:
    case VideoFrame::YV12A:
    case VideoFrame::YV16:
    case VideoFrame::I420:
    case VideoFrame::YV12J:
      frame->AllocateYUV();
      break;
    default:
      LOG(FATAL) << "Unsupported frame format: " << format;
  }
  return frame;
}

// static
std::string VideoFrame::FormatToString(VideoFrame::Format format) {
  switch (format) {
    case VideoFrame::UNKNOWN:
      return "UNKNOWN";
    case VideoFrame::YV12:
      return "YV12";
    case VideoFrame::YV16:
      return "YV16";
    case VideoFrame::I420:
      return "I420";
    case VideoFrame::NATIVE_TEXTURE:
      return "NATIVE_TEXTURE";
#if defined(VIDEO_HOLE)
    case VideoFrame::HOLE:
      return "HOLE";
#endif  // defined(VIDEO_HOLE)
    case VideoFrame::YV12A:
      return "YV12A";
    case VideoFrame::YV12J:
      return "YV12J";
  }
  NOTREACHED() << "Invalid videoframe format provided: " << format;
  return "";
}

// static
bool VideoFrame::IsValidConfig(VideoFrame::Format format,
                               const gfx::Size& coded_size,
                               const gfx::Rect& visible_rect,
                               const gfx::Size& natural_size) {
  return (format != VideoFrame::UNKNOWN &&
          !coded_size.IsEmpty() &&
          coded_size.GetArea() <= limits::kMaxCanvas &&
          coded_size.width() <= limits::kMaxDimension &&
          coded_size.height() <= limits::kMaxDimension &&
          !visible_rect.IsEmpty() &&
          visible_rect.x() >= 0 && visible_rect.y() >= 0 &&
          visible_rect.right() <= coded_size.width() &&
          visible_rect.bottom() <= coded_size.height() &&
          !natural_size.IsEmpty() &&
          natural_size.GetArea() <= limits::kMaxCanvas &&
          natural_size.width() <= limits::kMaxDimension &&
          natural_size.height() <= limits::kMaxDimension);
}

// static
scoped_refptr<VideoFrame> VideoFrame::WrapNativeTexture(
    scoped_ptr<gpu::MailboxHolder> mailbox_holder,
    const ReleaseMailboxCB& mailbox_holder_release_cb,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    base::TimeDelta timestamp,
    const ReadPixelsCB& read_pixels_cb) {
  scoped_refptr<VideoFrame> frame(new VideoFrame(NATIVE_TEXTURE,
                                                 coded_size,
                                                 visible_rect,
                                                 natural_size,
                                                 timestamp,
                                                 false));
  frame->mailbox_holder_ = mailbox_holder.Pass();
  frame->mailbox_holder_release_cb_ = mailbox_holder_release_cb;
  frame->read_pixels_cb_ = read_pixels_cb;

  return frame;
}

void VideoFrame::ReadPixelsFromNativeTexture(const SkBitmap& pixels) {
  DCHECK_EQ(format_, NATIVE_TEXTURE);
  if (!read_pixels_cb_.is_null())
    read_pixels_cb_.Run(pixels);
}

// static
scoped_refptr<VideoFrame> VideoFrame::WrapExternalPackedMemory(
    Format format,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    uint8* data,
    size_t data_size,
    base::SharedMemoryHandle handle,
    base::TimeDelta timestamp,
    const base::Closure& no_longer_needed_cb) {
  if (data_size < AllocationSize(format, coded_size))
    return NULL;

  switch (format) {
    case I420: {
      scoped_refptr<VideoFrame> frame(new VideoFrame(
          format, coded_size, visible_rect, natural_size, timestamp, false));
      frame->shared_memory_handle_ = handle;
      frame->strides_[kYPlane] = coded_size.width();
      frame->strides_[kUPlane] = coded_size.width() / 2;
      frame->strides_[kVPlane] = coded_size.width() / 2;
      frame->data_[kYPlane] = data;
      frame->data_[kUPlane] = data + coded_size.GetArea();
      frame->data_[kVPlane] = data + (coded_size.GetArea() * 5 / 4);
      frame->no_longer_needed_cb_ = no_longer_needed_cb;
      return frame;
    }
    default:
      NOTIMPLEMENTED();
      return NULL;
  }
}

// static
scoped_refptr<VideoFrame> VideoFrame::WrapExternalYuvData(
    Format format,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    int32 y_stride,
    int32 u_stride,
    int32 v_stride,
    uint8* y_data,
    uint8* u_data,
    uint8* v_data,
    base::TimeDelta timestamp,
    const base::Closure& no_longer_needed_cb) {
  DCHECK(format == YV12 || format == YV16 || format == I420) << format;
  scoped_refptr<VideoFrame> frame(new VideoFrame(
      format, coded_size, visible_rect, natural_size, timestamp, false));
  frame->strides_[kYPlane] = y_stride;
  frame->strides_[kUPlane] = u_stride;
  frame->strides_[kVPlane] = v_stride;
  frame->data_[kYPlane] = y_data;
  frame->data_[kUPlane] = u_data;
  frame->data_[kVPlane] = v_data;
  frame->no_longer_needed_cb_ = no_longer_needed_cb;
  return frame;
}

// static
scoped_refptr<VideoFrame> VideoFrame::WrapVideoFrame(
      const scoped_refptr<VideoFrame>& frame,
      const base::Closure& no_longer_needed_cb) {
  scoped_refptr<VideoFrame> wrapped_frame(new VideoFrame(
      frame->format(), frame->coded_size(), frame->visible_rect(),
      frame->natural_size(), frame->GetTimestamp(), frame->end_of_stream()));

  for (size_t i = 0; i < NumPlanes(frame->format()); ++i) {
    wrapped_frame->strides_[i] = frame->stride(i);
    wrapped_frame->data_[i] = frame->data(i);
  }

  wrapped_frame->no_longer_needed_cb_ = no_longer_needed_cb;
  return wrapped_frame;
}

// static
scoped_refptr<VideoFrame> VideoFrame::CreateEOSFrame() {
  return new VideoFrame(VideoFrame::UNKNOWN,
                        gfx::Size(),
                        gfx::Rect(),
                        gfx::Size(),
                        kNoTimestamp(),
                        true);
}

// static
scoped_refptr<VideoFrame> VideoFrame::CreateColorFrame(
    const gfx::Size& size,
    uint8 y, uint8 u, uint8 v,
    base::TimeDelta timestamp) {
  DCHECK(IsValidConfig(VideoFrame::YV12, size, gfx::Rect(size), size));
  scoped_refptr<VideoFrame> frame = VideoFrame::CreateFrame(
      VideoFrame::YV12, size, gfx::Rect(size), size, timestamp);
  FillYUV(frame.get(), y, u, v);
  return frame;
}

// static
scoped_refptr<VideoFrame> VideoFrame::CreateBlackFrame(const gfx::Size& size) {
  const uint8 kBlackY = 0x00;
  const uint8 kBlackUV = 0x80;
  const base::TimeDelta kZero;
  return CreateColorFrame(size, kBlackY, kBlackUV, kBlackUV, kZero);
}

#if defined(VIDEO_HOLE)
// This block and other blocks wrapped around #if defined(VIDEO_HOLE) is not
// maintained by the general compositor team. Please contact the following
// people instead:
//
// wonsik@chromium.org
// ycheo@chromium.org

// static
scoped_refptr<VideoFrame> VideoFrame::CreateHoleFrame(
    const gfx::Size& size) {
  DCHECK(IsValidConfig(VideoFrame::HOLE, size, gfx::Rect(size), size));
  scoped_refptr<VideoFrame> frame(new VideoFrame(
      VideoFrame::HOLE, size, gfx::Rect(size), size, base::TimeDelta(), false));
  return frame;
}
#endif  // defined(VIDEO_HOLE)

// static
size_t VideoFrame::NumPlanes(Format format) {
  switch (format) {
    case VideoFrame::NATIVE_TEXTURE:
#if defined(VIDEO_HOLE)
    case VideoFrame::HOLE:
#endif  // defined(VIDEO_HOLE)
      return 0;
    case VideoFrame::YV12:
    case VideoFrame::YV16:
    case VideoFrame::I420:
    case VideoFrame::YV12J:
      return 3;
    case VideoFrame::YV12A:
      return 4;
    case VideoFrame::UNKNOWN:
      break;
  }
  NOTREACHED() << "Unsupported video frame format: " << format;
  return 0;
}

static inline size_t RoundUp(size_t value, size_t alignment) {
  // Check that |alignment| is a power of 2.
  DCHECK((alignment + (alignment - 1)) == (alignment | (alignment - 1)));
  return ((value + (alignment - 1)) & ~(alignment-1));
}

// static
size_t VideoFrame::AllocationSize(Format format, const gfx::Size& coded_size) {
  size_t total = 0;
  for (size_t i = 0; i < NumPlanes(format); ++i)
    total += PlaneAllocationSize(format, i, coded_size);
  return total;
}

// static
size_t VideoFrame::PlaneAllocationSize(Format format,
                                       size_t plane,
                                       const gfx::Size& coded_size) {
  const size_t area =
      RoundUp(coded_size.width(), 2) * RoundUp(coded_size.height(), 2);
  switch (format) {
    case VideoFrame::YV12:
    case VideoFrame::YV12J:
    case VideoFrame::I420: {
      switch (plane) {
        case VideoFrame::kYPlane:
          return area;
        case VideoFrame::kUPlane:
        case VideoFrame::kVPlane:
          return area / 4;
        default:
          break;
      }
    }
    case VideoFrame::YV12A: {
      switch (plane) {
        case VideoFrame::kYPlane:
        case VideoFrame::kAPlane:
          return area;
        case VideoFrame::kUPlane:
        case VideoFrame::kVPlane:
          return area / 4;
        default:
          break;
      }
    }
    case VideoFrame::YV16: {
      switch (plane) {
        case VideoFrame::kYPlane:
          return area;
        case VideoFrame::kUPlane:
        case VideoFrame::kVPlane:
          return area / 2;
        default:
          break;
      }
    }
    case VideoFrame::UNKNOWN:
    case VideoFrame::NATIVE_TEXTURE:
#if defined(VIDEO_HOLE)
    case VideoFrame::HOLE:
#endif  // defined(VIDEO_HOLE)
      break;
  }
  NOTREACHED() << "Unsupported video frame format/plane: "
               << format << "/" << plane;
  return 0;
}

// Release data allocated by AllocateYUV().
static void ReleaseData(uint8* data) {
  DCHECK(data);
  base::AlignedFree(data);
}

void VideoFrame::AllocateYUV() {
  DCHECK(format_ == VideoFrame::YV12 || format_ == VideoFrame::YV16 ||
         format_ == VideoFrame::YV12A || format_ == VideoFrame::I420 ||
         format_ == VideoFrame::YV12J);
  // Align Y rows at least at 16 byte boundaries.  The stride for both
  // YV12 and YV16 is 1/2 of the stride of Y.  For YV12, every row of bytes for
  // U and V applies to two rows of Y (one byte of UV for 4 bytes of Y), so in
  // the case of YV12 the strides are identical for the same width surface, but
  // the number of bytes allocated for YV12 is 1/2 the amount for U & V as
  // YV16. We also round the height of the surface allocated to be an even
  // number to avoid any potential of faulting by code that attempts to access
  // the Y values of the final row, but assumes that the last row of U & V
  // applies to a full two rows of Y. YV12A is the same as YV12, but with an
  // additional alpha plane that has the same size and alignment as the Y plane.

  size_t y_stride = RoundUp(row_bytes(VideoFrame::kYPlane),
                            kFrameSizeAlignment);
  size_t uv_stride = RoundUp(row_bytes(VideoFrame::kUPlane),
                             kFrameSizeAlignment);
  // The *2 here is because some formats (e.g. h264) allow interlaced coding,
  // and then the size needs to be a multiple of two macroblocks (vertically).
  // See libavcodec/utils.c:avcodec_align_dimensions2().
  size_t y_height = RoundUp(coded_size_.height(), kFrameSizeAlignment * 2);
  size_t uv_height =
      (format_ == VideoFrame::YV12 || format_ == VideoFrame::YV12A ||
       format_ == VideoFrame::I420)
          ? y_height / 2
          : y_height;
  size_t y_bytes = y_height * y_stride;
  size_t uv_bytes = uv_height * uv_stride;
  size_t a_bytes = format_ == VideoFrame::YV12A ? y_bytes : 0;

  // The extra line of UV being allocated is because h264 chroma MC
  // overreads by one line in some cases, see libavcodec/utils.c:
  // avcodec_align_dimensions2() and libavcodec/x86/h264_chromamc.asm:
  // put_h264_chroma_mc4_ssse3().
  uint8* data = reinterpret_cast<uint8*>(
      base::AlignedAlloc(
          y_bytes + (uv_bytes * 2 + uv_stride) + a_bytes + kFrameSizePadding,
          kFrameAddressAlignment));
  no_longer_needed_cb_ = base::Bind(&ReleaseData, data);
  COMPILE_ASSERT(0 == VideoFrame::kYPlane, y_plane_data_must_be_index_0);
  data_[VideoFrame::kYPlane] = data;
  data_[VideoFrame::kUPlane] = data + y_bytes;
  data_[VideoFrame::kVPlane] = data + y_bytes + uv_bytes;
  strides_[VideoFrame::kYPlane] = y_stride;
  strides_[VideoFrame::kUPlane] = uv_stride;
  strides_[VideoFrame::kVPlane] = uv_stride;
  if (format_ == YV12A) {
    data_[VideoFrame::kAPlane] = data + y_bytes + (2 * uv_bytes);
    strides_[VideoFrame::kAPlane] = y_stride;
  }
}

VideoFrame::VideoFrame(VideoFrame::Format format,
                       const gfx::Size& coded_size,
                       const gfx::Rect& visible_rect,
                       const gfx::Size& natural_size,
                       base::TimeDelta timestamp,
                       bool end_of_stream)
    : format_(format),
      coded_size_(coded_size),
      visible_rect_(visible_rect),
      natural_size_(natural_size),
      shared_memory_handle_(base::SharedMemory::NULLHandle()),
      timestamp_(timestamp),
      end_of_stream_(end_of_stream) {
  memset(&strides_, 0, sizeof(strides_));
  memset(&data_, 0, sizeof(data_));
}

VideoFrame::~VideoFrame() {
  if (!mailbox_holder_release_cb_.is_null()) {
    base::ResetAndReturn(&mailbox_holder_release_cb_)
        .Run(mailbox_holder_.Pass());
  }
  if (!no_longer_needed_cb_.is_null())
    base::ResetAndReturn(&no_longer_needed_cb_).Run();
}

bool VideoFrame::IsValidPlane(size_t plane) const {
  return (plane < NumPlanes(format_));
}

int VideoFrame::stride(size_t plane) const {
  DCHECK(IsValidPlane(plane));
  return strides_[plane];
}

int VideoFrame::row_bytes(size_t plane) const {
  DCHECK(IsValidPlane(plane));
  int width = coded_size_.width();
  switch (format_) {
    // Planar, 8bpp.
    case YV12A:
      if (plane == kAPlane)
        return width;
    // Fallthrough.
    case YV12:
    case YV16:
    case I420:
    case YV12J:
      if (plane == kYPlane)
        return width;
      return RoundUp(width, 2) / 2;

    default:
      break;
  }

  // Intentionally leave out non-production formats.
  NOTREACHED() << "Unsupported video frame format: " << format_;
  return 0;
}

int VideoFrame::rows(size_t plane) const {
  DCHECK(IsValidPlane(plane));
  int height = coded_size_.height();
  switch (format_) {
    case YV16:
      return height;

    case YV12A:
      if (plane == kAPlane)
        return height;
    // Fallthrough.
    case YV12:
    case I420:
      if (plane == kYPlane)
        return height;
      return RoundUp(height, 2) / 2;

    default:
      break;
  }

  // Intentionally leave out non-production formats.
  NOTREACHED() << "Unsupported video frame format: " << format_;
  return 0;
}

uint8* VideoFrame::data(size_t plane) const {
  DCHECK(IsValidPlane(plane));
  return data_[plane];
}

gpu::MailboxHolder* VideoFrame::mailbox_holder() const {
  DCHECK_EQ(format_, NATIVE_TEXTURE);
  return mailbox_holder_.get();
}

base::SharedMemoryHandle VideoFrame::shared_memory_handle() const {
  return shared_memory_handle_;
}

void VideoFrame::HashFrameForTesting(base::MD5Context* context) {
  for (int plane = 0; plane < kMaxPlanes; ++plane) {
    if (!IsValidPlane(plane))
      break;
    for (int row = 0; row < rows(plane); ++row) {
      base::MD5Update(context, base::StringPiece(
          reinterpret_cast<char*>(data(plane) + stride(plane) * row),
          row_bytes(plane)));
    }
  }
}

}  // namespace media
