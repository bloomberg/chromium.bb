// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "media/base/video_frame.h"
#include "media/filters/video_frame_painter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class VideoFramePainterTest : public testing::Test {
 public:
  VideoFramePainterTest()
      : painter_(base::Bind(&VideoFramePainterTest::Invalidate,
                            base::Unretained(this)),
                 base::Bind(&VideoFramePainterTest::NaturalSizeChanged,
                            base::Unretained(this))),
        invalidate_count_(0),
        natural_size_changed_count_(0) {}
  virtual ~VideoFramePainterTest() {}

  VideoFramePainter* painter() { return &painter_; }
  int invalidate_count() { return invalidate_count_; }
  int natural_size_changed_count() { return natural_size_changed_count_; }
  gfx::Size natural_size() { return natural_size_; }

 private:
  void Invalidate() {
    ++invalidate_count_;
  }

  void NaturalSizeChanged(gfx::Size natural_size) {
    ++natural_size_changed_count_;
    natural_size_ = natural_size;
  }

  VideoFramePainter painter_;
  int invalidate_count_;
  int natural_size_changed_count_;
  gfx::Size natural_size_;

  DISALLOW_COPY_AND_ASSIGN(VideoFramePainterTest);
};

TEST_F(VideoFramePainterTest, InitialValues) {
  EXPECT_FALSE(painter()->GetCurrentFrame(false));
  EXPECT_EQ(0u, painter()->GetFramesDroppedBeforePaint());
}

TEST_F(VideoFramePainterTest, UpdateCurrentFrame) {
  scoped_refptr<VideoFrame> expected = VideoFrame::CreateEOSFrame();

  painter()->UpdateCurrentFrame(expected);
  scoped_refptr<VideoFrame> actual = painter()->GetCurrentFrame(false);

  EXPECT_EQ(1, invalidate_count());
  EXPECT_EQ(expected, actual);
}

TEST_F(VideoFramePainterTest, NaturalSizeChanged) {
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
  painter()->UpdateCurrentFrame(initial_frame);
  EXPECT_EQ(0, natural_size().width());
  EXPECT_EQ(0, natural_size().height());
  EXPECT_EQ(0, natural_size_changed_count());

  // Callback should be fired once.
  painter()->UpdateCurrentFrame(larger_frame);
  EXPECT_EQ(larger_size.width(), natural_size().width());
  EXPECT_EQ(larger_size.height(), natural_size().height());
  EXPECT_EQ(1, natural_size_changed_count());

  painter()->UpdateCurrentFrame(larger_frame);
  EXPECT_EQ(larger_size.width(), natural_size().width());
  EXPECT_EQ(larger_size.height(), natural_size().height());
  EXPECT_EQ(1, natural_size_changed_count());

  // Callback is fired once more when switching back to initial size.
  painter()->UpdateCurrentFrame(initial_frame);
  EXPECT_EQ(initial_size.width(), natural_size().width());
  EXPECT_EQ(initial_size.height(), natural_size().height());
  EXPECT_EQ(2, natural_size_changed_count());

  painter()->UpdateCurrentFrame(initial_frame);
  EXPECT_EQ(initial_size.width(), natural_size().width());
  EXPECT_EQ(initial_size, natural_size());
  EXPECT_EQ(2, natural_size_changed_count());
}

TEST_F(VideoFramePainterTest, DidFinishInvalidating) {
  scoped_refptr<VideoFrame> frame = VideoFrame::CreateEOSFrame();

  painter()->UpdateCurrentFrame(frame);
  EXPECT_EQ(1, invalidate_count());

  // Shouldn't increment until DidFinishInvalidating() is called.
  painter()->UpdateCurrentFrame(frame);
  EXPECT_EQ(1, invalidate_count());

  painter()->DidFinishInvalidating();
  painter()->UpdateCurrentFrame(frame);
  EXPECT_EQ(2, invalidate_count());
}

TEST_F(VideoFramePainterTest, GetFramesDroppedBeforePaint) {
  scoped_refptr<VideoFrame> frame = VideoFrame::CreateEOSFrame();

  painter()->UpdateCurrentFrame(frame);
  EXPECT_EQ(1, invalidate_count());
  EXPECT_EQ(0u, painter()->GetFramesDroppedBeforePaint());

  // Should not increment if we finished invalidating but didn't paint the
  // frame.
  //
  // This covers the scenario where the region we're invalidating isn't
  // visible so there wasn't a need to paint.
  painter()->DidFinishInvalidating();
  painter()->UpdateCurrentFrame(frame);
  EXPECT_EQ(2, invalidate_count());
  EXPECT_EQ(0u, painter()->GetFramesDroppedBeforePaint());

  // Should not increment if we didn't finish invalidating but still managed
  // to paint the frame.
  //
  // This covers the scenario where something else may have invalidated the
  // video's region and managed to paint the current frame.
  painter()->GetCurrentFrame(true);
  painter()->UpdateCurrentFrame(frame);
  EXPECT_EQ(2, invalidate_count());
  EXPECT_EQ(0u, painter()->GetFramesDroppedBeforePaint());

  // Should increment if we didn't invalidate and didn't paint the frame.
  //
  // This covers the scenario where we didn't even finish invalidating nor
  // painting the current frame before updating. Consider it dropped.
  painter()->UpdateCurrentFrame(frame);
  EXPECT_EQ(2, invalidate_count());
  EXPECT_EQ(1u, painter()->GetFramesDroppedBeforePaint());

  // Shouldn't overflow.
  painter()->SetFramesDroppedBeforePaintForTesting(kuint32max);
  painter()->UpdateCurrentFrame(frame);
  EXPECT_EQ(kuint32max, painter()->GetFramesDroppedBeforePaint());
}

}  // namespace media
