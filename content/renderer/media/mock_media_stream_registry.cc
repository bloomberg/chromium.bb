// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/mock_media_stream_registry.h"

#include <string>

#include "base/utf_string_conversions.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebMediaStreamSource.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebMediaStreamTrack.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebString.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebVector.h"
#include "third_party/libjingle/source/talk/media/base/videocapturer.h"

namespace content {

static const std::string kTestStreamLabel = "stream_label";

MockMediaStreamRegistry::MockMediaStreamRegistry(
    MockMediaStreamDependencyFactory* factory)
    : factory_(factory) {
}

void MockMediaStreamRegistry::Init(const std::string& stream_url) {
  stream_url_ = stream_url;
  scoped_refptr<webrtc::MediaStreamInterface> stream(
      factory_->CreateLocalMediaStream(kTestStreamLabel));
  WebKit::WebVector<WebKit::WebMediaStreamTrack> webkit_audio_tracks;
  WebKit::WebVector<WebKit::WebMediaStreamTrack> webkit_video_tracks;
  WebKit::WebString webkit_stream_label(UTF8ToUTF16(stream->label()));
  test_stream_.initialize(webkit_stream_label,
                          webkit_audio_tracks, webkit_video_tracks);
  test_stream_.setExtraData(new MediaStreamExtraData(stream, false));
}

bool MockMediaStreamRegistry::AddVideoTrack(const std::string& track_id) {
  cricket::VideoCapturer* capturer = NULL;
  return factory_->AddNativeVideoMediaTrack(track_id, &test_stream_, capturer);
}

WebKit::WebMediaStream MockMediaStreamRegistry::GetMediaStream(
    const std::string& url) {
  if (url != stream_url_) {
    return WebKit::WebMediaStream();
  }
  return test_stream_;
}

const WebKit::WebMediaStream MockMediaStreamRegistry::test_stream() const {
  return test_stream_;
}

}  // namespace content
