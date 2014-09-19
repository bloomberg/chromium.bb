// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/test_runner/mock_web_media_stream_center.h"

#include "base/logging.h"
#include "content/shell/renderer/test_runner/test_interfaces.h"
#include "content/shell/renderer/test_runner/web_test_delegate.h"
#include "third_party/WebKit/public/platform/WebAudioDestinationConsumer.h"
#include "third_party/WebKit/public/platform/WebAudioSourceProvider.h"
#include "third_party/WebKit/public/platform/WebMediaStream.h"
#include "third_party/WebKit/public/platform/WebMediaStreamCenterClient.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrackSourcesRequest.h"
#include "third_party/WebKit/public/platform/WebSourceInfo.h"
#include "third_party/WebKit/public/platform/WebVector.h"

namespace content {

namespace {
class NewTrackTask : public WebMethodTask<MockWebMediaStreamCenter> {
 public:
  NewTrackTask(MockWebMediaStreamCenter* object,
               const blink::WebMediaStream& stream)
      : WebMethodTask<MockWebMediaStreamCenter>(object), stream_(stream) {
    DCHECK(!stream_.isNull());
  }

  virtual ~NewTrackTask() {}

  virtual void RunIfValid() OVERRIDE {
    blink::WebMediaStreamSource source;
    blink::WebMediaStreamTrack track;
    source.initialize("MagicVideoDevice#1",
                      blink::WebMediaStreamSource::TypeVideo,
                      "Magic video track");
    track.initialize(source);
    stream_.addTrack(track);
  }

 private:
  blink::WebMediaStream stream_;

  DISALLOW_COPY_AND_ASSIGN(NewTrackTask);
};

class MockWebAudioDestinationConsumer
    : public blink::WebAudioDestinationConsumer {
 public:
  MockWebAudioDestinationConsumer() {}
  virtual ~MockWebAudioDestinationConsumer() {}
  virtual void setFormat(size_t number_of_channels, float sample_rate) {}
  virtual void consumeAudio(const blink::WebVector<const float*>&,
                            size_t number_of_frames) {}

  DISALLOW_COPY_AND_ASSIGN(MockWebAudioDestinationConsumer);
};

}  // namespace

MockWebMediaStreamCenter::MockWebMediaStreamCenter(
    blink::WebMediaStreamCenterClient* client,
    TestInterfaces* interfaces)
    : interfaces_(interfaces) {
}

MockWebMediaStreamCenter::~MockWebMediaStreamCenter() {
}

bool MockWebMediaStreamCenter::getMediaStreamTrackSources(
    const blink::WebMediaStreamTrackSourcesRequest& request) {
  size_t size = 2;
  blink::WebVector<blink::WebSourceInfo> results(size);
  results[0].initialize("MockAudioDevice#1",
                        blink::WebSourceInfo::SourceKindAudio,
                        "Mock audio device",
                        blink::WebSourceInfo::VideoFacingModeNone);
  results[1].initialize("MockVideoDevice#1",
                        blink::WebSourceInfo::SourceKindVideo,
                        "Mock video device",
                        blink::WebSourceInfo::VideoFacingModeEnvironment);
  request.requestSucceeded(results);
  return true;
}

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
  interfaces_->GetDelegate()->PostTask(new NewTrackTask(this, stream));
}

blink::WebAudioSourceProvider*
MockWebMediaStreamCenter::createWebAudioSourceFromMediaStreamTrack(
    const blink::WebMediaStreamTrack& track) {
  return NULL;
}

}  // namespace content
