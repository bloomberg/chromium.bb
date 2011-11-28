// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/mock_media_stream_impl.h"

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
  video_label_.clear();
}

bool MockMediaStreamImpl::SetVideoCaptureModule(const std::string& label) {
  video_label_ = label;
  return true;
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
