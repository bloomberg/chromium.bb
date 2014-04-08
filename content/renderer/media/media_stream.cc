// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream.h"

#include "base/logging.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/libjingle/source/talk/app/webrtc/mediastreaminterface.h"

namespace content {

// static
MediaStream* MediaStream::GetMediaStream(
    const blink::WebMediaStream& stream) {
  return static_cast<MediaStream*>(stream.extraData());
}

// static
webrtc::MediaStreamInterface* MediaStream::GetAdapter(
     const blink::WebMediaStream& stream) {
  MediaStream* native_stream = GetMediaStream(stream);
  DCHECK(native_stream);
  return native_stream->GetWebRtcAdapter(stream);
}

MediaStream::MediaStream(StreamStopCallback stream_stop,
                         const blink::WebMediaStream& stream)
    : stream_stop_callback_(stream_stop),
      is_local_(true),
      label_(stream.id().utf8()) {
}

MediaStream::MediaStream(webrtc::MediaStreamInterface* webrtc_stream)
    : is_local_(false),
      webrtc_media_stream_(webrtc_stream) {
}

MediaStream::~MediaStream() {
}

void MediaStream::OnStreamStopped() {
  if (!stream_stop_callback_.is_null())
    stream_stop_callback_.Run(label_);
}

webrtc::MediaStreamInterface* MediaStream::GetWebRtcAdapter(
    const blink::WebMediaStream& stream) {
  DCHECK(webrtc_media_stream_);
  return webrtc_media_stream_.get();
}

bool MediaStream::AddTrack(const blink::WebMediaStreamTrack& track) {
  // This method can be implemented if any clients need to know that a track
  // has been added to a MediaStream.
  NOTIMPLEMENTED();
  return true;
}

bool MediaStream::RemoveTrack(const blink::WebMediaStreamTrack& track) {
  // This method can be implemented if any clients need to know that a track
  // has been removed from a MediaStream.
  NOTIMPLEMENTED();
  return true;
}

}  // namespace content
