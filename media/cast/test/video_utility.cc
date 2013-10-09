// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <math.h>

#include "media/cast/test/video_utility.h"

namespace media {
namespace cast {

double I420PSNR(const I420VideoFrame& frame1, const I420VideoFrame& frame2) {
  // Frames should have equal resolution.
  if (frame1.width != frame2.width || frame1.height != frame2.height) return -1;

  double y_mse = 0.0;
  // Y.
  uint8* data1 = frame1.y_plane.data;
  uint8* data2 = frame2.y_plane.data;
  for (int i = 0; i < frame1.width; ++i) {
    for (int j = 0; j < frame1.height; ++j) {
      y_mse += (data1[j] - data2[j]) * (data1[j] - data2[j]);
    }
    // Account for stride.
    data1 += frame1.y_plane.stride;
    data2 += frame2.y_plane.stride;
  }
  y_mse /= (frame1.width * frame1.height);


  int half_width = (frame1.width + 1) / 2;
  int half_height = (frame1.height + 1) / 2;
  // U.
  double u_mse = 0.0;
  data1 = frame1.u_plane.data;
  data2 = frame2.u_plane.data;
  for (int i = 0; i < half_width; ++i) {
    for (int j = 0; j < half_height; ++j) {
      u_mse += (data1[j] - data2[j]) * (data1[j] - data2[j]);
    }
    // Account for stride.
    data1 += frame1.u_plane.stride;
    data2 += frame2.u_plane.stride;
  }
  u_mse /= half_width * half_height;

  // V.
  double v_mse = 0.0;
  data1 = frame1.v_plane.data;
  data2 = frame2.v_plane.data;
  for (int i = 0; i < half_width; ++i) {
    for (int j = 0; j < half_height; ++j) {
      v_mse += (data1[j] - data2[j]) * (data1[j] - data2[j]);
    }
    // Account for stride.
    data1 += frame2.v_plane.stride;
    data2 += frame2.v_plane.stride;
  }
  v_mse /= half_width * half_height;

  // Combine to one psnr value.
  return 10.0 * log10(255.0 * 255.0 * (y_mse + u_mse + v_mse));
}

void PopulateVideoFrame(I420VideoFrame* frame, int start_value) {
  int half_width = (frame->width + 1) / 2;
  int half_height = (frame->height + 1) / 2;
  int frame_length = frame->width * frame->height +
      2 * half_width * half_height;
  frame->y_plane.length = frame->width * frame->height;
  frame->y_plane.stride = frame->width;
  frame->y_plane.data = new uint8[frame->width * frame->height];

  frame->u_plane.stride = half_width;
  frame->u_plane.length = half_width * half_height;
  frame->u_plane.data = frame->y_plane.data +
      frame->width * frame->height;
  frame->u_plane.data = new uint8[half_width * half_height];

  frame->v_plane.stride = half_width;
  frame->v_plane.length = half_width * half_height;
  frame->v_plane.data = frame->u_plane.data +
      half_width * half_height;
  frame->v_plane.data = new uint8[half_width * half_height];

  // Set Y.
  for (int i = 0; i < frame->height * frame->width; ++i) {
    frame->y_plane.data[i] = static_cast<uint8>(start_value + i);
  }

  // Set U.
  for (int i = 0; i < half_width * half_height; ++i) {
    frame->u_plane.data[i] = static_cast<uint8>(start_value + i);
  }

  // Set V.
  for (int i = 0; i < half_width * half_height; ++i) {
    frame->v_plane.data[i] = static_cast<uint8>(start_value + i);
  }
}

}  // namespace cast
}  // namespace media
