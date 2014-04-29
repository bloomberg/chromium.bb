// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "cc/layers/video_frame_provider.h"
#include "content/renderer/media/video_frame_compositor.h"
#include "media/base/video_frame.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

using media::VideoFrame;

class VideoFrameCompositorTest : public testing::Test,
                                 public cc::VideoFrameProvider::Client {
 public:
  VideoFrameCompositorTest()
      : compositor_(new VideoFrameCompositor(
            message_loop_.message_loop_proxy(),
            base::Bind(&VideoFrameCompositorTest::NaturalSizeChanged,
                       base::Unretained(this)),
            base::Bind(&VideoFrameCompositorTest::OpacityChanged,
                       base::Unretained(this)))),
        did_receive_frame_count_(0),
        natural_size_changed_count_(0),
        opacity_changed_count_(0),
        opaque_(false) {
    provider()->SetVideoFrameProviderClient(this);
  }

  virtual ~VideoFrameCompositorTest() {
    provider()->SetVideoFrameProviderClient(NULL);
    compositor_.reset();
    message_loop_.RunUntilIdle();
  }

  base::MessageLoop* message_loop() { return &message_loop_; }
  VideoFrameCompositor* compositor() { return compositor_.get(); }
  cc::VideoFrameProvider* provider() {
    return compositor_->GetVideoFrameProvider();
  }
  int did_receive_frame_count() { return did_receive_frame_count_; }
  int natural_size_changed_count() { return natural_size_changed_count_; }
  gfx::Size natural_size() { return natural_size_; }

  int opacity_changed_count() { return opacity_changed_count_; }
  bool opaque() { return opaque_; }

 private:
  // cc::VideoFrameProvider::Client implementation.
  virtual void StopUsingProvider() OVERRIDE {}
  virtual void DidReceiveFrame() OVERRIDE {
    ++did_receive_frame_count_;
  }
  virtual void DidUpdateMatrix(const float* matrix) OVERRIDE {}

  void NaturalSizeChanged(gfx::Size natural_size) {
    ++natural_size_changed_count_;
    natural_size_ = natural_size;
  }

  void OpacityChanged(bool opaque) {
    ++opacity_changed_count_;
    opaque_ = opaque;
  }

  base::MessageLoop message_loop_;
  scoped_ptr<VideoFrameCompositor> compositor_;
  int did_receive_frame_count_;
  int natural_size_changed_count_;
  gfx::Size natural_size_;
  int opacity_changed_count_;
  bool opaque_;

  DISALLOW_COPY_AND_ASSIGN(VideoFrameCompositorTest);
};

TEST_F(VideoFrameCompositorTest, InitialValues) {
  EXPECT_TRUE(compositor()->GetVideoFrameProvider());
  EXPECT_FALSE(compositor()->GetCurrentFrame());
  EXPECT_EQ(0u, compositor()->GetFramesDroppedBeforeCompositorWasNotified());
}

TEST_F(VideoFrameCompositorTest, UpdateCurrentFrame) {
  scoped_refptr<VideoFrame> expected = VideoFrame::CreateEOSFrame();

  compositor()->UpdateCurrentFrame(expected);
  scoped_refptr<VideoFrame> actual = compositor()->GetCurrentFrame();
  EXPECT_EQ(expected, actual);

  // Should notify compositor asynchronously.
  EXPECT_EQ(0, did_receive_frame_count());
  message_loop()->RunUntilIdle();
  EXPECT_EQ(1, did_receive_frame_count());
}

