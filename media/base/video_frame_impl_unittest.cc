// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/video_frame_impl.h"

#include "base/format_macros.h"
#include "base/string_util.h"
#include "media/base/buffers.h"
#include "media/base/mock_filters.h"
#include "media/base/yuv_convert.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

// Helper function that initializes a YV12 frame with white and black scan
// lines based on the |white_to_black| parameter.  If 0, then the entire
// frame will be black, if 1 then the entire frame will be white.
void InitializeYV12Frame(VideoFrame* frame, double white_to_black) {
  VideoSurface surface;
  if (!frame->Lock(&surface)) {
    ADD_FAILURE();
    return;
  }
  EXPECT_EQ(VideoSurface::YV12, surface.format);
  size_t first_black_row = static_cast<size_t>(surface.height * white_to_black);
  uint8* y_plane = surface.data[VideoSurface::kYPlane];
  for (size_t row = 0; row < surface.height; ++row) {
    int color = (row < first_black_row) ? 0xFF : 0x00;
    memset(y_plane, color, surface.width);
    y_plane += surface.strides[VideoSurface::kYPlane];
  }
  uint8* u_plane = surface.data[VideoSurface::kUPlane];
  uint8* v_plane = surface.data[VideoSurface::kVPlane];
  for (size_t row = 0; row < surface.height; row += 2) {
    memset(u_plane, 0x80, surface.width / 2);
    memset(v_plane, 0x80, surface.width / 2);
    u_plane += surface.strides[VideoSurface::kUPlane];
    v_plane += surface.strides[VideoSurface::kVPlane];
  }
  frame->Unlock();
}

// Given a |yv12_frame| this method converts the YV12 frame to RGBA and
// makes sure that all the pixels of the RBG frame equal |expect_rgb_color|.
void ExpectFrameColor(media::VideoFrame* yv12_frame, uint32 expect_rgb_color) {
  // On linux and mac builds if you directly compare using EXPECT_EQ and use
  // the VideoSurface::kNumxxxPlanes constants, it generates an error when
  // linking.  These are declared so that we can compare against locals.
  const size_t expect_yuv_planes = VideoSurface::kNumYUVPlanes;
  const size_t expect_rgb_planes = VideoSurface::kNumRGBPlanes;

  VideoSurface yuv_surface;
  ASSERT_TRUE(yv12_frame->Lock(&yuv_surface));
  ASSERT_EQ(VideoSurface::YV12, yuv_surface.format);
  ASSERT_EQ(expect_yuv_planes, yuv_surface.planes);
  ASSERT_EQ(yuv_surface.strides[VideoSurface::kUPlane],
            yuv_surface.strides[VideoSurface::kVPlane]);

  scoped_refptr<media::VideoFrame> rgb_frame;
  media::VideoFrameImpl::CreateFrame(VideoSurface::RGBA,
                                     yuv_surface.width,
                                     yuv_surface.height,
                                     yv12_frame->GetTimestamp(),
                                     yv12_frame->GetDuration(),
                                     &rgb_frame);
  media::VideoSurface rgb_surface;
  ASSERT_TRUE(rgb_frame->Lock(&rgb_surface));
  ASSERT_EQ(yuv_surface.width, rgb_surface.width);
  ASSERT_EQ(yuv_surface.height, rgb_surface.height);
  ASSERT_EQ(expect_rgb_planes, rgb_surface.planes);

  media::ConvertYUVToRGB32(yuv_surface.data[VideoSurface::kYPlane],
                           yuv_surface.data[VideoSurface::kUPlane],
                           yuv_surface.data[VideoSurface::kVPlane],
                           rgb_surface.data[VideoSurface::kRGBPlane],
                           rgb_surface.width,
                           rgb_surface.height,
                           yuv_surface.strides[VideoSurface::kYPlane],
                           yuv_surface.strides[VideoSurface::kUPlane],
                           rgb_surface.strides[VideoSurface::kRGBPlane],
                           media::YV12);

  for (size_t row = 0; row < rgb_surface.height; ++row) {
    uint32* rgb_row_data = reinterpret_cast<uint32*>(
        rgb_surface.data[VideoSurface::kRGBPlane] +
        (rgb_surface.strides[VideoSurface::kRGBPlane] * row));
    for (size_t col = 0; col < rgb_surface.width; ++col) {
      SCOPED_TRACE(StringPrintf("Checking (%" PRIuS ", %" PRIuS ")", row, col));
      EXPECT_EQ(expect_rgb_color, rgb_row_data[col]);
    }
  }
  rgb_frame->Unlock();
  yv12_frame->Unlock();
}

