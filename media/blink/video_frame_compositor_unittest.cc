// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/test/simple_test_tick_clock.h"
#include "cc/layers/video_frame_provider.h"
#include "media/base/video_frame.h"
#include "media/blink/video_frame_compositor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Return;

namespace media {

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
        scoped_ptr<base::TickClock>(tick_clock_));
  }

  ~VideoFrameCompositorTest() override {
    compositor_->SetVideoFrameProviderClient(NULL);
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
  void DidUpdateMatrix(const float* matrix) override {}

  // VideoRendererSink::RenderCallback implementation.
  MOCK_METHOD2(Render,
               scoped_refptr<VideoFrame>(base::TimeTicks, base::TimeTicks));
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

  void StopVideoRendererSink() {
    EXPECT_CALL(*this, StopRendering());
    compositor()->Stop();
    message_loop.RunUntilIdle();

    // We should still have a frame after stop is called.
    EXPECT_TRUE(compositor_->GetCurrentFrame());
  }

  void RenderFrame() {
    compositor()->GetCurrentFrame();
    compositor()->PutCurrentFrame();
  }

  base::MessageLoop message_loop;
  base::SimpleTestTickClock* tick_clock_;  // Owned by |compositor_|
  scoped_ptr<VideoFrameCompositor> compositor_;

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

  // Callback isn't fired for the first frame.
  compositor()->PaintFrameUsingOldRenderingPath(initial_frame);
  EXPECT_EQ(empty_size, natural_size());
  EXPECT_EQ(0, natural_size_changed_count());

  // Callback should be fired once.
  compositor()->PaintFrameUsingOldRenderingPath(larger_frame);
  EXPECT_EQ(larger_size, natural_size());
  EXPECT_EQ(1, natural_size_changed_count());

  compositor()->PaintFrameUsingOldRenderingPath(larger_frame);
  EXPECT_EQ(larger_size, natural_size());
  EXPECT_EQ(1, natural_size_changed_count());

  // Callback is fired once more when switching back to initial size.
  compositor()->PaintFrameUsingOldRenderingPath(initial_frame);
  EXPECT_EQ(initial_size, natural_size());
  EXPECT_EQ(2, natural_size_changed_count());

  compositor()->PaintFrameUsingOldRenderingPath(initial_frame);
  EXPECT_EQ(initial_size, natural_size());
  EXPECT_EQ(2, natural_size_changed_count());

  natural_size_changed_count_ = 0;
  natural_size_ = empty_size;
  compositor()->clear_current_frame_for_testing();

  StartVideoRendererSink();
  EXPECT_CALL(*this, Render(_, _))
      .WillOnce(Return(initial_frame))
      .WillOnce(Return(larger_frame))
      .WillOnce(Return(initial_frame))
      .WillOnce(Return(initial_frame));
  // Callback isn't fired for the first frame.
  EXPECT_EQ(0, natural_size_changed_count());
  EXPECT_TRUE(
      compositor()->UpdateCurrentFrame(base::TimeTicks(), base::TimeTicks()));
  RenderFrame();
  EXPECT_EQ(empty_size, natural_size());
  EXPECT_EQ(0, natural_size_changed_count());

  EXPECT_TRUE(
      compositor()->UpdateCurrentFrame(base::TimeTicks(), base::TimeTicks()));
  RenderFrame();
  EXPECT_EQ(larger_size, natural_size());
  EXPECT_EQ(1, natural_size_changed_count());

  EXPECT_TRUE(
      compositor()->UpdateCurrentFrame(base::TimeTicks(), base::TimeTicks()));
  RenderFrame();
  EXPECT_EQ(initial_size, natural_size());
  EXPECT_EQ(2, natural_size_changed_count());

  EXPECT_FALSE(
      compositor()->UpdateCurrentFrame(base::TimeTicks(), base::TimeTicks()));
  EXPECT_EQ(initial_size, natural_size());
  EXPECT_EQ(2, natural_size_changed_count());
  RenderFrame();

  StopVideoRendererSink();
}

