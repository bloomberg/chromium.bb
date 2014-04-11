// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "content/public/renderer/media_stream_video_sink.h"
#include "content/renderer/media/media_stream_video_track.h"
#include "content/renderer/media/mock_media_stream_dependency_factory.h"
#include "content/renderer/media/webrtc/media_stream_remote_video_source.h"
#include "media/base/video_frame.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libjingle/source/talk/media/webrtc/webrtcvideoframe.h"

namespace content {

class MockVideoSink : public MediaStreamVideoSink {
 public:
  MockVideoSink()
      : number_of_frames_(0),
        state_(blink::WebMediaStreamSource::ReadyStateLive) {
  }

  virtual void OnVideoFrame(
      const scoped_refptr<media::VideoFrame>& frame) OVERRIDE {
    ++number_of_frames_;
  }

  virtual void OnReadyStateChanged(
      blink::WebMediaStreamSource::ReadyState state) OVERRIDE {
    state_ = state;
  }

  int number_of_frames() const { return number_of_frames_; }
  blink::WebMediaStreamSource::ReadyState state() const { return state_; }

 private:
  int number_of_frames_;
  blink::WebMediaStreamSource::ReadyState state_;
};

class MediaStreamRemoteVideoSourceUnderTest
    : public MediaStreamRemoteVideoSource {
 public:
  MediaStreamRemoteVideoSourceUnderTest(webrtc::VideoTrackInterface* track)
      : MediaStreamRemoteVideoSource(track) {
  }
  using MediaStreamRemoteVideoSource::RenderFrame;
};

class MediaStreamRemoteVideoSourceTest
    : public ::testing::Test {
 public:
  MediaStreamRemoteVideoSourceTest()
      : mock_factory_(new MockMediaStreamDependencyFactory()),
        webrtc_video_track_(
            mock_factory_->CreateLocalVideoTrack(
                "test",
                static_cast<cricket::VideoCapturer*>(NULL))),
        remote_source_(
            new MediaStreamRemoteVideoSourceUnderTest(webrtc_video_track_)),
        number_of_successful_constraints_applied_(0),
        number_of_failed_constraints_applied_(0) {
    webkit_source_.initialize(base::UTF8ToUTF16("dummy_source_id"),
                              blink::WebMediaStreamSource::TypeVideo,
                              base::UTF8ToUTF16("dummy_source_name"));
    webkit_source_.setExtraData(remote_source_);
  }
  virtual ~MediaStreamRemoteVideoSourceTest() {}

  MediaStreamRemoteVideoSourceUnderTest* source() {
    return remote_source_;
  }

 MediaStreamVideoTrack* CreateTrack() {
   bool enabled = true;
   blink::WebMediaConstraints constraints;
   constraints.initialize();
   MediaStreamDependencyFactory* factory = NULL;
   return new MediaStreamVideoTrack(
       source(),
       constraints,
       base::Bind(
           &MediaStreamRemoteVideoSourceTest::OnConstraintsApplied,
           base::Unretained(this)),
       enabled, factory);
  }

  int NumberOfSuccessConstraintsCallbacks() const {
    return number_of_successful_constraints_applied_;
  }

  int NumberOfFailedConstraintsCallbacks() const {
    return number_of_failed_constraints_applied_;
  }

  void StopWebRtcTrack() {
    static_cast<MockWebRtcVideoTrack*>(webrtc_video_track_.get())->set_state(
        webrtc::MediaStreamTrackInterface::kEnded);
  }

  base::MessageLoop* message_loop() { return &message_loop_; }

  const blink::WebMediaStreamSource& webkit_source() const {
    return  webkit_source_;
  }

 private:
  void OnConstraintsApplied(MediaStreamSource* source, bool success) {
    ASSERT_EQ(source, remote_source_);
    if (success)
      ++number_of_successful_constraints_applied_;
    else
      ++number_of_failed_constraints_applied_;
  }

  base::MessageLoop message_loop_;
  scoped_ptr<MockMediaStreamDependencyFactory> mock_factory_;
  scoped_refptr<webrtc::VideoTrackInterface> webrtc_video_track_;
  // |remote_source_| is owned by |webkit_source_|.
  MediaStreamRemoteVideoSourceUnderTest* remote_source_;
  blink::WebMediaStreamSource webkit_source_;
  int number_of_successful_constraints_applied_;
  int number_of_failed_constraints_applied_;
};

TEST_F(MediaStreamRemoteVideoSourceTest, StartTrack) {
  scoped_ptr<MediaStreamVideoTrack> track(CreateTrack());
  EXPECT_EQ(0, NumberOfSuccessConstraintsCallbacks());

  MockVideoSink sink;
  track->AddSink(&sink);

  cricket::WebRtcVideoFrame webrtc_frame;
  webrtc_frame.InitToBlack(320, 240, 1, 1, 0, 1);
  source()->RenderFrame(&webrtc_frame);
  message_loop()->RunUntilIdle();

  EXPECT_EQ(1, NumberOfSuccessConstraintsCallbacks());
  EXPECT_EQ(1, sink.number_of_frames());
  track->RemoveSink(&sink);
}

TEST_F(MediaStreamRemoteVideoSourceTest, RemoteTrackStop) {
  scoped_ptr<MediaStreamVideoTrack> track(CreateTrack());

  MockVideoSink sink;
  track->AddSink(&sink);

  EXPECT_EQ(blink::WebMediaStreamSource::ReadyStateLive, sink.state());
  EXPECT_EQ(blink::WebMediaStreamSource::ReadyStateLive,
            webkit_source().readyState());
  StopWebRtcTrack();
  EXPECT_EQ(blink::WebMediaStreamSource::ReadyStateEnded,
            webkit_source().readyState());
  EXPECT_EQ(blink::WebMediaStreamSource::ReadyStateEnded, sink.state());

  track->RemoveSink(&sink);
}

}  // namespace content