TEST(VideoFrameImpl, CreateFrame) {
  const size_t kWidth = 64;
  const size_t kHeight = 48;
  const base::TimeDelta kTimestampA = base::TimeDelta::FromMicroseconds(1337);
  const base::TimeDelta kDurationA = base::TimeDelta::FromMicroseconds(1667);
  const base::TimeDelta kTimestampB = base::TimeDelta::FromMicroseconds(1234);
  const base::TimeDelta kDurationB = base::TimeDelta::FromMicroseconds(5678);

  // Create a YV12 Video Frame.
  scoped_refptr<media::VideoFrame> frame;
  VideoFrameImpl::CreateFrame(media::VideoSurface::YV12, kWidth, kHeight,
                              kTimestampA, kDurationA, &frame);
  ASSERT_TRUE(frame);

  // Test StreamSample implementation.
  EXPECT_EQ(kTimestampA.InMicroseconds(),
            frame->GetTimestamp().InMicroseconds());
  EXPECT_EQ(kDurationA.InMicroseconds(), frame->GetDuration().InMicroseconds());
  EXPECT_FALSE(frame->IsEndOfStream());
  EXPECT_FALSE(frame->IsDiscontinuous());
  frame->SetTimestamp(kTimestampB);
  frame->SetDuration(kDurationB);
  EXPECT_EQ(kTimestampB.InMicroseconds(),
            frame->GetTimestamp().InMicroseconds());
  EXPECT_EQ(kDurationB.InMicroseconds(), frame->GetDuration().InMicroseconds());
  EXPECT_FALSE(frame->IsEndOfStream());
  frame->SetDiscontinuous(true);
  EXPECT_TRUE(frame->IsDiscontinuous());
  frame->SetDiscontinuous(false);
  EXPECT_FALSE(frame->IsDiscontinuous());

  // Test VideoFrame implementation.
  {
    SCOPED_TRACE("");
    InitializeYV12Frame(frame, 0.0f);
    ExpectFrameColor(frame, 0xFF000000);
  }
  {
    SCOPED_TRACE("");
    InitializeYV12Frame(frame, 1.0f);
    ExpectFrameColor(frame, 0xFFFFFFFF);
  }

  // Test an empty frame.
  VideoFrameImpl::CreateEmptyFrame(&frame);
  EXPECT_TRUE(frame->IsEndOfStream());
}

TEST(VideoFrameImpl, CreateBlackFrame) {
  const size_t kWidth = 2;
  const size_t kHeight = 2;
  const uint8 kExpectedYRow[] = { 0, 0 };
  const uint8 kExpectedUVRow[] = { 128 };

  scoped_refptr<media::VideoFrame> frame;
  VideoFrameImpl::CreateBlackFrame(kWidth, kHeight, &frame);
  ASSERT_TRUE(frame);

  // Test basic properties.
  EXPECT_EQ(0, frame->GetTimestamp().InMicroseconds());
  EXPECT_EQ(0, frame->GetDuration().InMicroseconds());
  EXPECT_FALSE(frame->IsEndOfStream());

  // Test surface properties.
  VideoSurface surface;
  EXPECT_TRUE(frame->Lock(&surface));
  EXPECT_EQ(VideoSurface::YV12, surface.format);
  EXPECT_EQ(kWidth, surface.width);
  EXPECT_EQ(kHeight, surface.height);
  EXPECT_EQ(3u, surface.planes);

  // Test surfaces themselves.
  for (size_t y = 0; y < surface.height; ++y) {
    EXPECT_EQ(0, memcmp(kExpectedYRow, surface.data[VideoSurface::kYPlane],
                        arraysize(kExpectedYRow)));
    surface.data[VideoSurface::kYPlane] +=
        surface.strides[VideoSurface::kYPlane];
  }
  for (size_t y = 0; y < surface.height / 2; ++y) {
    EXPECT_EQ(0, memcmp(kExpectedUVRow, surface.data[VideoSurface::kUPlane],
                        arraysize(kExpectedUVRow)));
    EXPECT_EQ(0, memcmp(kExpectedUVRow, surface.data[VideoSurface::kVPlane],
                        arraysize(kExpectedUVRow)));
    surface.data[VideoSurface::kUPlane] +=
        surface.strides[VideoSurface::kUPlane];
    surface.data[VideoSurface::kVPlane] +=
        surface.strides[VideoSurface::kVPlane];
  }
}

}  // namespace media
