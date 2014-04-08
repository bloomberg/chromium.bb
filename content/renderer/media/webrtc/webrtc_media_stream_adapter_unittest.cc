// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "content/renderer/media/media_stream.h"
#include "content/renderer/media/media_stream_audio_source.h"
#include "content/renderer/media/media_stream_video_source.h"
#include "content/renderer/media/media_stream_video_track.h"
#include "content/renderer/media/mock_media_stream_dependency_factory.h"
#include "content/renderer/media/mock_media_stream_video_source.h"
#include "content/renderer/media/webrtc/webrtc_local_audio_track_adapter.h"
#include "content/renderer/media/webrtc/webrtc_media_stream_adapter.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebMediaStream.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"
#include "third_party/WebKit/public/platform/WebVector.h"

namespace content {

class WebRtcMediaStreamAdapterTest : public ::testing::Test {
 public:
  virtual void SetUp() {
    dependency_factory_.reset(new MockMediaStreamDependencyFactory());
  }

  blink::WebMediaStream CreateBlinkMediaStream(bool audio, bool video) {
    blink::WebVector<blink::WebMediaStreamTrack> audio_track_vector(
        audio ? static_cast<size_t>(1) : 0);
    if (audio) {
      blink::WebMediaStreamSource audio_source;
      audio_source.initialize("audio",
                              blink::WebMediaStreamSource::TypeAudio,
                              "audio");
      audio_source.setExtraData(new MediaStreamAudioSource());

      audio_track_vector[0].initialize(audio_source);
      MediaStreamTrack* native_track =
          new MediaStreamTrack(
              WebRtcLocalAudioTrackAdapter::Create(
                  audio_track_vector[0].id().utf8(), NULL),
                  true);
      audio_track_vector[0].setExtraData(native_track);
    }

    blink::WebVector<blink::WebMediaStreamTrack> video_track_vector(
        video ? static_cast<size_t>(1) : 0);
    MediaStreamSource::SourceStoppedCallback dummy_callback;
    if (video) {
      blink::WebMediaStreamSource video_source;
      video_source.initialize("video",
                              blink::WebMediaStreamSource::TypeVideo,
                              "video");
      MediaStreamVideoSource* native_source =
          new MockMediaStreamVideoSource(dependency_factory_.get(), false);
      video_source.setExtraData(native_source);
      blink::WebMediaConstraints constraints;
      constraints.initialize();
      video_track_vector[0] = MediaStreamVideoTrack::CreateVideoTrack(
          native_source, constraints,
          MediaStreamVideoSource::ConstraintsCallback(), true,
          dependency_factory_.get());
    }

    blink::WebMediaStream stream_desc;
    stream_desc.initialize("media stream",
                           audio_track_vector,
                           video_track_vector);
    stream_desc.setExtraData(
        new content::MediaStream(content::MediaStream::StreamStopCallback(),
                                 stream_desc));
    return stream_desc;
  }

  void CreateWebRtcMediaStream(const blink::WebMediaStream& blink_stream,
                               size_t expected_number_of_audio_tracks,
                               size_t expected_number_of_video_tracks) {
    scoped_ptr<WebRtcMediaStreamAdapter> adapter(
        new WebRtcMediaStreamAdapter(blink_stream,
                                     dependency_factory_.get()));

    EXPECT_EQ(expected_number_of_audio_tracks,
              adapter->webrtc_media_stream()->GetAudioTracks().size());
    EXPECT_EQ(expected_number_of_video_tracks,
              adapter->webrtc_media_stream()->GetVideoTracks().size());
    EXPECT_EQ(blink_stream.id().utf8(),
              adapter->webrtc_media_stream()->label());
  }

 protected:
  scoped_ptr<MockMediaStreamDependencyFactory> dependency_factory_;
};

TEST_F(WebRtcMediaStreamAdapterTest, CreateWebRtcMediaStream) {
  blink::WebMediaStream blink_stream = CreateBlinkMediaStream(true, true);
  CreateWebRtcMediaStream(blink_stream, 1, 1);
}

// Test that we don't crash if a MediaStream is created in Blink with an unknown
// audio sources. This can happen if a MediaStream is created with
// remote audio track.
TEST_F(WebRtcMediaStreamAdapterTest,
       CreateWebRtcMediaStreamWithoutAudioSource) {
  // Create a blink MediaStream description.
  blink::WebMediaStreamSource audio_source;
  audio_source.initialize("audio source",
                          blink::WebMediaStreamSource::TypeAudio,
                          "something");

  blink::WebVector<blink::WebMediaStreamTrack> audio_tracks(
      static_cast<size_t>(1));
  audio_tracks[0].initialize(audio_source.id(), audio_source);
  blink::WebVector<blink::WebMediaStreamTrack> video_tracks(
      static_cast<size_t>(0));

  blink::WebMediaStream blink_stream;
  blink_stream.initialize("new stream", audio_tracks, video_tracks);
  blink_stream.setExtraData(
      new content::MediaStream(content::MediaStream::StreamStopCallback(),
                               blink_stream));
  CreateWebRtcMediaStream(blink_stream, 0, 0);
}

}  // namespace content