TEST_F(VideoFrameCompositorTest, OpacityChanged) {
  gfx::Size size(8, 8);
  scoped_refptr<VideoFrame> opaque_frame = VideoFrame::CreateFrame(
      VideoFrame::YV12, size, gfx::Rect(size), size, base::TimeDelta());
  scoped_refptr<VideoFrame> not_opaque_frame = VideoFrame::CreateFrame(
      VideoFrame::YV12A, size, gfx::Rect(size), size, base::TimeDelta());

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

  StartVideoRendererSink();
  EXPECT_CALL(*this, Render(_, _))
      .WillOnce(Return(not_opaque_frame))
      .WillOnce(Return(not_opaque_frame))
      .WillOnce(Return(opaque_frame))
      .WillOnce(Return(opaque_frame));
  EXPECT_EQ(0, opacity_changed_count());
  EXPECT_TRUE(
      compositor()->UpdateCurrentFrame(base::TimeTicks(), base::TimeTicks()));
  RenderFrame();
  EXPECT_FALSE(opaque());
  EXPECT_EQ(1, opacity_changed_count());

  EXPECT_FALSE(
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

  StopVideoRendererSink();
}

TEST_F(VideoFrameCompositorTest, VideoRendererSinkFrameDropped) {
  gfx::Size size(8, 8);
  scoped_refptr<VideoFrame> opaque_frame = VideoFrame::CreateFrame(
      VideoFrame::YV12, size, gfx::Rect(size), size, base::TimeDelta());

  StartVideoRendererSink();
  EXPECT_CALL(*this, Render(_, _)).WillRepeatedly(Return(opaque_frame));
  EXPECT_TRUE(
      compositor()->UpdateCurrentFrame(base::TimeTicks(), base::TimeTicks()));

  // If we don't call RenderFrame() the frame should be reported as dropped.
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

  StopVideoRendererSink();
}

TEST_F(VideoFrameCompositorTest, GetCurrentFrameAndUpdateIfStale) {
  gfx::Size size(8, 8);
  scoped_refptr<VideoFrame> opaque_frame = VideoFrame::CreateFrame(
      VideoFrame::YV12, size, gfx::Rect(size), size, base::TimeDelta());
  scoped_refptr<VideoFrame> opaque_frame_2 = VideoFrame::CreateFrame(
      VideoFrame::YV12, size, gfx::Rect(size), size, base::TimeDelta());

  StartVideoRendererSink();
  EXPECT_CALL(*this, Render(_, _))
      .WillOnce(Return(opaque_frame))
      .WillOnce(Return(opaque_frame_2));
  EXPECT_TRUE(
      compositor()->UpdateCurrentFrame(base::TimeTicks(), base::TimeTicks()));

  base::TimeDelta stale_frame_threshold =
      compositor()->get_stale_frame_threshold_for_testing();

  // Advancing time a little bit shouldn't cause the frame to be stale.
  tick_clock_->Advance(stale_frame_threshold / 2);
  EXPECT_EQ(opaque_frame, compositor()->GetCurrentFrameAndUpdateIfStale());

  // Since rendering of frames is likely not happening, this will trigger a
  // dropped frame call.
  EXPECT_CALL(*this, OnFrameDropped());

  // Advancing the clock over the threshold should cause a new frame request.
  tick_clock_->Advance(stale_frame_threshold / 2 +
                       base::TimeDelta::FromMicroseconds(1));
  EXPECT_EQ(opaque_frame_2, compositor()->GetCurrentFrameAndUpdateIfStale());

  StopVideoRendererSink();
}

TEST_F(VideoFrameCompositorTest, StopUpdatesCurrentFrameIfStale) {
  gfx::Size size(8, 8);
  scoped_refptr<VideoFrame> opaque_frame = VideoFrame::CreateFrame(
      VideoFrame::YV12, size, gfx::Rect(size), size, base::TimeDelta());

  const base::TimeDelta interval = base::TimeDelta::FromSecondsD(1.0 / 60);

  StartVideoRendererSink();

  // Expect two calls to Render(), one from UpdateCurrentFrame() and one from
  // Stop() because the frame is too old.
  EXPECT_CALL(*this, Render(_, _))
      .WillOnce(Return(opaque_frame))
      .WillOnce(Return(opaque_frame));
  EXPECT_TRUE(compositor()->UpdateCurrentFrame(base::TimeTicks(),
                                               base::TimeTicks() + interval));
  tick_clock_->Advance(interval * 2);
  StopVideoRendererSink();
}

}  // namespace media
