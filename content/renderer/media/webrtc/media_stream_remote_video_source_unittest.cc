// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc/media_stream_remote_video_source.h"

#include <memory>
#include <utility>

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/child/child_process.h"
#include "content/renderer/media/media_stream_video_track.h"
#include "content/renderer/media/mock_media_stream_video_sink.h"
#include "content/renderer/media/webrtc/mock_peer_connection_dependency_factory.h"
#include "content/renderer/media/webrtc/track_observer.h"
#include "media/base/video_frame.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/web/WebHeap.h"

namespace content {

ACTION_P(RunClosure, closure) {
  closure.Run();
}

class MediaStreamRemoteVideoSourceUnderTest
    : public MediaStreamRemoteVideoSource {
 public:
  explicit MediaStreamRemoteVideoSourceUnderTest(
      std::unique_ptr<TrackObserver> observer)
      : MediaStreamRemoteVideoSource(std::move(observer)) {}
  using MediaStreamRemoteVideoSource::SinkInterfaceForTest;
};

class MediaStreamRemoteVideoSourceTest
    : public ::testing::Test {
 public:
  MediaStreamRemoteVideoSourceTest()
      : child_process_(new ChildProcess()),
        mock_factory_(new MockPeerConnectionDependencyFactory()),
        webrtc_video_track_(MockWebRtcVideoTrack::Create("test")),
        remote_source_(new MediaStreamRemoteVideoSourceUnderTest(
            std::unique_ptr<TrackObserver>(
                new TrackObserver(base::ThreadTaskRunnerHandle::Get(),
                                  webrtc_video_track_.get())))),
        number_of_successful_track_starts_(0),
        number_of_failed_track_starts_(0) {
    webkit_source_.Initialize(blink::WebString::FromASCII("dummy_source_id"),
                              blink::WebMediaStreamSource::kTypeVideo,
                              blink::WebString::FromASCII("dummy_source_name"),
                              true /* remote */);
    webkit_source_.SetExtraData(remote_source_);
  }

  void TearDown() override {
    remote_source_->OnSourceTerminated();
    webkit_source_.Reset();
    blink::WebHeap::CollectAllGarbageForTesting();
  }

  MediaStreamRemoteVideoSourceUnderTest* source() {
    return remote_source_;
  }

  MediaStreamVideoTrack* CreateTrack() {
    bool enabled = true;
    return new MediaStreamVideoTrack(
        source(),
        base::Bind(&MediaStreamRemoteVideoSourceTest::OnTrackStarted,
                   base::Unretained(this)),
        enabled);
  }

  int NumberOfSuccessConstraintsCallbacks() const {
    return number_of_successful_track_starts_;
  }

  int NumberOfFailedConstraintsCallbacks() const {
    return number_of_failed_track_starts_;
  }

  void StopWebRtcTrack() {
    static_cast<MockWebRtcVideoTrack*>(webrtc_video_track_.get())->SetEnded();
  }

  const blink::WebMediaStreamSource& webkit_source() const {
    return  webkit_source_;
  }

 private:
  void OnTrackStarted(MediaStreamSource* source,
                      MediaStreamRequestResult result,
                      const blink::WebString& result_name) {
    ASSERT_EQ(source, remote_source_);
    if (result == MEDIA_DEVICE_OK)
      ++number_of_successful_track_starts_;
    else
      ++number_of_failed_track_starts_;
  }

  base::MessageLoopForUI message_loop_;
  std::unique_ptr<ChildProcess> child_process_;
  std::unique_ptr<MockPeerConnectionDependencyFactory> mock_factory_;
  scoped_refptr<webrtc::VideoTrackInterface> webrtc_video_track_;
  // |remote_source_| is owned by |webkit_source_|.
  MediaStreamRemoteVideoSourceUnderTest* remote_source_;
  blink::WebMediaStreamSource webkit_source_;
  int number_of_successful_track_starts_;
  int number_of_failed_track_starts_;
};

TEST_F(MediaStreamRemoteVideoSourceTest, StartTrack) {
  std::unique_ptr<MediaStreamVideoTrack> track(CreateTrack());
  EXPECT_EQ(1, NumberOfSuccessConstraintsCallbacks());

  MockMediaStreamVideoSink sink;
  track->AddSink(&sink, sink.GetDeliverFrameCB(), false);
  base::RunLoop run_loop;
  base::Closure quit_closure = run_loop.QuitClosure();
  EXPECT_CALL(sink, OnVideoFrame()).WillOnce(
      RunClosure(quit_closure));
  rtc::scoped_refptr<webrtc::I420Buffer> buffer(
      new rtc::RefCountedObject<webrtc::I420Buffer>(320, 240));

  buffer->SetToBlack();

  source()->SinkInterfaceForTest()->OnFrame(
      webrtc::VideoFrame(buffer, webrtc::kVideoRotation_0, 1000));
  run_loop.Run();

  EXPECT_EQ(1, sink.number_of_frames());
  track->RemoveSink(&sink);
}

TEST_F(MediaStreamRemoteVideoSourceTest, RemoteTrackStop) {
  std::unique_ptr<MediaStreamVideoTrack> track(CreateTrack());

  MockMediaStreamVideoSink sink;
  track->AddSink(&sink, sink.GetDeliverFrameCB(), false);
  EXPECT_EQ(blink::WebMediaStreamSource::kReadyStateLive, sink.state());
  EXPECT_EQ(blink::WebMediaStreamSource::kReadyStateLive,
            webkit_source().GetReadyState());
  StopWebRtcTrack();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(blink::WebMediaStreamSource::kReadyStateEnded,
            webkit_source().GetReadyState());
  EXPECT_EQ(blink::WebMediaStreamSource::kReadyStateEnded, sink.state());

  track->RemoveSink(&sink);
}

}  // namespace content
