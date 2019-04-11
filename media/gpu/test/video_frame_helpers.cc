// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/test/video_frame_helpers.h"

#include <utility>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "media/base/video_frame.h"
#include "media/gpu/test/image.h"
#include "third_party/libyuv/include/libyuv.h"
#include "ui/gfx/gpu_memory_buffer.h"

#if defined(OS_CHROMEOS)
#include "media/base/scopedfd_helper.h"
#include "media/gpu/linux/platform_video_frame_utils.h"
#endif

namespace media {
namespace test {

namespace {

bool ConvertVideoFrameToI420(const VideoFrame* src_frame,
                             VideoFrame* dst_frame) {
  LOG_ASSERT(src_frame->visible_rect() == dst_frame->visible_rect());
  LOG_ASSERT(dst_frame->format() == PIXEL_FORMAT_I420);

  const auto& visible_rect = src_frame->visible_rect();
  const int width = visible_rect.width();
  const int height = visible_rect.height();
  uint8_t* const dst_y = dst_frame->data(VideoFrame::kYPlane);
  uint8_t* const dst_u = dst_frame->data(VideoFrame::kUPlane);
  uint8_t* const dst_v = dst_frame->data(VideoFrame::kVPlane);
  const int dst_stride_y = dst_frame->stride(VideoFrame::kYPlane);
  const int dst_stride_u = dst_frame->stride(VideoFrame::kUPlane);
  const int dst_stride_v = dst_frame->stride(VideoFrame::kVPlane);

  switch (src_frame->format()) {
    case PIXEL_FORMAT_I420:
      return libyuv::I420Copy(src_frame->data(VideoFrame::kYPlane),
                              src_frame->stride(VideoFrame::kYPlane),
                              src_frame->data(VideoFrame::kUPlane),
                              src_frame->stride(VideoFrame::kUPlane),
                              src_frame->data(VideoFrame::kVPlane),
                              src_frame->stride(VideoFrame::kVPlane), dst_y,
                              dst_stride_y, dst_u, dst_stride_u, dst_v,
                              dst_stride_v, width, height) == 0;
    case PIXEL_FORMAT_NV12:
      return libyuv::NV12ToI420(src_frame->data(VideoFrame::kYPlane),
                                src_frame->stride(VideoFrame::kYPlane),
                                src_frame->data(VideoFrame::kUVPlane),
                                src_frame->stride(VideoFrame::kUVPlane), dst_y,
                                dst_stride_y, dst_u, dst_stride_u, dst_v,
                                dst_stride_v, width, height) == 0;
    case PIXEL_FORMAT_YV12:
      // Swap U and V planes.
      return libyuv::I420Copy(src_frame->data(VideoFrame::kYPlane),
                              src_frame->stride(VideoFrame::kYPlane),
                              src_frame->data(VideoFrame::kVPlane),
                              src_frame->stride(VideoFrame::kVPlane),
                              src_frame->data(VideoFrame::kUPlane),
                              src_frame->stride(VideoFrame::kUPlane), dst_y,
                              dst_stride_y, dst_u, dst_stride_u, dst_v,
                              dst_stride_v, width, height) == 0;
    default:
      LOG(ERROR) << "Unsupported input format: " << src_frame->format();
      return false;
  }
}

bool ConvertVideoFrameToARGB(const VideoFrame* src_frame,
                             VideoFrame* dst_frame) {
  LOG_ASSERT(src_frame->visible_rect() == dst_frame->visible_rect());
  LOG_ASSERT(dst_frame->format() == PIXEL_FORMAT_ARGB);

  const auto& visible_rect = src_frame->visible_rect();
  const int width = visible_rect.width();
  const int height = visible_rect.height();
  uint8_t* const dst_argb = dst_frame->data(VideoFrame::kARGBPlane);
  const int dst_stride = dst_frame->stride(VideoFrame::kARGBPlane);

  switch (src_frame->format()) {
    case PIXEL_FORMAT_I420:
      // Note that we use J420ToARGB instead of I420ToARGB so that the
      // kYuvJPEGConstants YUV-to-RGB conversion matrix is used.
      return libyuv::J420ToARGB(src_frame->data(VideoFrame::kYPlane),
                                src_frame->stride(VideoFrame::kYPlane),
                                src_frame->data(VideoFrame::kUPlane),
                                src_frame->stride(VideoFrame::kUPlane),
                                src_frame->data(VideoFrame::kVPlane),
                                src_frame->stride(VideoFrame::kVPlane),
                                dst_argb, dst_stride, width, height) == 0;
    case PIXEL_FORMAT_NV12:
      return libyuv::NV12ToARGB(src_frame->data(VideoFrame::kYPlane),
                                src_frame->stride(VideoFrame::kYPlane),
                                src_frame->data(VideoFrame::kUVPlane),
                                src_frame->stride(VideoFrame::kUVPlane),
                                dst_argb, dst_stride, width, height) == 0;
    case PIXEL_FORMAT_YV12:
      // Same as I420, but U and V planes are swapped.
      return libyuv::J420ToARGB(src_frame->data(VideoFrame::kYPlane),
                                src_frame->stride(VideoFrame::kYPlane),
                                src_frame->data(VideoFrame::kVPlane),
                                src_frame->stride(VideoFrame::kVPlane),
                                src_frame->data(VideoFrame::kUPlane),
                                src_frame->stride(VideoFrame::kUPlane),
                                dst_argb, dst_stride, width, height) == 0;
      break;
    default:
      LOG(ERROR) << "Unsupported input format: " << src_frame->format();
      return false;
  }
}

}  // namespace

bool ConvertVideoFrame(const VideoFrame* src_frame, VideoFrame* dst_frame) {
  LOG_ASSERT(src_frame->visible_rect() == dst_frame->visible_rect());
  LOG_ASSERT(src_frame->IsMappable() && dst_frame->IsMappable());

  // Writing into non-owned memory might produce some unexpected side effects.
  if (dst_frame->storage_type() != VideoFrame::STORAGE_OWNED_MEMORY)
    LOG(WARNING) << "writing into non-owned memory";

  // Only I420 and ARGB are currently supported as output formats.
  switch (dst_frame->format()) {
    case PIXEL_FORMAT_I420:
      return ConvertVideoFrameToI420(src_frame, dst_frame);
    case PIXEL_FORMAT_ARGB:
      return ConvertVideoFrameToARGB(src_frame, dst_frame);
    default:
      LOG(ERROR) << "Unsupported output format: " << dst_frame->format();
      return false;
  }
}

scoped_refptr<VideoFrame> ConvertVideoFrame(const VideoFrame* src_frame,
                                            VideoPixelFormat dst_pixel_format) {
  gfx::Rect visible_rect = src_frame->visible_rect();
  auto dst_frame = VideoFrame::CreateFrame(
      dst_pixel_format, visible_rect.size(), visible_rect, visible_rect.size(),
      base::TimeDelta());
  if (!dst_frame) {
    LOG(ERROR) << "Failed to convert video frame to " << dst_frame->format();
    return nullptr;
  }
  bool conversion_success = ConvertVideoFrame(src_frame, dst_frame.get());
  if (!conversion_success) {
    LOG(ERROR) << "Failed to convert video frame to " << dst_frame->format();
    return nullptr;
  }
  return dst_frame;
}

base::Optional<VideoFrameLayout> CreateVideoFrameLayout(VideoPixelFormat format,
                                                        const gfx::Size& size,
                                                        bool single_buffer) {
  const size_t num_planes = VideoFrame::NumPlanes(format);
  const size_t num_buffers = single_buffer ? 1u : num_planes;
  // If num_buffers = 1, all the planes are stored in the same buffer.
  // If num_buffers = num_planes, each of plane is stored in a separate
  // buffer and located in the beginning of the buffer.
  std::vector<size_t> buffer_sizes(num_buffers);
  std::vector<VideoFrameLayout::Plane> planes(num_planes);
  const auto strides = VideoFrame::ComputeStrides(format, size);
  size_t offset = 0;
  for (size_t i = 0; i < num_planes; ++i) {
    planes[i].stride = strides[i];
    if (num_buffers == 1) {
      planes[i].offset = offset;
      offset += VideoFrame::PlaneSize(format, i, size).GetArea();
    } else {
      planes[i].offset = 0;
      buffer_sizes[i] = VideoFrame::PlaneSize(format, i, size).GetArea();
    }
  }

  if (num_buffers == 1)
    buffer_sizes[0] = VideoFrame::AllocationSize(format, size);

  return VideoFrameLayout::CreateWithPlanes(format, size, std::move(planes),
                                            std::move(buffer_sizes));
}

scoped_refptr<const VideoFrame> CreateVideoFrameFromImage(const Image& image) {
  DCHECK(image.IsLoaded());
  const auto format = image.PixelFormat();
  const auto& visible_size = image.Size();
  // Loaded image data must be tight.
  DCHECK_EQ(image.DataSize(), VideoFrame::AllocationSize(format, visible_size));

  // Create planes for layout. We cannot use WrapExternalData() because it
  // calls GetDefaultLayout() and it supports only a few pixel formats.
  base::Optional<VideoFrameLayout> layout =
      CreateVideoFrameLayout(format, visible_size, 1u /* num_buffers */);
  if (!layout) {
    LOG(ERROR) << "Failed to create VideoFrameLayout";
    return nullptr;
  }

  scoped_refptr<const VideoFrame> video_frame =
      VideoFrame::WrapExternalDataWithLayout(
          *layout, gfx::Rect(visible_size), visible_size, image.Data(),
          image.DataSize(), base::TimeDelta());
  if (!video_frame) {
    LOG(ERROR) << "Failed to create VideoFrame";
    return nullptr;
  }

  return video_frame;
}

}  // namespace test
}  // namespace media
