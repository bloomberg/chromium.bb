// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <math.h>
#include <cstdio>

#include "media/base/video_frame.h"
#include "media/cast/test/video_utility.h"
#include "third_party/libyuv/include/libyuv/compare.h"
#include "ui/gfx/size.h"

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

double I420PSNR(const VideoFrame& frame1, const I420VideoFrame& frame2) {
  if (frame1.coded_size().width() != frame2.width ||
      frame1.coded_size().height() != frame2.height) return -1;

  return libyuv::I420Psnr(
      frame1.data(VideoFrame::kYPlane), frame1.stride(VideoFrame::kYPlane),
      frame1.data(VideoFrame::kUPlane), frame1.stride(VideoFrame::kUPlane),
      frame1.data(VideoFrame::kVPlane), frame1.stride(VideoFrame::kVPlane),
      frame2.y_plane.data, frame2.y_plane.stride,
      frame2.u_plane.data, frame2.u_plane.stride,
      frame2.v_plane.data, frame2.v_plane.stride,
      frame2.width, frame2.height);
}

void PopulateVideoFrame(VideoFrame* frame, int start_value) {
  int width = frame->coded_size().width();
  int height = frame->coded_size().height();
  int half_width = (width + 1) / 2;
  int half_height = (height + 1) / 2;
  uint8* y_plane = frame->data(VideoFrame::kYPlane);
  uint8* u_plane = frame->data(VideoFrame::kUPlane);
  uint8* v_plane = frame->data(VideoFrame::kVPlane);

  // Set Y.
  for (int i = 0; i < width * height; ++i) {
    y_plane[i] = static_cast<uint8>(start_value + i);
  }

  // Set U.
  for (int i = 0; i < half_width * half_height; ++i) {
    u_plane[i] = static_cast<uint8>(start_value + i);
  }

  // Set V.
  for (int i = 0; i < half_width * half_height; ++i) {
    v_plane[i] = static_cast<uint8>(start_value + i);
  }
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
  if (count != frame_size) return false;

  memcpy(y_plane, raw_data, width * height);
  memcpy(u_plane, raw_data + width * height, half_width * half_height);
  memcpy(v_plane, raw_data + width * height +
      half_width * half_height, half_width * half_height);
  delete [] raw_data;
  return true;
}

}  // namespace cast
}  // namespace media
