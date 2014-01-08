// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "content/renderer/media/media_stream_video_source.h"
#include "media/base/video_frame.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class MediaStreamVideoSourceTest
    : public ::testing::Test,
      public MediaStreamVideoSource {
 public:
  MediaStreamVideoSourceTest() {}
};

TEST_F(MediaStreamVideoSourceTest, OnVideoFrame) {
  blink::WebMediaConstraints constraints;
  blink::WebMediaStreamTrack track;
  AddTrack(track, constraints);
  SetReadyState(blink::WebMediaStreamSource::ReadyStateLive);
  const int kWidth = 640;
  const int kHeight = 480;
  scoped_refptr<media::VideoFrame> frame =
      media::VideoFrame::CreateBlackFrame(gfx::Size(kWidth, kHeight));
  ASSERT_TRUE(frame.get());
  DeliverVideoFrame(frame);
  SetReadyState(blink::WebMediaStreamSource::ReadyStateEnded);
  RemoveTrack(track);
}

}  // namespace content
