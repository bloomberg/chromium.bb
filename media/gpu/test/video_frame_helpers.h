// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_TEST_VIDEO_FRAME_HELPERS_H_
#define MEDIA_GPU_TEST_VIDEO_FRAME_HELPERS_H_

#include "base/memory/scoped_refptr.h"
#include "media/base/video_frame_layout.h"
#include "media/base/video_types.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/size.h"

namespace media {

class VideoFrame;

namespace test {

// The video frame processor defines an abstract interface for classes that are
// interested in processing video frames (e.g. FrameValidator,...).
class VideoFrameProcessor {
 public:
  virtual ~VideoFrameProcessor() = default;

  // Process the specified |video_frame|. This can e.g. validate the frame,
  // calculate the frame's checksum, write the frame to file,... The
  // |frame_index| is the index of the video frame in display order. The caller
  // should not modify the video frame while a reference is being held by the
  // VideoFrameProcessor.
  virtual void ProcessVideoFrame(scoped_refptr<const VideoFrame> video_frame,
                                 size_t frame_index) = 0;

  // Wait until all currently scheduled frames have been processed. Returns
  // whether processing was successful.
  virtual bool WaitUntilDone() = 0;
};

// Convert and copy the |src_frame| to the specified |dst_frame|. Supported
// input formats are I420, NV12 and YV12. Supported output formats are I420 and
// ARGB. All mappable output storages types are supported, but writing into
// non-owned memory might produce unexpected side effects.
bool ConvertVideoFrame(const VideoFrame* src_frame, VideoFrame* dst_frame);

// Convert and copy the |src_frame| to a new video frame with specified format.
// Supported input formats are I420, NV12 and YV12. Supported output formats are
// I420 and ARGB.
scoped_refptr<VideoFrame> ConvertVideoFrame(const VideoFrame* src_frame,
                                            VideoPixelFormat dst_pixel_format);

// Create a platform-specific DMA-buffer-backed video frame with specified
// |pixel_format|, |size| and |buffer_usage|.
scoped_refptr<VideoFrame> CreatePlatformVideoFrame(
    VideoPixelFormat pixel_format,
    const gfx::Size& size,
    gfx::BufferUsage buffer_usage = gfx::BufferUsage::SCANOUT_VDA_WRITE);

// Create a video frame layout for the specified |pixel_format| and |size|. The
// created layout will have a separate buffer for each plane in the format.
base::Optional<VideoFrameLayout> CreateVideoFrameLayout(
    VideoPixelFormat pixel_format,
    const gfx::Size& size);

}  // namespace test
}  // namespace media

#endif  // MEDIA_GPU_TEST_VIDEO_FRAME_HELPERS_H_
