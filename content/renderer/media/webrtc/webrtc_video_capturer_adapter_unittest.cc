// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/bind.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "content/child/child_process.h"
#include "content/renderer/media/webrtc/webrtc_video_capturer_adapter.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "media/base/video_frame.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
static void ReleaseMailboxCB(const gpu::SyncToken& sync_token) {}
}  // anonymous namespace

namespace content {

class WebRtcVideoCapturerAdapterTest
    : public rtc::VideoSinkInterface<webrtc::VideoFrame>,
      public ::testing::Test {
 public:
  WebRtcVideoCapturerAdapterTest()
      : adapter_(new WebRtcVideoCapturerAdapter(
            false,
            blink::WebMediaStreamTrack::ContentHintType::None)),
        output_frame_width_(0),
        output_frame_height_(0) {
    adapter_->AddOrUpdateSink(this, rtc::VideoSinkWants());
  }
  ~WebRtcVideoCapturerAdapterTest() override { adapter_->RemoveSink(this); }

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
    adapter_->OnFrameCaptured(frame);
    EXPECT_EQ(natural_width, output_frame_width_);
    EXPECT_EQ(natural_height, output_frame_height_);
  }

  void TestSourceTextureFrame() {
    EXPECT_TRUE(message_loop_.IsCurrent());
    gpu::MailboxHolder holders[media::VideoFrame::kMaxPlanes] = {
        gpu::MailboxHolder(gpu::Mailbox::Generate(), gpu::SyncToken(), 5)};
    scoped_refptr<media::VideoFrame> frame =
        media::VideoFrame::WrapNativeTextures(
            media::PIXEL_FORMAT_ARGB, holders, base::Bind(&ReleaseMailboxCB),
            gfx::Size(10, 10), gfx::Rect(10, 10), gfx::Size(10, 10),
            base::TimeDelta());
    adapter_->OnFrameCaptured(frame);
    ASSERT_TRUE(output_frame_);
    rtc::scoped_refptr<webrtc::VideoFrameBuffer> texture_frame =
        output_frame_->video_frame_buffer();
    EXPECT_EQ(media::VideoFrame::STORAGE_OPAQUE,
              static_cast<media::VideoFrame*>(texture_frame->native_handle())
                  ->storage_type());

    rtc::scoped_refptr<webrtc::VideoFrameBuffer> copied_frame =
        texture_frame->NativeToI420Buffer();
    EXPECT_TRUE(copied_frame);
    EXPECT_TRUE(copied_frame->DataY());
    EXPECT_TRUE(copied_frame->DataU());
    EXPECT_TRUE(copied_frame->DataV());
  }

  // rtc::VideoSinkInterface
  void OnFrame(const webrtc::VideoFrame& frame) override {
    output_frame_ = base::Optional<webrtc::VideoFrame>(frame);
    output_frame_width_ = frame.width();
    output_frame_height_ = frame.height();
  }

  void TestContentHintResolutionAdaptation(
      bool is_screencast,
      blink::WebMediaStreamTrack::ContentHintType construction_content_hint,
      bool expect_initial_downscale,
      blink::WebMediaStreamTrack::ContentHintType set_content_hint,
      bool expect_final_downscale) {
    // Reset and configure adapter to the test.
    adapter_->RemoveSink(this);
    adapter_.reset(new WebRtcVideoCapturerAdapter(is_screencast,
                                                  construction_content_hint));

    const int kInputWidth = 1280;
    const int kInputHeight = 720;
    const gfx::Size kSize(kInputWidth, kInputHeight);
    scoped_refptr<media::VideoFrame> frame = media::VideoFrame::CreateFrame(
        media::PIXEL_FORMAT_I420, kSize, gfx::Rect(kSize), kSize,
        base::TimeDelta());

    // Request smaller scale to make sure scaling normally kicks in.
    rtc::VideoSinkWants wants;
    wants.max_pixel_count = rtc::Optional<int>(kInputWidth * kInputHeight / 2);
    adapter_->AddOrUpdateSink(this, wants);

    adapter_->OnFrameCaptured(frame);
    if (expect_initial_downscale) {
      EXPECT_LT(output_frame_width_, kInputWidth);
      EXPECT_LT(output_frame_height_, kInputHeight);
    } else {
      EXPECT_EQ(kInputWidth, output_frame_width_);
      EXPECT_EQ(kInputHeight, output_frame_height_);
    }

    adapter_->SetContentHint(set_content_hint);
    adapter_->OnFrameCaptured(frame);
    if (expect_final_downscale) {
      EXPECT_LT(output_frame_width_, kInputWidth);
      EXPECT_LT(output_frame_height_, kInputHeight);
    } else {
      EXPECT_EQ(kInputWidth, output_frame_width_);
      EXPECT_EQ(kInputHeight, output_frame_height_);
    }
  }

