// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/thread_task_runner_handle.h"
#include "content/child/child_process.h"
#include "content/renderer/media/canvas_capture_handler.h"
#include "content/renderer/media/media_stream_video_capturer_source.h"
#include "media/base/limits.h"
#include "skia/ext/refptr.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"
#include "third_party/WebKit/public/platform/WebSize.h"
#include "third_party/WebKit/public/web/WebHeap.h"

using ::testing::_;
using ::testing::InSequence;
using ::testing::Mock;
using ::testing::SaveArg;
using ::testing::Test;

namespace content {

static const int kTestCanvasCaptureWidth = 320;
static const int kTestCanvasCaptureHeight = 240;
static const double kTestCanvasCaptureFramesPerSecond = 55.5;

static const int kTestCanvasCaptureFrameWidth = 2;
static const int kTestCanvasCaptureFrameHeight = 2;
static const int kTestCanvasCaptureFrameErrorTolerance = 2;

ACTION_P(RunClosure, closure) {
  closure.Run();
}

class CanvasCaptureHandlerTest : public Test {
 public:
  CanvasCaptureHandlerTest() {}

  void SetUp() override {
    canvas_capture_handler_.reset(
        CanvasCaptureHandler::CreateCanvasCaptureHandler(
            blink::WebSize(kTestCanvasCaptureWidth, kTestCanvasCaptureHeight),
            kTestCanvasCaptureFramesPerSecond, message_loop_.task_runner(),
            &track_));
  }

  void TearDown() override {
    track_.reset();
    blink::WebHeap::collectAllGarbageForTesting();
    canvas_capture_handler_.reset();

    // Let the message loop run to finish destroying the capturer.
    base::RunLoop().RunUntilIdle();
  }

  // Necessary callbacks and MOCK_METHODS for VideoCapturerSource.
  MOCK_METHOD2(DoOnDeliverFrame,
               void(const scoped_refptr<media::VideoFrame>&, base::TimeTicks));
  void OnDeliverFrame(const scoped_refptr<media::VideoFrame>& video_frame,
                      base::TimeTicks estimated_capture_time) {
    DoOnDeliverFrame(video_frame, estimated_capture_time);
  }

  MOCK_METHOD1(DoOnVideoCaptureDeviceFormats,
               void(const media::VideoCaptureFormats&));
  void OnVideoCaptureDeviceFormats(const media::VideoCaptureFormats& formats) {
    DoOnVideoCaptureDeviceFormats(formats);
  }

  MOCK_METHOD1(DoOnRunning, void(bool));
  void OnRunning(bool state) { DoOnRunning(state); }

  // Verify returned frames.
  static skia::RefPtr<SkImage> GenerateTestImage() {
    SkBitmap testBitmap;
    testBitmap.allocN32Pixels(kTestCanvasCaptureFrameWidth,
                              kTestCanvasCaptureFrameHeight);
    testBitmap.eraseColor(SK_ColorBLUE);
    return skia::AdoptRef(SkImage::NewFromBitmap(testBitmap));
  }

  void OnVerifyDeliveredFrame(
      const scoped_refptr<media::VideoFrame>& video_frame,
      base::TimeTicks estimated_capture_time) {
    EXPECT_EQ(media::PIXEL_FORMAT_I420, video_frame->format());
    const gfx::Size& size = video_frame->coded_size();
    EXPECT_EQ(kTestCanvasCaptureFrameWidth, size.width());
    EXPECT_EQ(kTestCanvasCaptureFrameHeight, size.height());
    const uint8_t* y_plane = video_frame->data(0);
    EXPECT_NEAR(41, y_plane[0], kTestCanvasCaptureFrameErrorTolerance);
    const uint8_t* u_plane = video_frame->data(1);
    EXPECT_NEAR(239, u_plane[0], kTestCanvasCaptureFrameErrorTolerance);
    const uint8_t* v_plane = video_frame->data(2);
    EXPECT_NEAR(110, v_plane[0], kTestCanvasCaptureFrameErrorTolerance);
  }

  blink::WebMediaStreamTrack track_;
  // The Class under test. Needs to be scoped_ptr to force its destruction.
  scoped_ptr<CanvasCaptureHandler> canvas_capture_handler_;

 protected:
  media::VideoCapturerSource* GetVideoCapturerSource(
      MediaStreamVideoCapturerSource* ms_source) {
    return ms_source->source_.get();
  }

