// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/mock_media_stream_impl.h"

#include <utility>

#include "content/renderer/media/rtc_video_decoder.h"

MockMediaStreamImpl::MockMediaStreamImpl()
    : MediaStreamImpl(NULL, NULL, NULL, NULL) {
}

MockMediaStreamImpl::~MockMediaStreamImpl() {}

WebKit::WebPeerConnectionHandler*
MockMediaStreamImpl::CreatePeerConnectionHandler(
    WebKit::WebPeerConnectionHandlerClient* client) {
  NOTIMPLEMENTED();
  return NULL;
}

void MockMediaStreamImpl::ClosePeerConnection() {
}

webrtc::MediaStreamTrackInterface*
MockMediaStreamImpl::GetLocalMediaStreamTrack(const std::string& label) {
  MockMediaStreamTrackPtrMap::iterator it = mock_local_tracks_.find(label);
  if (it == mock_local_tracks_.end())
    return NULL;
  webrtc::MediaStreamTrackInterface* stream = it->second;
  return stream;
}

scoped_refptr<media::VideoDecoder> MockMediaStreamImpl::GetVideoDecoder(
    const GURL& url,
    media::MessageLoopFactory* message_loop_factory) {
  NOTIMPLEMENTED();
  return NULL;
}

void MockMediaStreamImpl::OnStreamGenerated(
    int request_id,
    const std::string& label,
    const media_stream::StreamDeviceInfoArray& audio_array,
    const media_stream::StreamDeviceInfoArray& video_array) {
  NOTIMPLEMENTED();
}

void MockMediaStreamImpl::OnStreamGenerationFailed(int request_id) {
  NOTIMPLEMENTED();
}

void MockMediaStreamImpl::OnVideoDeviceFailed(
    const std::string& label,
    int index) {
  NOTIMPLEMENTED();
}

void MockMediaStreamImpl::OnAudioDeviceFailed(
    const std::string& label,
    int index) {
  NOTIMPLEMENTED();
}

void MockMediaStreamImpl::AddTrack(
    const std::string& label,
    webrtc::MediaStreamTrackInterface* track) {
  mock_local_tracks_.insert(
      std::pair<std::string, webrtc::MediaStreamTrackInterface*>(label, track));
}