 private:
  const base::MessageLoopForIO message_loop_;
  const ChildProcess child_process_;

  std::unique_ptr<WebRtcVideoCapturerAdapter> adapter_;
  base::Optional<webrtc::VideoFrame> output_frame_;
  int output_frame_width_;
  int output_frame_height_;
};

TEST_F(WebRtcVideoCapturerAdapterTest, CropFrameTo640360) {
  TestSourceCropFrame(640, 480, 640, 360, 640, 360);
}

TEST_F(WebRtcVideoCapturerAdapterTest, CropFrameTo320320) {
  TestSourceCropFrame(640, 480, 480, 480, 320, 320);
}

TEST_F(WebRtcVideoCapturerAdapterTest, Scale720To640360) {
  TestSourceCropFrame(1280, 720, 1280, 720, 640, 360);
}

TEST_F(WebRtcVideoCapturerAdapterTest, SendsWithCopyTextureFrameCallback) {
  TestSourceTextureFrame();
}

TEST_F(WebRtcVideoCapturerAdapterTest,
       NonScreencastAdapterDoesNotAdaptContentHintDetail) {
  // Non-screenshare adapter should not adapt frames when detail is set.
  TestContentHintResolutionAdaptation(
      false, blink::WebMediaStreamTrack::ContentHintType::None, true,
      blink::WebMediaStreamTrack::ContentHintType::VideoDetail, false);
}

TEST_F(WebRtcVideoCapturerAdapterTest,
       NonScreencastAdapterAdaptsContentHintFluid) {
  // Non-screenshare adapter should still adapt frames when motion is set.
  TestContentHintResolutionAdaptation(
      false, blink::WebMediaStreamTrack::ContentHintType::None, true,
      blink::WebMediaStreamTrack::ContentHintType::VideoMotion, true);
}

TEST_F(WebRtcVideoCapturerAdapterTest,
       ScreencastAdapterAdaptsContentHintFluid) {
  // Screenshare adapter should adapt frames when motion is set.
  TestContentHintResolutionAdaptation(
      true, blink::WebMediaStreamTrack::ContentHintType::None, false,
      blink::WebMediaStreamTrack::ContentHintType::VideoMotion, true);
}

TEST_F(WebRtcVideoCapturerAdapterTest,
       ScreencastAdapterDoesNotAdaptContentHintDetailed) {
  // Screenshare adapter should still not adapt frames when detail is set.
  TestContentHintResolutionAdaptation(
      true, blink::WebMediaStreamTrack::ContentHintType::None, false,
      blink::WebMediaStreamTrack::ContentHintType::VideoDetail, false);
}

TEST_F(WebRtcVideoCapturerAdapterTest, RespectsConstructionTimeContentHint) {
  // Non-screenshare adapter constructed with detail content hint should not
  // adapt before SetContentHint is run.
  TestContentHintResolutionAdaptation(
      false, blink::WebMediaStreamTrack::ContentHintType::VideoDetail, false,
      blink::WebMediaStreamTrack::ContentHintType::VideoMotion, true);
}

}  // namespace content
