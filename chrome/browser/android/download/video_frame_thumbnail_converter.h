// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_DOWNLOAD_VIDEO_FRAME_THUMBNAIL_CONVERTER_H_
#define CHROME_BROWSER_ANDROID_DOWNLOAD_VIDEO_FRAME_THUMBNAIL_CONVERTER_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/memory/scoped_refptr.h"
#include "media/base/video_codecs.h"

namespace media {
class VideoFrame;
}  // namespace media

namespace ws {
class ContextProviderCommandBuffer;
}  // namespace ws

class SkBitmap;

// Generates video thumbnail from a video frame. For most video format such as
// H264, the input video frame contains a texture created in GPU process, we do
// a readback and generate the bitmap. For vp8, vp9 format, the video frame
// contains in-memory YUV data.
class VideoFrameThumbnailConverter {
 public:
  using BitmapCallback = base::OnceCallback<void(bool, SkBitmap)>;

  // Create the video thumbnail renderer for video type with |codec|. The
  // decoder to generate video frame may yields video frames backed by texture
  // in remote process or raw YUV data.
  static std::unique_ptr<VideoFrameThumbnailConverter> Create(
      media::VideoCodec codec,
      scoped_refptr<ws::ContextProviderCommandBuffer> context_provider);

  virtual void ConvertToBitmap(const scoped_refptr<media::VideoFrame>& frame,
                               BitmapCallback pixel_callback) = 0;

  virtual ~VideoFrameThumbnailConverter() = default;
};

#endif  // CHROME_BROWSER_ANDROID_DOWNLOAD_VIDEO_FRAME_THUMBNAIL_CONVERTER_H_
