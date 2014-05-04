// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/test_runner/MockWebMediaStreamCenter.h"

#include "base/logging.h"
#include "content/shell/renderer/test_runner/TestInterfaces.h"
#include "content/shell/renderer/test_runner/WebTestDelegate.h"
#include "third_party/WebKit/public/platform/WebAudioDestinationConsumer.h"
#include "third_party/WebKit/public/platform/WebAudioSourceProvider.h"
#include "third_party/WebKit/public/platform/WebMediaStream.h"
#include "third_party/WebKit/public/platform/WebMediaStreamCenterClient.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrackSourcesRequest.h"
#include "third_party/WebKit/public/platform/WebSourceInfo.h"
#include "third_party/WebKit/public/platform/WebVector.h"

using namespace blink;

namespace content {

class NewTrackTask : public WebMethodTask<MockWebMediaStreamCenter> {
public:
    NewTrackTask(MockWebMediaStreamCenter* object, const WebMediaStream& stream)
        : WebMethodTask<MockWebMediaStreamCenter>(object)
        , m_stream(stream)
    {
        DCHECK(!m_stream.isNull());
    }

    virtual void runIfValid() OVERRIDE
    {
        WebMediaStreamSource source;
        WebMediaStreamTrack track;
        source.initialize("MagicVideoDevice#1", WebMediaStreamSource::TypeVideo, "Magic video track");
        track.initialize(source);
        m_stream.addTrack(track);
    }

private:
    WebMediaStream m_stream;
};

MockWebMediaStreamCenter::MockWebMediaStreamCenter(WebMediaStreamCenterClient* client, TestInterfaces* interfaces)
    : m_interfaces(interfaces)
{
}

bool MockWebMediaStreamCenter::getMediaStreamTrackSources(const WebMediaStreamTrackSourcesRequest& request)
{
    size_t size = 2;
    WebVector<WebSourceInfo> results(size);
    results[0].initialize("MockAudioDevice#1", WebSourceInfo::SourceKindAudio, "Mock audio device", WebSourceInfo::VideoFacingModeNone);
    results[1].initialize("MockVideoDevice#1", WebSourceInfo::SourceKindVideo, "Mock video device", WebSourceInfo::VideoFacingModeEnvironment);
    request.requestSucceeded(results);
    return true;
}

void MockWebMediaStreamCenter::didEnableMediaStreamTrack(const WebMediaStreamTrack& track)
{
    track.source().setReadyState(WebMediaStreamSource::ReadyStateLive);
}

void MockWebMediaStreamCenter::didDisableMediaStreamTrack(const WebMediaStreamTrack& track)
{
    track.source().setReadyState(WebMediaStreamSource::ReadyStateMuted);
}

bool MockWebMediaStreamCenter::didAddMediaStreamTrack(const WebMediaStream&, const WebMediaStreamTrack&)
{
    return true;
}

bool MockWebMediaStreamCenter::didRemoveMediaStreamTrack(const WebMediaStream&, const WebMediaStreamTrack&)
{
    return true;
}

void MockWebMediaStreamCenter::didStopLocalMediaStream(const WebMediaStream& stream)
{
    WebVector<WebMediaStreamTrack> tracks;
    stream.audioTracks(tracks);
    for (size_t i = 0; i < tracks.size(); ++i)
        tracks[i].source().setReadyState(WebMediaStreamSource::ReadyStateEnded);
    stream.videoTracks(tracks);
    for (size_t i = 0; i < tracks.size(); ++i)
        tracks[i].source().setReadyState(WebMediaStreamSource::ReadyStateEnded);
}

bool MockWebMediaStreamCenter::didStopMediaStreamTrack(const blink::WebMediaStreamTrack& track)
{
    track.source().setReadyState(WebMediaStreamSource::ReadyStateEnded);
    return true;
}

class MockWebAudioDestinationConsumer : public WebAudioDestinationConsumer {
public:
    virtual ~MockWebAudioDestinationConsumer() { }
    virtual void setFormat(size_t numberOfChannels, float sampleRate) OVERRIDE { }
    virtual void consumeAudio(const WebVector<const float*>&, size_t number_of_frames) OVERRIDE { }
};

void MockWebMediaStreamCenter::didCreateMediaStream(WebMediaStream& stream)
{
    WebVector<WebMediaStreamTrack> audioTracks;
    stream.audioTracks(audioTracks);
    for (size_t i = 0; i < audioTracks.size(); ++i) {
        WebMediaStreamSource source = audioTracks[i].source();
        if (source.requiresAudioConsumer()) {
            MockWebAudioDestinationConsumer* consumer = new MockWebAudioDestinationConsumer();
            source.addAudioConsumer(consumer);
            source.removeAudioConsumer(consumer);
            delete consumer;
        }
    }
    m_interfaces->delegate()->postTask(new NewTrackTask(this, stream));
}

blink::WebAudioSourceProvider* MockWebMediaStreamCenter::createWebAudioSourceFromMediaStreamTrack(const blink::WebMediaStreamTrack& track)
{
    return NULL;
}

}  // namespace content
