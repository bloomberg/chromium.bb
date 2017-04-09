// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/test_runner/mock_web_media_stream_center.h"

#include <stddef.h>

#include "base/logging.h"
#include "base/macros.h"
#include "content/shell/test_runner/test_interfaces.h"
#include "content/shell/test_runner/web_test_delegate.h"
#include "third_party/WebKit/public/platform/WebAudioDestinationConsumer.h"
#include "third_party/WebKit/public/platform/WebAudioSourceProvider.h"
#include "third_party/WebKit/public/platform/WebMediaStream.h"
#include "third_party/WebKit/public/platform/WebMediaStreamCenterClient.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"
#include "third_party/WebKit/public/platform/WebVector.h"

namespace test_runner {

namespace {

class MockWebAudioDestinationConsumer
    : public blink::WebAudioDestinationConsumer {
 public:
  MockWebAudioDestinationConsumer() {}
  ~MockWebAudioDestinationConsumer() override {}
  void SetFormat(size_t number_of_channels, float sample_rate) override {}
  void ConsumeAudio(const blink::WebVector<const float*>&,
                    size_t number_of_frames) override {}

  DISALLOW_COPY_AND_ASSIGN(MockWebAudioDestinationConsumer);
};

}  // namespace

void MockWebMediaStreamCenter::DidEnableMediaStreamTrack(
    const blink::WebMediaStreamTrack& track) {
  track.Source().SetReadyState(blink::WebMediaStreamSource::kReadyStateLive);
}

void MockWebMediaStreamCenter::DidDisableMediaStreamTrack(
    const blink::WebMediaStreamTrack& track) {
  track.Source().SetReadyState(blink::WebMediaStreamSource::kReadyStateMuted);
}

bool MockWebMediaStreamCenter::DidAddMediaStreamTrack(
    const blink::WebMediaStream& stream,
    const blink::WebMediaStreamTrack& track) {
  return true;
}

bool MockWebMediaStreamCenter::DidRemoveMediaStreamTrack(
    const blink::WebMediaStream& stream,
    const blink::WebMediaStreamTrack& track) {
  return true;
}

void MockWebMediaStreamCenter::DidStopLocalMediaStream(
    const blink::WebMediaStream& stream) {
  blink::WebVector<blink::WebMediaStreamTrack> tracks;
  stream.AudioTracks(tracks);
  for (size_t i = 0; i < tracks.size(); ++i)
    tracks[i].Source().SetReadyState(
        blink::WebMediaStreamSource::kReadyStateEnded);
  stream.VideoTracks(tracks);
  for (size_t i = 0; i < tracks.size(); ++i)
    tracks[i].Source().SetReadyState(
        blink::WebMediaStreamSource::kReadyStateEnded);
}

bool MockWebMediaStreamCenter::DidStopMediaStreamTrack(
    const blink::WebMediaStreamTrack& track) {
  track.Source().SetReadyState(blink::WebMediaStreamSource::kReadyStateEnded);
  return true;
}

void MockWebMediaStreamCenter::DidCreateMediaStream(
    blink::WebMediaStream& stream) {
  blink::WebVector<blink::WebMediaStreamTrack> audio_tracks;
  stream.AudioTracks(audio_tracks);
  for (size_t i = 0; i < audio_tracks.size(); ++i) {
    blink::WebMediaStreamSource source = audio_tracks[i].Source();
    if (source.RequiresAudioConsumer()) {
      MockWebAudioDestinationConsumer* consumer =
          new MockWebAudioDestinationConsumer();
      source.AddAudioConsumer(consumer);
      source.RemoveAudioConsumer(consumer);
      delete consumer;
    }
  }
}

blink::WebAudioSourceProvider*
MockWebMediaStreamCenter::CreateWebAudioSourceFromMediaStreamTrack(
    const blink::WebMediaStreamTrack& track) {
  return NULL;
}

}  // namespace test_runner
