// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc/media_stream_video_webrtc_sink.h"

#include "base/test/scoped_feature_list.h"
#include "content/child/child_process.h"
#include "content/public/common/content_features.h"
#include "content/renderer/media/mock_constraint_factory.h"
#include "content/renderer/media/mock_media_stream_registry.h"
#include "content/renderer/media/video_track_adapter.h"
#include "content/renderer/media/webrtc/mock_peer_connection_dependency_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

class MediaStreamVideoWebRtcSinkTest : public ::testing::Test {
 public:
  MediaStreamVideoWebRtcSinkTest() {
    scoped_feature_list_.InitAndDisableFeature(
        features::kMediaStreamOldVideoConstraints);
  }

  void SetVideoTrack() {
    registry_.Init("stream URL");
    registry_.AddVideoTrack("test video track");
    blink::WebVector<blink::WebMediaStreamTrack> video_tracks;
    registry_.test_stream().VideoTracks(video_tracks);
    track_ = video_tracks[0];
    // TODO(hta): Verify that track_ is valid. When constraints produce
    // no valid format, using the track will cause a crash.
  }

  void SetVideoTrack(blink::WebMediaConstraints constraints) {
    registry_.Init("stream URL");
    registry_.AddVideoTrack("test video track", constraints);
    blink::WebVector<blink::WebMediaStreamTrack> video_tracks;
    registry_.test_stream().VideoTracks(video_tracks);
    track_ = video_tracks[0];
    // TODO(hta): Verify that track_ is valid. When constraints produce
    // no valid format, using the track will cause a crash.
  }

  void SetVideoTrack(const base::Optional<bool>& noise_reduction) {
    registry_.Init("stream URL");
    registry_.AddVideoTrack("test video track", VideoTrackAdapterSettings(),
                            noise_reduction, false, 0.0);
    blink::WebVector<blink::WebMediaStreamTrack> video_tracks;
    registry_.test_stream().VideoTracks(video_tracks);
    track_ = video_tracks[0];
    // TODO(hta): Verify that track_ is valid. When constraints produce
    // no valid format, using the track will cause a crash.
  }

 protected:
  blink::WebMediaStreamTrack track_;
  MockPeerConnectionDependencyFactory dependency_factory_;

 private:
  MockMediaStreamRegistry registry_;
  // A ChildProcess and a MessageLoopForUI are both needed to fool the Tracks
  // and Sources in |registry_| into believing they are on the right threads.
  base::MessageLoopForUI message_loop_;
  const ChildProcess child_process_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(MediaStreamVideoWebRtcSinkTest, NoiseReductionDefaultsToNotSet) {
  SetVideoTrack();
  MediaStreamVideoWebRtcSink my_sink(track_, &dependency_factory_);
  EXPECT_TRUE(my_sink.webrtc_video_track());
  EXPECT_FALSE(my_sink.SourceNeedsDenoisingForTesting());
}

// TODO(guidou): Remove this test. http://crbug.com/706408
TEST_F(MediaStreamVideoWebRtcSinkTest, NoiseReductionConstraintPassThrough) {
  SetVideoTrack(base::Optional<bool>(true));
  MediaStreamVideoWebRtcSink my_sink(track_, &dependency_factory_);
  EXPECT_TRUE(my_sink.SourceNeedsDenoisingForTesting());
  EXPECT_TRUE(*(my_sink.SourceNeedsDenoisingForTesting()));
}

// TODO(guidou): Remove this test. http://crbug.com/706408
class MediaStreamVideoWebRtcSinkOldConstraintsTest : public ::testing::Test {
 public:
  MediaStreamVideoWebRtcSinkOldConstraintsTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kMediaStreamOldVideoConstraints);
  }

  void SetVideoTrack() {
    registry_.Init("stream URL");
    registry_.AddVideoTrack("test video track");
    blink::WebVector<blink::WebMediaStreamTrack> video_tracks;
    registry_.test_stream().VideoTracks(video_tracks);
    track_ = video_tracks[0];
    // TODO(hta): Verify that track_ is valid. When constraints produce
    // no valid format, using the track will cause a crash.
  }

  void SetVideoTrack(blink::WebMediaConstraints constraints) {
    registry_.Init("stream URL");
    registry_.AddVideoTrack("test video track", constraints);
    blink::WebVector<blink::WebMediaStreamTrack> video_tracks;
    registry_.test_stream().VideoTracks(video_tracks);
    track_ = video_tracks[0];
    // TODO(hta): Verify that track_ is valid. When constraints produce
    // no valid format, using the track will cause a crash.
  }

  void SetVideoTrack(const base::Optional<bool>& noise_reduction) {
    registry_.Init("stream URL");
    registry_.AddVideoTrack("test video track", VideoTrackAdapterSettings(),
                            noise_reduction, false, 0.0);
    blink::WebVector<blink::WebMediaStreamTrack> video_tracks;
    registry_.test_stream().VideoTracks(video_tracks);
    track_ = video_tracks[0];
    // TODO(hta): Verify that track_ is valid. When constraints produce
    // no valid format, using the track will cause a crash.
  }

 protected:
  blink::WebMediaStreamTrack track_;
  MockPeerConnectionDependencyFactory dependency_factory_;

 private:
  MockMediaStreamRegistry registry_;
  // A ChildProcess and a MessageLoopForUI are both needed to fool the Tracks
  // and Sources in |registry_| into believing they are on the right threads.
  base::MessageLoopForUI message_loop_;
  const ChildProcess child_process_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(MediaStreamVideoWebRtcSinkOldConstraintsTest,
       NoiseReductionDefaultsToNotSet) {
  blink::WebMediaConstraints constraints;
  constraints.Initialize();
  SetVideoTrack(constraints);
  MediaStreamVideoWebRtcSink my_sink(track_, &dependency_factory_);
  EXPECT_TRUE(my_sink.webrtc_video_track());
  EXPECT_FALSE(my_sink.SourceNeedsDenoisingForTesting());
}

TEST_F(MediaStreamVideoWebRtcSinkOldConstraintsTest,
       NoiseReductionConstraintPassThrough) {
  MockConstraintFactory factory;
  factory.basic().goog_noise_reduction.SetExact(true);
  SetVideoTrack(factory.CreateWebMediaConstraints());
  MediaStreamVideoWebRtcSink my_sink(track_, &dependency_factory_);
  EXPECT_TRUE(my_sink.SourceNeedsDenoisingForTesting());
  EXPECT_TRUE(*(my_sink.SourceNeedsDenoisingForTesting()));
}

}  // namespace
}  // namespace content