  // A ChildProcess and a MessageLoopForUI are both needed to fool the Tracks
  // and Sources into believing they are on the right threads.
  base::MessageLoopForUI message_loop_;
  ChildProcess child_process_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CanvasCaptureHandlerTest);
};

// Checks that the initialization-destruction sequence works fine.
TEST_F(CanvasCaptureHandlerTest, ConstructAndDestruct) {
  EXPECT_TRUE(canvas_capture_handler_->needsNewFrame());
  base::RunLoop().RunUntilIdle();
}

// Checks that VideoCapturerSource call sequence works fine.
TEST_F(CanvasCaptureHandlerTest, GetFormatsStartAndStop) {
  InSequence s;
  const blink::WebMediaStreamSource& web_media_stream_source = track_.source();
  EXPECT_FALSE(web_media_stream_source.isNull());
  MediaStreamVideoCapturerSource* const ms_source =
      static_cast<MediaStreamVideoCapturerSource*>(
          web_media_stream_source.extraData());
  EXPECT_TRUE(ms_source != nullptr);
  media::VideoCapturerSource* source = GetVideoCapturerSource(ms_source);
  EXPECT_TRUE(source != nullptr);

  media::VideoCaptureFormats formats;
  EXPECT_CALL(*this, DoOnVideoCaptureDeviceFormats(_))
      .Times(1)
      .WillOnce(SaveArg<0>(&formats));
  source->GetCurrentSupportedFormats(
      media::limits::kMaxCanvas /* max_requesteed_width */,
      media::limits::kMaxCanvas /* max_requesteed_height */,
      media::limits::kMaxFramesPerSecond /* max_requested_frame_rate */,
      base::Bind(&CanvasCaptureHandlerTest::OnVideoCaptureDeviceFormats,
                 base::Unretained(this)));
  ASSERT_EQ(1u, formats.size());
  EXPECT_EQ(kTestCanvasCaptureWidth, formats[0].frame_size.width());
  EXPECT_EQ(kTestCanvasCaptureHeight, formats[0].frame_size.height());
  media::VideoCaptureParams params;
  params.requested_format = formats[0];

  base::RunLoop run_loop;
  base::Closure quit_closure = run_loop.QuitClosure();
  EXPECT_CALL(*this, DoOnRunning(true)).Times(1);
  EXPECT_CALL(*this, DoOnDeliverFrame(_, _))
      .Times(1)
      .WillOnce(RunClosure(quit_closure));
  source->StartCapture(
      params, base::Bind(&CanvasCaptureHandlerTest::OnDeliverFrame,
                         base::Unretained(this)),
      base::Bind(&CanvasCaptureHandlerTest::OnRunning, base::Unretained(this)));
  canvas_capture_handler_->sendNewFrame(GenerateTestImage().get());
  run_loop.Run();

  source->StopCapture();
}

// Verifies that SkImage is processed and produces VideoFrame as expected.
TEST_F(CanvasCaptureHandlerTest, VerifyFrame) {
  InSequence s;
  media::VideoCapturerSource* const source =
      GetVideoCapturerSource(static_cast<MediaStreamVideoCapturerSource*>(
          track_.source().extraData()));
  EXPECT_TRUE(source != nullptr);

  base::RunLoop run_loop;
  EXPECT_CALL(*this, DoOnRunning(true)).Times(1);
  media::VideoCaptureParams params;
  source->StartCapture(
      params, base::Bind(&CanvasCaptureHandlerTest::OnVerifyDeliveredFrame,
                         base::Unretained(this)),
      base::Bind(&CanvasCaptureHandlerTest::OnRunning, base::Unretained(this)));
  canvas_capture_handler_->sendNewFrame(GenerateTestImage().get());
  run_loop.RunUntilIdle();
}

// Checks that needsNewFrame() works as expected.
TEST_F(CanvasCaptureHandlerTest, CheckNeedsNewFrame) {
  InSequence s;
  media::VideoCapturerSource* source =
      GetVideoCapturerSource(static_cast<MediaStreamVideoCapturerSource*>(
          track_.source().extraData()));
  EXPECT_TRUE(source != nullptr);
  EXPECT_TRUE(canvas_capture_handler_->needsNewFrame());
  source->StopCapture();
  EXPECT_FALSE(canvas_capture_handler_->needsNewFrame());
}

}  // namespace content
