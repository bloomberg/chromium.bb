// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/mock_media_stream_impl.h"

#include <utility>

#include "base/utf_string_conversions.h"
#include "content/renderer/media/rtc_video_decoder.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebMediaStreamDescriptor.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"

MockMediaStreamImpl::MockMediaStreamImpl()
    : MediaStreamImpl(NULL, NULL, NULL, NULL, NULL) {
}

MockMediaStreamImpl::~MockMediaStreamImpl() {}

WebKit::WebPeerConnectionHandler*
MockMediaStreamImpl::CreatePeerConnectionHandler(
    WebKit::WebPeerConnectionHandlerClient* client) {
  NOTIMPLEMENTED();
  return NULL;
}

void MockMediaStreamImpl::ClosePeerConnection(
    PeerConnectionHandlerBase* pc_handler) {
}

webrtc::LocalMediaStreamInterface* MockMediaStreamImpl::GetLocalMediaStream(
    const WebKit::WebMediaStreamDescriptor& stream) {
  MockMediaStreamPtrMap::iterator it = mock_local_streams_.find(
      UTF16ToUTF8(stream.label()));
  if (it == mock_local_streams_.end())
    return NULL;
  return it->second;
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

void MockMediaStreamImpl::AddLocalStream(
    webrtc::LocalMediaStreamInterface* stream) {
  mock_local_streams_[stream->label()] =
      talk_base::scoped_refptr<webrtc::LocalMediaStreamInterface>(stream);
}
