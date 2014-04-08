// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream.h"

#include "base/logging.h"
#include "content/renderer/media/media_stream_dependency_factory.h"
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

MediaStream::MediaStream(MediaStreamDependencyFactory* factory,
                         StreamStopCallback stream_stop,
                         const blink::WebMediaStream& stream)
    : stream_stop_callback_(stream_stop),
      stream_adapter_(NULL),
      is_local_(true),
      label_(stream.id().utf8()),
      factory_(factory) {
  DCHECK(factory_);
}

MediaStream::MediaStream(webrtc::MediaStreamInterface* stream)
    : stream_adapter_(stream),
      is_local_(false),
      factory_(NULL) {
  DCHECK(stream);
}

MediaStream::~MediaStream() {
}

void MediaStream::OnStreamStopped() {
  if (!stream_stop_callback_.is_null())
    stream_stop_callback_.Run(label_);
}

webrtc::MediaStreamInterface* MediaStream::GetWebRtcAdapter(
    const blink::WebMediaStream& stream) {
  if (!stream_adapter_) {
    DCHECK(is_local_);
    stream_adapter_ = factory_->CreateNativeLocalMediaStream(stream);
  }
  DCHECK(stream_adapter_);
  return stream_adapter_.get();
}

bool MediaStream::AddTrack(const blink::WebMediaStream& stream,
                           const blink::WebMediaStreamTrack& track) {
  // If the libjingle representation of the stream has not been created, it
  // does not matter if the tracks are added or removed. Once the
  // libjingle representation is created, a libjingle track representation will
  // be created for all blink tracks.
  if (!stream_adapter_)
    return true;
  return factory_->AddNativeMediaStreamTrack(stream, track);
}

bool MediaStream::RemoveTrack(const blink::WebMediaStream& stream,
                              const blink::WebMediaStreamTrack& track) {
  // If the libjingle representation of the stream has not been created, it
  // does not matter if the tracks are added or removed.
  if (!stream_adapter_)
    return true;
  return factory_->RemoveNativeMediaStreamTrack(stream, track);
}

}  // namespace content