TEST_F(VideoFrameCompositorTest, NaturalSizeChanged) {
  gfx::Size initial_size(8, 8);
  scoped_refptr<VideoFrame> initial_frame =
      VideoFrame::CreateBlackFrame(initial_size);

  gfx::Size larger_size(16, 16);
  scoped_refptr<VideoFrame> larger_frame =
      VideoFrame::CreateBlackFrame(larger_size);

  // Initial expectations.
  EXPECT_EQ(0, natural_size().width());
  EXPECT_EQ(0, natural_size().height());
  EXPECT_EQ(0, natural_size_changed_count());

  // Callback isn't fired for the first frame.
  compositor()->UpdateCurrentFrame(initial_frame);
  EXPECT_EQ(0, natural_size().width());
  EXPECT_EQ(0, natural_size().height());
  EXPECT_EQ(0, natural_size_changed_count());

  // Callback should be fired once.
  compositor()->UpdateCurrentFrame(larger_frame);
  EXPECT_EQ(larger_size.width(), natural_size().width());
  EXPECT_EQ(larger_size.height(), natural_size().height());
  EXPECT_EQ(1, natural_size_changed_count());

  compositor()->UpdateCurrentFrame(larger_frame);
  EXPECT_EQ(larger_size.width(), natural_size().width());
  EXPECT_EQ(larger_size.height(), natural_size().height());
  EXPECT_EQ(1, natural_size_changed_count());

  // Callback is fired once more when switching back to initial size.
  compositor()->UpdateCurrentFrame(initial_frame);
  EXPECT_EQ(initial_size.width(), natural_size().width());
  EXPECT_EQ(initial_size.height(), natural_size().height());
  EXPECT_EQ(2, natural_size_changed_count());

  compositor()->UpdateCurrentFrame(initial_frame);
  EXPECT_EQ(initial_size.width(), natural_size().width());
  EXPECT_EQ(initial_size, natural_size());
  EXPECT_EQ(2, natural_size_changed_count());
}

TEST_F(VideoFrameCompositorTest, OpacityChanged) {
  gfx::Size size(8, 8);
  gfx::Rect rect(gfx::Point(0, 0), size);
  scoped_refptr<VideoFrame> opaque_frame = VideoFrame::CreateFrame(
      VideoFrame::YV12, size, rect, size, base::TimeDelta());
  scoped_refptr<VideoFrame> not_opaque_frame = VideoFrame::CreateFrame(
      VideoFrame::YV12A, size, rect, size, base::TimeDelta());

  // Initial expectations.
  EXPECT_FALSE(opaque());
  EXPECT_EQ(0, opacity_changed_count());

  // Callback is fired for the first frame.
  compositor()->UpdateCurrentFrame(not_opaque_frame);
  EXPECT_FALSE(opaque());
  EXPECT_EQ(1, opacity_changed_count());

  // Callback shouldn't be first subsequent times with same opaqueness.
  compositor()->UpdateCurrentFrame(not_opaque_frame);
  EXPECT_FALSE(opaque());
  EXPECT_EQ(1, opacity_changed_count());

  // Callback is fired when using opacity changes.
  compositor()->UpdateCurrentFrame(opaque_frame);
  EXPECT_TRUE(opaque());
  EXPECT_EQ(2, opacity_changed_count());

  // Callback shouldn't be first subsequent times with same opaqueness.
  compositor()->UpdateCurrentFrame(opaque_frame);
  EXPECT_TRUE(opaque());
  EXPECT_EQ(2, opacity_changed_count());
}

TEST_F(VideoFrameCompositorTest, GetFramesDroppedBeforeCompositorWasNotified) {
  scoped_refptr<VideoFrame> frame = VideoFrame::CreateEOSFrame();

  compositor()->UpdateCurrentFrame(frame);
  EXPECT_EQ(0, did_receive_frame_count());
  EXPECT_EQ(0u, compositor()->GetFramesDroppedBeforeCompositorWasNotified());

  // Should not increment if we finished notifying the compositor.
  //
  // This covers the normal scenario where the compositor is getting
  // notifications in a timely manner.
  message_loop()->RunUntilIdle();
  compositor()->UpdateCurrentFrame(frame);
  EXPECT_EQ(1, did_receive_frame_count());
  EXPECT_EQ(0u, compositor()->GetFramesDroppedBeforeCompositorWasNotified());

  // Should increment if we didn't notify the compositor.
  //
  // This covers the scenario where the compositor is falling behind.
  // Consider it dropped.
  message_loop()->RunUntilIdle();
  compositor()->UpdateCurrentFrame(frame);
  compositor()->UpdateCurrentFrame(frame);
  EXPECT_EQ(2, did_receive_frame_count());
  EXPECT_EQ(1u, compositor()->GetFramesDroppedBeforeCompositorWasNotified());

  // Shouldn't overflow.
  compositor()->SetFramesDroppedBeforeCompositorWasNotifiedForTesting(
      kuint32max);
  compositor()->UpdateCurrentFrame(frame);
  EXPECT_EQ(kuint32max,
            compositor()->GetFramesDroppedBeforeCompositorWasNotified());
}

}  // namespace content
