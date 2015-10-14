// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "content/child/child_process.h"
#include "content/renderer/media/media_stream_video_renderer_sink.h"
#include "content/renderer/media/mock_media_stream_registry.h"
#include "media/base/video_frame.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebHeap.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::InSequence;
using ::testing::Lt;
using ::testing::Mock;

namespace content {

ACTION_P(RunClosure, closure) {
  closure.Run();
}

static const std::string kTestStreamUrl = "stream_url";
static const std::string kTestVideoTrackId = "video_track_id";

class MediaStreamVideoRendererSinkTest : public testing::Test {
 public:
  MediaStreamVideoRendererSinkTest() {
    registry_.Init(kTestStreamUrl);
    registry_.AddVideoTrack(kTestVideoTrackId);

    // Extract the Blink Video Track for the MSVRSink.
    blink::WebVector<blink::WebMediaStreamTrack> video_tracks;
    registry_.test_stream().videoTracks(video_tracks);
    EXPECT_EQ(1u, video_tracks.size());

    media_stream_video_renderer_sink_ = new MediaStreamVideoRendererSink(
        video_tracks[0],
        base::Bind(&MediaStreamVideoRendererSinkTest::ErrorCallback,
                   base::Unretained(this)),
        base::Bind(&MediaStreamVideoRendererSinkTest::RepaintCallback,
                   base::Unretained(this)));

    EXPECT_TRUE(IsInStoppedState());
  }

  ~MediaStreamVideoRendererSinkTest() {
    media_stream_video_renderer_sink_ = nullptr;
    registry_.reset();
    blink::WebHeap::collectAllGarbageForTesting();
  }

  MOCK_METHOD1(RepaintCallback, void(const scoped_refptr<media::VideoFrame>&));
  MOCK_METHOD0(ErrorCallback, void(void));

  bool IsInStartedState() const {
    return media_stream_video_renderer_sink_->state_ ==
           MediaStreamVideoRendererSink::STARTED;
  }
  bool IsInStoppedState() const {
    return media_stream_video_renderer_sink_->state_ ==
           MediaStreamVideoRendererSink::STOPPED;
  }
  bool IsInPausedState() const {
    return media_stream_video_renderer_sink_->state_ ==
           MediaStreamVideoRendererSink::PAUSED;
  }

  void OnVideoFrame(const scoped_refptr<media::VideoFrame>& frame) {
    media_stream_video_renderer_sink_->OnVideoFrame(frame,
                                                    base::TimeTicks::Now());
  }

  scoped_refptr<MediaStreamVideoRendererSink> media_stream_video_renderer_sink_;

  // A ChildProcess and a MessageLoopForUI are both needed to fool the Tracks
  // and Sources in |registry_| into believing they are on the right threads.
  const base::MessageLoopForUI message_loop_;
  const ChildProcess child_process_;
  MockMediaStreamRegistry registry_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaStreamVideoRendererSinkTest);
};

// Checks that the initialization-destruction sequence works fine.
TEST_F(MediaStreamVideoRendererSinkTest, StartStop) {
  EXPECT_TRUE(IsInStoppedState());

  media_stream_video_renderer_sink_->Start();
  EXPECT_TRUE(IsInStartedState());

  media_stream_video_renderer_sink_->Pause();
  EXPECT_TRUE(IsInPausedState());

  media_stream_video_renderer_sink_->Play();  // Should be called Resume().
  EXPECT_TRUE(IsInStartedState());

  media_stream_video_renderer_sink_->Stop();
  EXPECT_TRUE(IsInStoppedState());
}

// Sends 2 frames and expect them as WebM contained encoded data in writeData().
TEST_F(MediaStreamVideoRendererSinkTest, EncodeVideoFrames) {
  media_stream_video_renderer_sink_->Start();

  InSequence s;
  const scoped_refptr<media::VideoFrame> video_frame =
      media::VideoFrame::CreateBlackFrame(gfx::Size(160, 80));

  EXPECT_CALL(*this, RepaintCallback(video_frame)).Times(1);
  OnVideoFrame(video_frame);

  media_stream_video_renderer_sink_->Stop();
}

}  // namespace content
