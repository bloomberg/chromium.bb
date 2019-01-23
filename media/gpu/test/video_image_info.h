
// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_TEST_VIDEO_IMAGE_INFO_H_
#define MEDIA_GPU_TEST_VIDEO_IMAGE_INFO_H_

#include <vector>

#include "base/files/file_path.h"
#include "base/optional.h"
#include "media/base/video_frame.h"
#include "media/base/video_frame_layout.h"
#include "media/base/video_types.h"
#include "ui/gfx/geometry/size.h"

namespace media {
namespace test {

// VideoImageInfo is the information about raw video frame in file.
struct VideoImageInfo {
  // TODO(crbug.com/917951): Deprecate this constructor once we load these info
  // from json file.
  constexpr VideoImageInfo(const base::FilePath::CharType* const file_name,
                           const char* const md5sum,
                           VideoPixelFormat pixel_format,
                           gfx::Size size)
      : file_name(file_name),
        md5sum(md5sum),
        pixel_format(pixel_format),
        visible_size(size.width(), size.height()) {}
  VideoImageInfo() = delete;
  ~VideoImageInfo() = default;

  base::Optional<VideoFrameLayout> VideoFrameLayout() const {
    return VideoFrameLayout::CreateWithStrides(
        pixel_format, visible_size,
        VideoFrame::ComputeStrides(pixel_format, visible_size),
        std::vector<size_t>(VideoFrame::NumPlanes(pixel_format),
                            0) /* buffer_sizes */);
  }

  // |file_name| is a file name to be read(e.g. "bear_320x192.i420.yuv"), not
  // file path.
  const base::FilePath::CharType* const file_name;
  //| md5sum| is the md5sum value of the video frame, whose coded_size is the
  // same as visible size.
  const char* const md5sum;

  // |pixel_format| and |visible_size| of the video frame in file.
  // NOTE: visible_size should be the same as coded_size, i.e., there is no
  // extra padding in the file.
  const VideoPixelFormat pixel_format;
  const gfx::Size visible_size;
};

}  // namespace test
}  // namespace media
#endif  // MEDIA_GPU_TEST_VIDEO_IMAGE_INFO_H_
