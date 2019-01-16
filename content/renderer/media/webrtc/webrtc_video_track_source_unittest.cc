// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/single_thread_task_runner.h"
#include "base/test/scoped_task_environment.h"
#include "content/renderer/media/webrtc/webrtc_video_frame_adapter.h"
#include "content/renderer/media/webrtc/webrtc_video_track_source.h"
#include "media/base/video_frame.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/rtc_base/ref_counted_object.h"

namespace content {

class WebRtcVideoTrackSourceTest
    : public rtc::VideoSinkInterface<webrtc::VideoFrame>,
      public ::testing::Test {
 public:
  WebRtcVideoTrackSourceTest()
      : track_source_(new rtc::RefCountedObject<WebRtcVideoTrackSource>(
            /*is_screencast=*/false,
            /*needs_denoising=*/absl::nullopt)),
        output_frame_width_(0),
        output_frame_height_(0) {
    track_source_->AddOrUpdateSink(this, rtc::VideoSinkWants());
  }
  ~WebRtcVideoTrackSourceTest() override { track_source_->RemoveSink(this); }

  void TestSourceCropFrame(int capture_width,
                           int capture_height,
                           int cropped_width,
                           int cropped_height,
                           int natural_width,
                           int natural_height) {
    const int horiz_crop = ((capture_width - cropped_width) / 2);
    const int vert_crop = ((capture_height - cropped_height) / 2);

    gfx::Size coded_size(capture_width, capture_height);
    gfx::Size natural_size(natural_width, natural_height);
    gfx::Rect view_rect(horiz_crop, vert_crop, cropped_width, cropped_height);
    scoped_refptr<media::VideoFrame> frame = media::VideoFrame::CreateFrame(
        media::PIXEL_FORMAT_I420, coded_size, view_rect, natural_size,
        base::TimeDelta());
    track_source_->OnFrameCaptured(frame);
    EXPECT_EQ(natural_width, output_frame_width_);
    EXPECT_EQ(natural_height, output_frame_height_);
  }

  // rtc::VideoSinkInterface
  void OnFrame(const webrtc::VideoFrame& frame) override {
    output_frame_width_ = frame.width();
    output_frame_height_ = frame.height();
  }

 private:
  scoped_refptr<WebRtcVideoTrackSource> track_source_;
  int output_frame_width_;
  int output_frame_height_;
};

TEST_F(WebRtcVideoTrackSourceTest, CropFrameTo640360) {
  TestSourceCropFrame(640, 480, 640, 360, 640, 360);
}

TEST_F(WebRtcVideoTrackSourceTest, CropFrameTo320320) {
  TestSourceCropFrame(640, 480, 480, 480, 320, 320);
}

TEST_F(WebRtcVideoTrackSourceTest, Scale720To640360) {
  TestSourceCropFrame(1280, 720, 1280, 720, 640, 360);
}

}  // namespace content
