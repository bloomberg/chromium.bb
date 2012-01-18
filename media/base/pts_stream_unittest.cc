// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/pts_stream.h"
#include "media/base/video_frame.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class PtsStreamTest : public testing::Test {
 public:
  PtsStreamTest() {
    video_frame_ = VideoFrame::CreateBlackFrame(16, 16);

    // Use typical frame rate of 25 fps.
    base::TimeDelta frame_duration = base::TimeDelta::FromMicroseconds(40000);
    pts_stream_.Initialize(frame_duration);
  }

  virtual ~PtsStreamTest() {}

 protected:
  PtsStream pts_stream_;
  scoped_refptr<VideoFrame> video_frame_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PtsStreamTest);
};

TEST_F(PtsStreamTest, NoTimestamp) {
  // Simulate an uninitialized |video_frame| where we cannot determine a
  // timestamp at all.
  video_frame_->SetTimestamp(kNoTimestamp());
  video_frame_->SetDuration(kNoTimestamp());
  pts_stream_.UpdatePtsAndDuration(video_frame_);
  EXPECT_EQ(0, pts_stream_.current_pts().InMicroseconds());
  EXPECT_EQ(40000, pts_stream_.current_duration().InMicroseconds());
}

TEST_F(PtsStreamTest, LastKnownTimestamp) {
  // Setup the last known pts to be at 100 microseconds with 16 microsecond
  // duration.
  video_frame_->SetTimestamp(base::TimeDelta::FromMicroseconds(100));
  video_frame_->SetDuration(base::TimeDelta::FromMicroseconds(16));
  pts_stream_.UpdatePtsAndDuration(video_frame_);

  // Simulate an uninitialized |video_frame| where last known pts will be used
  // to generate a timestamp and |frame_duration| will be used to generate a
  // duration.
  video_frame_->SetTimestamp(kNoTimestamp());
  video_frame_->SetDuration(kNoTimestamp());
  pts_stream_.UpdatePtsAndDuration(video_frame_);
  EXPECT_EQ(116, pts_stream_.current_pts().InMicroseconds());
  EXPECT_EQ(40000, pts_stream_.current_duration().InMicroseconds());
}

TEST_F(PtsStreamTest, TimestampIsZero) {
  // Test that having pts == 0 in the frame also behaves like the pts is not
  // provided.  This is because FFmpeg set the pts to zero when there is no
  // data for the frame, which means that value is useless to us.
  //
  // TODO(scherkus): FFmpegVideoDecodeEngine should be able to detect this
  // situation and set the timestamp to kInvalidTimestamp.
  video_frame_->SetTimestamp(base::TimeDelta::FromMicroseconds(100));
  video_frame_->SetDuration(base::TimeDelta::FromMicroseconds(16));
  pts_stream_.UpdatePtsAndDuration(video_frame_);
  EXPECT_EQ(100, pts_stream_.current_pts().InMicroseconds());
  EXPECT_EQ(16, pts_stream_.current_duration().InMicroseconds());

  // Should use estimation and default frame rate.
  video_frame_->SetTimestamp(base::TimeDelta::FromMicroseconds(0));
  video_frame_->SetDuration(base::TimeDelta::FromMicroseconds(0));
  pts_stream_.UpdatePtsAndDuration(video_frame_);
  EXPECT_EQ(116, pts_stream_.current_pts().InMicroseconds());
  EXPECT_EQ(40000, pts_stream_.current_duration().InMicroseconds());

  // Should override estimation but still use default frame rate.
  video_frame_->SetTimestamp(base::TimeDelta::FromMicroseconds(200));
  video_frame_->SetDuration(base::TimeDelta::FromMicroseconds(0));
  pts_stream_.EnqueuePts(video_frame_);

  video_frame_->SetTimestamp(base::TimeDelta::FromMicroseconds(0));
  video_frame_->SetDuration(base::TimeDelta::FromMicroseconds(0));
  pts_stream_.UpdatePtsAndDuration(video_frame_);
  EXPECT_EQ(200, pts_stream_.current_pts().InMicroseconds());
  EXPECT_EQ(40000, pts_stream_.current_duration().InMicroseconds());

  // Should override estimation and frame rate.
  video_frame_->SetTimestamp(base::TimeDelta::FromMicroseconds(456));
  video_frame_->SetDuration(base::TimeDelta::FromMicroseconds(0));
  pts_stream_.EnqueuePts(video_frame_);

  video_frame_->SetTimestamp(base::TimeDelta::FromMicroseconds(0));
  video_frame_->SetDuration(base::TimeDelta::FromMicroseconds(789));
  pts_stream_.UpdatePtsAndDuration(video_frame_);
  EXPECT_EQ(456, pts_stream_.current_pts().InMicroseconds());
  EXPECT_EQ(789, pts_stream_.current_duration().InMicroseconds());
}

}  // namespace media
