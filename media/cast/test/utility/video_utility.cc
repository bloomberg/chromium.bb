// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/test/utility/video_utility.h"

#include <math.h>
#include <cstdio>

#include "base/rand_util.h"
#include "third_party/libyuv/include/libyuv/compare.h"
#include "ui/gfx/geometry/size.h"

namespace media {
namespace cast {

double I420PSNR(const scoped_refptr<media::VideoFrame>& frame1,
                const scoped_refptr<media::VideoFrame>& frame2) {
  if (frame1->coded_size().width() != frame2->coded_size().width() ||
      frame1->coded_size().height() != frame2->coded_size().height())
    return -1;

  return libyuv::I420Psnr(frame1->data(VideoFrame::kYPlane),
                          frame1->stride(VideoFrame::kYPlane),
                          frame1->data(VideoFrame::kUPlane),
                          frame1->stride(VideoFrame::kUPlane),
                          frame1->data(VideoFrame::kVPlane),
                          frame1->stride(VideoFrame::kVPlane),
                          frame2->data(VideoFrame::kYPlane),
                          frame2->stride(VideoFrame::kYPlane),
                          frame2->data(VideoFrame::kUPlane),
                          frame2->stride(VideoFrame::kUPlane),
                          frame2->data(VideoFrame::kVPlane),
                          frame2->stride(VideoFrame::kVPlane),
                          frame1->coded_size().width(),
                          frame1->coded_size().height());
}

double I420SSIM(const scoped_refptr<media::VideoFrame>& frame1,
                const scoped_refptr<media::VideoFrame>& frame2) {
  if (frame1->coded_size().width() != frame2->coded_size().width() ||
      frame1->coded_size().height() != frame2->coded_size().height())
    return -1;

  return libyuv::I420Ssim(frame1->data(VideoFrame::kYPlane),
                          frame1->stride(VideoFrame::kYPlane),
                          frame1->data(VideoFrame::kUPlane),
                          frame1->stride(VideoFrame::kUPlane),
                          frame1->data(VideoFrame::kVPlane),
                          frame1->stride(VideoFrame::kVPlane),
                          frame2->data(VideoFrame::kYPlane),
                          frame2->stride(VideoFrame::kYPlane),
                          frame2->data(VideoFrame::kUPlane),
                          frame2->stride(VideoFrame::kUPlane),
                          frame2->data(VideoFrame::kVPlane),
                          frame2->stride(VideoFrame::kVPlane),
                          frame1->coded_size().width(),
                          frame1->coded_size().height());
}

void PopulateVideoFrame(VideoFrame* frame, int start_value) {
  const gfx::Size frame_size = frame->coded_size();
  const int stripe_size =
      std::max(32, std::min(frame_size.width(), frame_size.height()) / 8) & -2;

  // Set Y.
  const int height = frame_size.height();
  const int stride_y = frame->stride(VideoFrame::kYPlane);
  uint8* y_plane = frame->data(VideoFrame::kYPlane);
  for (int j = 0; j < height; ++j) {
    const int stripe_j = (j / stripe_size) * stripe_size;
    for (int i = 0; i < stride_y; ++i) {
      const int stripe_i = (i / stripe_size) * stripe_size;
      *y_plane = static_cast<uint8>(start_value + stripe_i + stripe_j);
      ++y_plane;
    }
  }

  const int half_height = (height + 1) / 2;
  if (frame->format() == VideoFrame::NV12) {
    const int stride_uv = frame->stride(VideoFrame::kUVPlane);
    uint8* uv_plane = frame->data(VideoFrame::kUVPlane);

    // Set U and V.
    for (int j = 0; j < half_height; ++j) {
      const int stripe_j = (j / stripe_size) * stripe_size;
      for (int i = 0; i < stride_uv; i += 2) {
        const int stripe_i = (i / stripe_size) * stripe_size;
        *uv_plane = *(uv_plane + 1) =
            static_cast<uint8>(start_value + stripe_i + stripe_j);
        uv_plane += 2;
      }
    }
  } else {  // I420, YV12, etc.
    const int stride_u = frame->stride(VideoFrame::kUPlane);
    const int stride_v = frame->stride(VideoFrame::kVPlane);
    uint8* u_plane = frame->data(VideoFrame::kUPlane);
    uint8* v_plane = frame->data(VideoFrame::kVPlane);

    // Set U.
    for (int j = 0; j < half_height; ++j) {
      const int stripe_j = (j / stripe_size) * stripe_size;
      for (int i = 0; i < stride_u; ++i) {
        const int stripe_i = (i / stripe_size) * stripe_size;
        *u_plane = static_cast<uint8>(start_value + stripe_i + stripe_j);
        ++u_plane;
      }
    }

    // Set V.
    for (int j = 0; j < half_height; ++j) {
      const int stripe_j = (j / stripe_size) * stripe_size;
      for (int i = 0; i < stride_v; ++i) {
        const int stripe_i = (i / stripe_size) * stripe_size;
        *u_plane = static_cast<uint8>(start_value + stripe_i + stripe_j);
        ++v_plane;
      }
    }
  }
}

void PopulateVideoFrameWithNoise(VideoFrame* frame) {
  int height = frame->coded_size().height();
  int stride_y = frame->stride(VideoFrame::kYPlane);
  int stride_u = frame->stride(VideoFrame::kUPlane);
  int stride_v = frame->stride(VideoFrame::kVPlane);
  int half_height = (height + 1) / 2;
  uint8* y_plane = frame->data(VideoFrame::kYPlane);
  uint8* u_plane = frame->data(VideoFrame::kUPlane);
  uint8* v_plane = frame->data(VideoFrame::kVPlane);

  base::RandBytes(y_plane, height * stride_y);
  base::RandBytes(u_plane, half_height * stride_u);
  base::RandBytes(v_plane, half_height * stride_v);
}

bool PopulateVideoFrameFromFile(VideoFrame* frame, FILE* video_file) {
  int width = frame->coded_size().width();
  int height = frame->coded_size().height();
  int half_width = (width + 1) / 2;
  int half_height = (height + 1) / 2;
  size_t frame_size = width * height + 2 * half_width * half_height;
  uint8* y_plane = frame->data(VideoFrame::kYPlane);
  uint8* u_plane = frame->data(VideoFrame::kUPlane);
  uint8* v_plane = frame->data(VideoFrame::kVPlane);

  uint8* raw_data = new uint8[frame_size];
  size_t count = fread(raw_data, 1, frame_size, video_file);
  if (count != frame_size)
    return false;

  memcpy(y_plane, raw_data, width * height);
  memcpy(u_plane, raw_data + width * height, half_width * half_height);
  memcpy(v_plane,
         raw_data + width * height + half_width * half_height,
         half_width * half_height);
  delete[] raw_data;
  return true;
}

}  // namespace cast
}  // namespace media
