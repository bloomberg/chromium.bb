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
  void setFormat(size_t number_of_channels, float sample_rate) override {}
  void consumeAudio(const blink::WebVector<const float*>&,
                    size_t number_of_frames) override {}

  DISALLOW_COPY_AND_ASSIGN(MockWebAudioDestinationConsumer);
};

}  // namespace

void MockWebMediaStreamCenter::didEnableMediaStreamTrack(
    const blink::WebMediaStreamTrack& track) {
  track.source().setReadyState(blink::WebMediaStreamSource::ReadyStateLive);
}

void MockWebMediaStreamCenter::didDisableMediaStreamTrack(
    const blink::WebMediaStreamTrack& track) {
  track.source().setReadyState(blink::WebMediaStreamSource::ReadyStateMuted);
}

bool MockWebMediaStreamCenter::didAddMediaStreamTrack(
    const blink::WebMediaStream& stream,
    const blink::WebMediaStreamTrack& track) {
  return true;
}

bool MockWebMediaStreamCenter::didRemoveMediaStreamTrack(
    const blink::WebMediaStream& stream,
    const blink::WebMediaStreamTrack& track) {
  return true;
}

void MockWebMediaStreamCenter::didStopLocalMediaStream(
    const blink::WebMediaStream& stream) {
  blink::WebVector<blink::WebMediaStreamTrack> tracks;
  stream.audioTracks(tracks);
  for (size_t i = 0; i < tracks.size(); ++i)
    tracks[i].source().setReadyState(
        blink::WebMediaStreamSource::ReadyStateEnded);
  stream.videoTracks(tracks);
  for (size_t i = 0; i < tracks.size(); ++i)
    tracks[i].source().setReadyState(
        blink::WebMediaStreamSource::ReadyStateEnded);
}

bool MockWebMediaStreamCenter::didStopMediaStreamTrack(
    const blink::WebMediaStreamTrack& track) {
  track.source().setReadyState(blink::WebMediaStreamSource::ReadyStateEnded);
  return true;
}

void MockWebMediaStreamCenter::didCreateMediaStream(
    blink::WebMediaStream& stream) {
  blink::WebVector<blink::WebMediaStreamTrack> audio_tracks;
  stream.audioTracks(audio_tracks);
  for (size_t i = 0; i < audio_tracks.size(); ++i) {
    blink::WebMediaStreamSource source = audio_tracks[i].source();
    if (source.requiresAudioConsumer()) {
      MockWebAudioDestinationConsumer* consumer =
          new MockWebAudioDestinationConsumer();
      source.addAudioConsumer(consumer);
      source.removeAudioConsumer(consumer);
      delete consumer;
    }
  }
}

blink::WebAudioSourceProvider*
MockWebMediaStreamCenter::createWebAudioSourceFromMediaStreamTrack(
    const blink::WebMediaStreamTrack& track) {
  return NULL;
}

}  // namespace test_runner
