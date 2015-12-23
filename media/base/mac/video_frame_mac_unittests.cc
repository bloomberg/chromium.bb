// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/mac/video_frame_mac.h"

#include <stddef.h>

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "media/base/mac/corevideo_glue.h"
#include "media/base/video_frame.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {

const int kWidth = 64;
const int kHeight = 48;
const base::TimeDelta kTimestamp = base::TimeDelta::FromMicroseconds(1337);

struct FormatPair {
  VideoPixelFormat chrome;
  OSType corevideo;
};

void Increment(int* i) {
  ++(*i);
}

}  // namespace

TEST(VideoFrameMac, CheckBasicAttributes) {
  gfx::Size size(kWidth, kHeight);
  auto frame = VideoFrame::CreateFrame(PIXEL_FORMAT_I420, size, gfx::Rect(size),
                                       size, kTimestamp);
  ASSERT_TRUE(frame.get());

  auto pb = WrapVideoFrameInCVPixelBuffer(*frame);
  ASSERT_TRUE(pb.get());

  gfx::Size coded_size = frame->coded_size();
  VideoPixelFormat format = frame->format();

  EXPECT_EQ(coded_size.width(), static_cast<int>(CVPixelBufferGetWidth(pb)));
  EXPECT_EQ(coded_size.height(), static_cast<int>(CVPixelBufferGetHeight(pb)));
  EXPECT_EQ(VideoFrame::NumPlanes(format), CVPixelBufferGetPlaneCount(pb));

  CVPixelBufferLockBaseAddress(pb, 0);
  for (size_t i = 0; i < VideoFrame::NumPlanes(format); ++i) {
    gfx::Size plane_size = VideoFrame::PlaneSize(format, i, coded_size);
    EXPECT_EQ(plane_size.width(),
              static_cast<int>(CVPixelBufferGetWidthOfPlane(pb, i)));
    EXPECT_EQ(plane_size.height(),
              static_cast<int>(CVPixelBufferGetHeightOfPlane(pb, i)));
    EXPECT_EQ(frame->data(i), CVPixelBufferGetBaseAddressOfPlane(pb, i));
  }
  CVPixelBufferUnlockBaseAddress(pb, 0);
}

TEST(VideoFrameMac, CheckFormats) {
  // CreateFrame() does not support non planar YUV, e.g. NV12.
  const FormatPair format_pairs[] = {
      {PIXEL_FORMAT_I420, kCVPixelFormatType_420YpCbCr8Planar},
      {PIXEL_FORMAT_YV12, 0},
      {PIXEL_FORMAT_YV16, 0},
      {PIXEL_FORMAT_YV12A, 0},
      {PIXEL_FORMAT_YV24, 0},
  };

  gfx::Size size(kWidth, kHeight);
  for (const auto& format_pair : format_pairs) {
    auto frame = VideoFrame::CreateFrame(format_pair.chrome, size,
                                         gfx::Rect(size), size, kTimestamp);
    ASSERT_TRUE(frame.get());
    auto pb = WrapVideoFrameInCVPixelBuffer(*frame);
    if (format_pair.corevideo) {
      EXPECT_EQ(format_pair.corevideo, CVPixelBufferGetPixelFormatType(pb));
    } else {
      EXPECT_EQ(nullptr, pb.get());
    }
  }
}

TEST(VideoFrameMac, CheckLifetime) {
  gfx::Size size(kWidth, kHeight);
  auto frame = VideoFrame::CreateFrame(PIXEL_FORMAT_I420, size, gfx::Rect(size),
                                       size, kTimestamp);
  ASSERT_TRUE(frame.get());

  int instances_destroyed = 0;
  auto wrapper_frame = VideoFrame::WrapVideoFrame(
      frame, frame->visible_rect(), frame->natural_size());
  wrapper_frame->AddDestructionObserver(
      base::Bind(&Increment, &instances_destroyed));
  ASSERT_TRUE(wrapper_frame.get());

  auto pb = WrapVideoFrameInCVPixelBuffer(*wrapper_frame);
  ASSERT_TRUE(pb.get());

  wrapper_frame = nullptr;
  EXPECT_EQ(0, instances_destroyed);
  pb.reset();
  EXPECT_EQ(1, instances_destroyed);
}

TEST(VideoFrameMac, CheckWrapperFrame) {
  const FormatPair format_pairs[] = {
      {PIXEL_FORMAT_I420, kCVPixelFormatType_420YpCbCr8Planar},
      {PIXEL_FORMAT_NV12,
       CoreVideoGlue::kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange},
  };

  gfx::Size size(kWidth, kHeight);
  for (const auto& format_pair : format_pairs) {
    base::ScopedCFTypeRef<CVPixelBufferRef> pb;
    CVPixelBufferCreate(nullptr, kWidth, kHeight, format_pair.corevideo,
                        nullptr, pb.InitializeInto());
    ASSERT_TRUE(pb.get());

    auto frame = VideoFrame::WrapCVPixelBuffer(pb.get(), kTimestamp);
    ASSERT_TRUE(frame.get());
    EXPECT_EQ(pb.get(), frame->cv_pixel_buffer());
    EXPECT_EQ(format_pair.chrome, frame->format());

    frame = nullptr;
    EXPECT_EQ(1, CFGetRetainCount(pb.get()));
  }
}

}  // namespace media
