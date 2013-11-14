// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <math.h>
#include <cstdio>

#include "media/cast/test/video_utility.h"
#include "third_party/libyuv/include/libyuv/compare.h"

namespace media {
namespace cast {

double I420PSNR(const I420VideoFrame& frame1, const I420VideoFrame& frame2) {
  // Frames should have equal resolution.
  if (frame1.width != frame2.width || frame1.height != frame2.height) return -1;

  return libyuv::I420Psnr(frame1.y_plane.data, frame1.y_plane.stride,
                          frame1.u_plane.data, frame1.u_plane.stride,
                          frame1.v_plane.data, frame1.v_plane.stride,
                          frame2.y_plane.data, frame2.y_plane.stride,
                          frame2.u_plane.data, frame2.u_plane.stride,
                          frame2.v_plane.data, frame2.v_plane.stride,
                          frame1.width, frame1.height);
}

void PopulateVideoFrame(I420VideoFrame* frame, int start_value) {
  int half_width = (frame->width + 1) / 2;
  int half_height = (frame->height + 1) / 2;
  frame->y_plane.stride = frame->width;
  frame->y_plane.length = frame->width * frame->height;
  frame->y_plane.data = new uint8[frame->y_plane.length];

  frame->u_plane.stride = half_width;
  frame->u_plane.length = half_width * half_height;
  frame->u_plane.data = new uint8[frame->u_plane.length];

  frame->v_plane.stride = half_width;
  frame->v_plane.length = half_width * half_height;
  frame->v_plane.data = new uint8[frame->v_plane.length];

  // Set Y.
  for (int i = 0; i < frame->y_plane.length; ++i) {
    frame->y_plane.data[i] = static_cast<uint8>(start_value + i);
  }

  // Set U.
  for (int i = 0; i < frame->u_plane.length; ++i) {
    frame->u_plane.data[i] = static_cast<uint8>(start_value + i);
  }

  // Set V.
  for (int i = 0; i < frame->v_plane.length; ++i) {
    frame->v_plane.data[i] = static_cast<uint8>(start_value + i);
  }
}

bool PopulateVideoFrameFromFile(I420VideoFrame* frame, FILE* video_file) {
  int half_width = (frame->width + 1) / 2;
  int half_height = (frame->height + 1) / 2;
  size_t frame_size =
      frame->width * frame->height + 2 * half_width * half_height;

  uint8* raw_data = new uint8[frame_size];
  size_t count = fread(raw_data, 1, frame_size, video_file);
  if (count != frame_size) return false;

  frame->y_plane.stride = frame->width;
  frame->y_plane.length = frame->width * frame->height;
  frame->y_plane.data = new uint8[frame->y_plane.length];

  frame->u_plane.stride = half_width;
  frame->u_plane.length = half_width * half_height;
  frame->u_plane.data = new uint8[frame->u_plane.length];

  frame->v_plane.stride = half_width;
  frame->v_plane.length = half_width * half_height;
  frame->v_plane.data = new uint8[frame->v_plane.length];

  memcpy(frame->y_plane.data, raw_data, frame->width * frame->height);
  memcpy(frame->u_plane.data, raw_data + frame->width * frame->height,
      half_width * half_height);
  memcpy(frame->v_plane.data, raw_data + frame->width * frame->height +
      half_width * half_height, half_width * half_height);
  delete [] raw_data;
  return true;
}

}  // namespace cast
}  // namespace media
