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
                       base::Unretained(this)))),
        did_receive_frame_count_(0),
        natural_size_changed_count_(0) {
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

  base::MessageLoop message_loop_;
  scoped_ptr<VideoFrameCompositor> compositor_;
  int did_receive_frame_count_;
  int natural_size_changed_count_;
  gfx::Size natural_size_;

  DISALLOW_COPY_AND_ASSIGN(VideoFrameCompositorTest);
};

TEST_F(VideoFrameCompositorTest, InitialValues) {
  EXPECT_TRUE(compositor()->GetVideoFrameProvider());
  EXPECT_FALSE(compositor()->GetCurrentFrame());
  EXPECT_EQ(0u, compositor()->GetFramesDroppedBeforeComposite());
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

TEST_F(VideoFrameCompositorTest, GetFramesDroppedBeforeComposite) {
  scoped_refptr<VideoFrame> frame = VideoFrame::CreateEOSFrame();

  compositor()->UpdateCurrentFrame(frame);
  EXPECT_EQ(0, did_receive_frame_count());
  EXPECT_EQ(0u, compositor()->GetFramesDroppedBeforeComposite());

  // Should not increment if we finished notifying the compositor but didn't
  // composite the frame.
  //
  // This covers the scenario where the region we're rendering isn't
  // visible so there wasn't a need to composite.
  message_loop()->RunUntilIdle();
  compositor()->UpdateCurrentFrame(frame);
  EXPECT_EQ(1, did_receive_frame_count());
  EXPECT_EQ(0u, compositor()->GetFramesDroppedBeforeComposite());

  // Should not increment if we didn't finish notifying the compositor but still
  // managed to composite the frame.
  //
  // This covers the scenario where something else may have notified the
  // compositor and managed to composite the current frame.
  message_loop()->RunUntilIdle();
  provider()->GetCurrentFrame();
  provider()->PutCurrentFrame(NULL);
  compositor()->UpdateCurrentFrame(frame);
  EXPECT_EQ(2, did_receive_frame_count());
  EXPECT_EQ(0u, compositor()->GetFramesDroppedBeforeComposite());

  // Should increment if we didn't notify the compositor and didn't composite
  // the frame.
  //
  // This covers the scenario where we didn't even finish notifying nor
  // compositing the current frame before updating. Consider it dropped.
  message_loop()->RunUntilIdle();
  compositor()->UpdateCurrentFrame(frame);
  compositor()->UpdateCurrentFrame(frame);
  EXPECT_EQ(3, did_receive_frame_count());
  EXPECT_EQ(1u, compositor()->GetFramesDroppedBeforeComposite());

  // Shouldn't overflow.
  compositor()->SetFramesDroppedBeforeCompositeForTesting(kuint32max);
  compositor()->UpdateCurrentFrame(frame);
  EXPECT_EQ(kuint32max, compositor()->GetFramesDroppedBeforeComposite());
}

}  // namespace content
