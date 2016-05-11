// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/mock_media_stream_registry.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "content/renderer/media/media_stream.h"
#include "content/renderer/media/media_stream_audio_source.h"
#include "content/renderer/media/media_stream_video_track.h"
#include "content/renderer/media/mock_media_stream_video_source.h"
#include "content/renderer/media/webrtc/webrtc_local_audio_track_adapter.h"
#include "content/renderer/media/webrtc_local_audio_track.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebVector.h"

namespace content {

static const char kTestStreamLabel[] = "stream_label";

MockMediaStreamRegistry::MockMediaStreamRegistry() {}

void MockMediaStreamRegistry::Init(const std::string& stream_url) {
  stream_url_ = stream_url;
  const blink::WebVector<blink::WebMediaStreamTrack> webkit_audio_tracks;
  const blink::WebVector<blink::WebMediaStreamTrack> webkit_video_tracks;
  const blink::WebString label(kTestStreamLabel);
  test_stream_.initialize(label, webkit_audio_tracks, webkit_video_tracks);
  test_stream_.setExtraData(new MediaStream());
}

void MockMediaStreamRegistry::AddVideoTrack(const std::string& track_id) {
  blink::WebMediaStreamSource blink_source;
  blink_source.initialize("mock video source id",
                          blink::WebMediaStreamSource::TypeVideo,
                          "mock video source name",
                          false /* remote */);
  MockMediaStreamVideoSource* native_source =
      new MockMediaStreamVideoSource(false /* manual get supported formats */);
  blink_source.setExtraData(native_source);
  blink::WebMediaStreamTrack blink_track;
  blink_track.initialize(base::UTF8ToUTF16(track_id), blink_source);
  blink::WebMediaConstraints constraints;
  constraints.initialize();

  MediaStreamVideoTrack* native_track = new MediaStreamVideoTrack(
      native_source, constraints, MediaStreamVideoSource::ConstraintsCallback(),
      true /* enabled */);
  blink_track.setExtraData(native_track);
  test_stream_.addTrack(blink_track);
}

void MockMediaStreamRegistry::AddAudioTrack(const std::string& track_id) {
  blink::WebMediaStreamSource audio_source;
  audio_source.initialize(
      "mock audio source id", blink::WebMediaStreamSource::TypeAudio,
      "mock audio source name", false /* remote */);
  audio_source.setExtraData(new MediaStreamAudioSource());
  blink::WebMediaStreamTrack blink_track;
  blink_track.initialize(audio_source);
  const scoped_refptr<WebRtcLocalAudioTrackAdapter> adapter(
      WebRtcLocalAudioTrackAdapter::Create(track_id,
                                           nullptr /* track source */));
  std::unique_ptr<WebRtcLocalAudioTrack> native_track(
      new WebRtcLocalAudioTrack(adapter.get()));
  blink_track.setExtraData(native_track.release());
  test_stream_.addTrack(blink_track);
}

blink::WebMediaStream MockMediaStreamRegistry::GetMediaStream(
    const std::string& url) {
  return (url != stream_url_) ? blink::WebMediaStream() : test_stream_;
}

}  // namespace content
