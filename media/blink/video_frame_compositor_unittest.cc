// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/test/simple_test_tick_clock.h"
#include "cc/layers/video_frame_provider.h"
#include "media/base/video_frame.h"
#include "media/blink/video_frame_compositor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::DoAll;
using testing::Return;

namespace media {

ACTION_P(RunClosure, closure) {
  closure.Run();
}

class VideoFrameCompositorTest : public testing::Test,
                                 public cc::VideoFrameProvider::Client,
                                 public VideoRendererSink::RenderCallback {
 public:
  VideoFrameCompositorTest()
      : tick_clock_(new base::SimpleTestTickClock()),
        compositor_(new VideoFrameCompositor(
            message_loop.task_runner(),
            base::Bind(&VideoFrameCompositorTest::NaturalSizeChanged,
                       base::Unretained(this)),
            base::Bind(&VideoFrameCompositorTest::OpacityChanged,
                       base::Unretained(this)))),
        did_receive_frame_count_(0),
        natural_size_changed_count_(0),
        opacity_changed_count_(0),
        opaque_(false) {
    compositor_->SetVideoFrameProviderClient(this);
    compositor_->set_tick_clock_for_testing(
        std::unique_ptr<base::TickClock>(tick_clock_));
    // Disable background rendering by default.
    compositor_->set_background_rendering_for_testing(false);
  }

  ~VideoFrameCompositorTest() override {
    compositor_->SetVideoFrameProviderClient(nullptr);
  }

  scoped_refptr<VideoFrame> CreateOpaqueFrame() {
    gfx::Size size(8, 8);
    return VideoFrame::CreateFrame(PIXEL_FORMAT_YV12, size, gfx::Rect(size),
                                   size, base::TimeDelta());
  }

  VideoFrameCompositor* compositor() { return compositor_.get(); }
  int did_receive_frame_count() { return did_receive_frame_count_; }
  int natural_size_changed_count() { return natural_size_changed_count_; }
  gfx::Size natural_size() { return natural_size_; }

  int opacity_changed_count() { return opacity_changed_count_; }
  bool opaque() { return opaque_; }

 protected:
  // cc::VideoFrameProvider::Client implementation.
  void StopUsingProvider() override {}
  MOCK_METHOD0(StartRendering, void());
  MOCK_METHOD0(StopRendering, void());
  void DidReceiveFrame() override { ++did_receive_frame_count_; }

  // VideoRendererSink::RenderCallback implementation.
  MOCK_METHOD3(Render,
               scoped_refptr<VideoFrame>(base::TimeTicks,
                                         base::TimeTicks,
                                         bool));
  MOCK_METHOD0(OnFrameDropped, void());

  void NaturalSizeChanged(gfx::Size natural_size) {
    ++natural_size_changed_count_;
    natural_size_ = natural_size;
  }

  void OpacityChanged(bool opaque) {
    ++opacity_changed_count_;
    opaque_ = opaque;
  }

  void StartVideoRendererSink() {
    EXPECT_CALL(*this, StartRendering());
    const bool had_current_frame = !!compositor_->GetCurrentFrame();
    compositor()->Start(this);
    // If we previously had a frame, we should still have one now.
    EXPECT_EQ(had_current_frame, !!compositor_->GetCurrentFrame());
    message_loop.RunUntilIdle();
  }

  void StopVideoRendererSink(bool have_client) {
    if (have_client)
      EXPECT_CALL(*this, StopRendering());
    const bool had_current_frame = !!compositor_->GetCurrentFrame();
    compositor()->Stop();
    // If we previously had a frame, we should still have one now.
    EXPECT_EQ(had_current_frame, !!compositor_->GetCurrentFrame());
    message_loop.RunUntilIdle();
  }

  void RenderFrame() {
    compositor()->GetCurrentFrame();
    compositor()->PutCurrentFrame();
  }

  base::MessageLoop message_loop;
  base::SimpleTestTickClock* tick_clock_;  // Owned by |compositor_|
  std::unique_ptr<VideoFrameCompositor> compositor_;

  int did_receive_frame_count_;
  int natural_size_changed_count_;
  gfx::Size natural_size_;
  int opacity_changed_count_;
  bool opaque_;

  DISALLOW_COPY_AND_ASSIGN(VideoFrameCompositorTest);
};

TEST_F(VideoFrameCompositorTest, InitialValues) {
  EXPECT_FALSE(compositor()->GetCurrentFrame().get());
}

TEST_F(VideoFrameCompositorTest, PaintFrameUsingOldRenderingPath) {
  scoped_refptr<VideoFrame> expected = VideoFrame::CreateEOSFrame();

  // Should notify compositor synchronously.
  EXPECT_EQ(0, did_receive_frame_count());
  compositor()->PaintFrameUsingOldRenderingPath(expected);
  scoped_refptr<VideoFrame> actual = compositor()->GetCurrentFrame();
  EXPECT_EQ(expected, actual);
  EXPECT_EQ(1, did_receive_frame_count());
}

TEST_F(VideoFrameCompositorTest, NaturalSizeChanged) {
  gfx::Size initial_size(8, 8);
  scoped_refptr<VideoFrame> initial_frame =
      VideoFrame::CreateBlackFrame(initial_size);

  gfx::Size larger_size(16, 16);
  scoped_refptr<VideoFrame> larger_frame =
      VideoFrame::CreateBlackFrame(larger_size);

  gfx::Size empty_size(0, 0);

  // Initial expectations.
  EXPECT_EQ(empty_size, natural_size());
  EXPECT_EQ(0, natural_size_changed_count());

  // Callback is fired for the first frame.
  compositor()->PaintFrameUsingOldRenderingPath(initial_frame);
  EXPECT_EQ(initial_size, natural_size());
  EXPECT_EQ(1, natural_size_changed_count());

  // Callback should be fired once.
  compositor()->PaintFrameUsingOldRenderingPath(larger_frame);
  EXPECT_EQ(larger_size, natural_size());
  EXPECT_EQ(2, natural_size_changed_count());

  compositor()->PaintFrameUsingOldRenderingPath(larger_frame);
  EXPECT_EQ(larger_size, natural_size());
  EXPECT_EQ(2, natural_size_changed_count());

  // Callback is fired once more when switching back to initial size.
  compositor()->PaintFrameUsingOldRenderingPath(initial_frame);
  EXPECT_EQ(initial_size, natural_size());
  EXPECT_EQ(3, natural_size_changed_count());

  compositor()->PaintFrameUsingOldRenderingPath(initial_frame);
  EXPECT_EQ(initial_size, natural_size());
  EXPECT_EQ(3, natural_size_changed_count());

  natural_size_changed_count_ = 0;
  natural_size_ = empty_size;
  compositor()->clear_current_frame_for_testing();

  EXPECT_CALL(*this, Render(_, _, _))
      .WillOnce(Return(initial_frame))
      .WillOnce(Return(larger_frame))
      .WillOnce(Return(initial_frame))
      .WillOnce(Return(initial_frame));
  StartVideoRendererSink();

  // Starting the sink will issue one Render() call, ensure the callback is
  // fired for the first frame.
  EXPECT_EQ(1, natural_size_changed_count());
  EXPECT_EQ(initial_size, natural_size());

  // Once another frame is received with a different size it should fire.
  EXPECT_TRUE(
      compositor()->UpdateCurrentFrame(base::TimeTicks(), base::TimeTicks()));
  RenderFrame();
  EXPECT_EQ(larger_size, natural_size());
  EXPECT_EQ(2, natural_size_changed_count());

  EXPECT_TRUE(
      compositor()->UpdateCurrentFrame(base::TimeTicks(), base::TimeTicks()));
  RenderFrame();
  EXPECT_EQ(initial_size, natural_size());
  EXPECT_EQ(3, natural_size_changed_count());

  EXPECT_FALSE(
      compositor()->UpdateCurrentFrame(base::TimeTicks(), base::TimeTicks()));
  EXPECT_EQ(initial_size, natural_size());
  EXPECT_EQ(3, natural_size_changed_count());
  RenderFrame();

  StopVideoRendererSink(true);
}

TEST_F(VideoFrameCompositorTest, OpacityChanged) {
  gfx::Size size(8, 8);
  scoped_refptr<VideoFrame> opaque_frame = CreateOpaqueFrame();
  scoped_refptr<VideoFrame> not_opaque_frame = VideoFrame::CreateFrame(
      PIXEL_FORMAT_YV12A, size, gfx::Rect(size), size, base::TimeDelta());

  // Initial expectations.
  EXPECT_FALSE(opaque());
  EXPECT_EQ(0, opacity_changed_count());

  // Callback is fired for the first frame.
  compositor()->PaintFrameUsingOldRenderingPath(not_opaque_frame);
  EXPECT_FALSE(opaque());
  EXPECT_EQ(1, opacity_changed_count());

  // Callback shouldn't be first subsequent times with same opaqueness.
  compositor()->PaintFrameUsingOldRenderingPath(not_opaque_frame);
  EXPECT_FALSE(opaque());
  EXPECT_EQ(1, opacity_changed_count());

  // Callback is fired when using opacity changes.
  compositor()->PaintFrameUsingOldRenderingPath(opaque_frame);
  EXPECT_TRUE(opaque());
  EXPECT_EQ(2, opacity_changed_count());

  // Callback shouldn't be first subsequent times with same opaqueness.
  compositor()->PaintFrameUsingOldRenderingPath(opaque_frame);
  EXPECT_TRUE(opaque());
  EXPECT_EQ(2, opacity_changed_count());

  opacity_changed_count_ = 0;
  compositor()->clear_current_frame_for_testing();

  EXPECT_CALL(*this, Render(_, _, _))
      .WillOnce(Return(not_opaque_frame))
      .WillOnce(Return(not_opaque_frame))
      .WillOnce(Return(opaque_frame))
      .WillOnce(Return(opaque_frame));
  StartVideoRendererSink();
  EXPECT_FALSE(opaque());
  EXPECT_EQ(1, opacity_changed_count());

  EXPECT_TRUE(
      compositor()->UpdateCurrentFrame(base::TimeTicks(), base::TimeTicks()));
  RenderFrame();
  EXPECT_FALSE(opaque());
  EXPECT_EQ(1, opacity_changed_count());

  EXPECT_TRUE(
      compositor()->UpdateCurrentFrame(base::TimeTicks(), base::TimeTicks()));
  RenderFrame();
  EXPECT_TRUE(opaque());
  EXPECT_EQ(2, opacity_changed_count());

  EXPECT_FALSE(
      compositor()->UpdateCurrentFrame(base::TimeTicks(), base::TimeTicks()));
  EXPECT_TRUE(opaque());
  EXPECT_EQ(2, opacity_changed_count());
  RenderFrame();

  StopVideoRendererSink(true);
}

TEST_F(VideoFrameCompositorTest, VideoRendererSinkFrameDropped) {
  scoped_refptr<VideoFrame> opaque_frame = CreateOpaqueFrame();

  EXPECT_CALL(*this, Render(_, _, _)).WillRepeatedly(Return(opaque_frame));
  StartVideoRendererSink();

  EXPECT_TRUE(
      compositor()->UpdateCurrentFrame(base::TimeTicks(), base::TimeTicks()));

  // Another call should trigger a dropped frame callback.
  EXPECT_CALL(*this, OnFrameDropped());
  EXPECT_FALSE(
      compositor()->UpdateCurrentFrame(base::TimeTicks(), base::TimeTicks()));

  // Ensure it always happens until the frame is rendered.
  EXPECT_CALL(*this, OnFrameDropped());
  EXPECT_FALSE(
      compositor()->UpdateCurrentFrame(base::TimeTicks(), base::TimeTicks()));

  // Call GetCurrentFrame() but not PutCurrentFrame()
  compositor()->GetCurrentFrame();

  // The frame should still register as dropped until PutCurrentFrame is called.
  EXPECT_CALL(*this, OnFrameDropped());
  EXPECT_FALSE(
      compositor()->UpdateCurrentFrame(base::TimeTicks(), base::TimeTicks()));

  RenderFrame();
  EXPECT_FALSE(
      compositor()->UpdateCurrentFrame(base::TimeTicks(), base::TimeTicks()));

  StopVideoRendererSink(true);
}

TEST_F(VideoFrameCompositorTest, VideoLayerShutdownWhileRendering) {
  EXPECT_CALL(*this, Render(_, _, true)).WillOnce(Return(nullptr));
  StartVideoRendererSink();
  compositor_->SetVideoFrameProviderClient(nullptr);
  StopVideoRendererSink(false);
}

TEST_F(VideoFrameCompositorTest, StartFiresBackgroundRender) {
  scoped_refptr<VideoFrame> opaque_frame = CreateOpaqueFrame();
  EXPECT_CALL(*this, Render(_, _, true)).WillRepeatedly(Return(opaque_frame));
  StartVideoRendererSink();
  StopVideoRendererSink(true);
}

TEST_F(VideoFrameCompositorTest, BackgroundRenderTicks) {
  scoped_refptr<VideoFrame> opaque_frame = CreateOpaqueFrame();
  compositor_->set_background_rendering_for_testing(true);

  base::RunLoop run_loop;
  EXPECT_CALL(*this, Render(_, _, true))
      .WillOnce(Return(opaque_frame))
      .WillOnce(
          DoAll(RunClosure(run_loop.QuitClosure()), Return(opaque_frame)));
  StartVideoRendererSink();
  run_loop.Run();

  // UpdateCurrentFrame() calls should indicate they are not synthetic.
  EXPECT_CALL(*this, Render(_, _, false)).WillOnce(Return(opaque_frame));
  EXPECT_FALSE(
      compositor()->UpdateCurrentFrame(base::TimeTicks(), base::TimeTicks()));

  // Background rendering should tick another render callback.
  StopVideoRendererSink(true);
}

TEST_F(VideoFrameCompositorTest,
       UpdateCurrentFrameWorksWhenBackgroundRendered) {
  scoped_refptr<VideoFrame> opaque_frame = CreateOpaqueFrame();
  compositor_->set_background_rendering_for_testing(true);

  // Background render a frame that succeeds immediately.
  EXPECT_CALL(*this, Render(_, _, true)).WillOnce(Return(opaque_frame));
  StartVideoRendererSink();

  // The background render completes immediately, so the next call to
  // UpdateCurrentFrame is expected to return true to account for the frame
  // rendered in the background.
  EXPECT_CALL(*this, Render(_, _, false))
      .WillOnce(Return(scoped_refptr<VideoFrame>(opaque_frame)));
  EXPECT_TRUE(
      compositor()->UpdateCurrentFrame(base::TimeTicks(), base::TimeTicks()));
  RenderFrame();

  // Second call to UpdateCurrentFrame will return false as no new frame has
  // been created since the last call.
  EXPECT_CALL(*this, Render(_, _, false))
      .WillOnce(Return(scoped_refptr<VideoFrame>(opaque_frame)));
  EXPECT_FALSE(
      compositor()->UpdateCurrentFrame(base::TimeTicks(), base::TimeTicks()));

  StopVideoRendererSink(true);
}

TEST_F(VideoFrameCompositorTest, GetCurrentFrameAndUpdateIfStale) {
  scoped_refptr<VideoFrame> opaque_frame_1 = CreateOpaqueFrame();
  scoped_refptr<VideoFrame> opaque_frame_2 = CreateOpaqueFrame();
  compositor_->set_background_rendering_for_testing(true);

  // |current_frame_| should be null at this point since we don't have a client
  // or a callback.
  ASSERT_FALSE(compositor()->GetCurrentFrameAndUpdateIfStale());

  // Starting the video renderer should return a single frame.
  EXPECT_CALL(*this, Render(_, _, true)).WillOnce(Return(opaque_frame_1));
  StartVideoRendererSink();

  // Since we have a client, this call should not call background render, even
  // if a lot of time has elapsed between calls.
  tick_clock_->Advance(base::TimeDelta::FromSeconds(1));
  ASSERT_EQ(opaque_frame_1, compositor()->GetCurrentFrameAndUpdateIfStale());

  // An update current frame call should stop background rendering.
  EXPECT_CALL(*this, Render(_, _, false)).WillOnce(Return(opaque_frame_2));
  EXPECT_TRUE(
      compositor()->UpdateCurrentFrame(base::TimeTicks(), base::TimeTicks()));

  // This call should still not call background render.
  ASSERT_EQ(opaque_frame_2, compositor()->GetCurrentFrameAndUpdateIfStale());

  testing::Mock::VerifyAndClearExpectations(this);

  // Clear our client, which means no mock function calls for Client.
  compositor()->SetVideoFrameProviderClient(nullptr);

  // This call should still not call background render, because we aren't in the
  // background rendering state yet.
  ASSERT_EQ(opaque_frame_2, compositor()->GetCurrentFrameAndUpdateIfStale());

  // Wait for background rendering to tick again.
  base::RunLoop run_loop;
  EXPECT_CALL(*this, Render(_, _, true))
      .WillOnce(
           DoAll(RunClosure(run_loop.QuitClosure()), Return(opaque_frame_1)))
      .WillOnce(Return(opaque_frame_2));
  run_loop.Run();

  // This call should still not call background render, because not enough time
  // has elapsed since the last background render call.
  ASSERT_EQ(opaque_frame_1, compositor()->GetCurrentFrameAndUpdateIfStale());

  // Advancing the tick clock should allow a new frame to be requested.
  tick_clock_->Advance(base::TimeDelta::FromMilliseconds(10));
  ASSERT_EQ(opaque_frame_2, compositor()->GetCurrentFrameAndUpdateIfStale());

  // Background rendering should tick another render callback.
  StopVideoRendererSink(false);
}

}  // namespace media
