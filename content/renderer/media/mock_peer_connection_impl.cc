// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/mock_peer_connection_impl.h"

#include "base/logging.h"

namespace webrtc {

MockPeerConnectionImpl::MockPeerConnectionImpl()
    : observer_(NULL),
      video_stream_(false),
      connected_(false),
      video_capture_set_(false) {
}

MockPeerConnectionImpl::~MockPeerConnectionImpl() {}

void MockPeerConnectionImpl::RegisterObserver(
    PeerConnectionObserver* observer) {
  observer_ = observer;
}

bool MockPeerConnectionImpl::SignalingMessage(
    const std::string& signaling_message) {
  signaling_message_ = signaling_message;
  return true;
}

bool MockPeerConnectionImpl::AddStream(
    const std::string& stream_id,
    bool video) {
  stream_id_ = stream_id;
  video_stream_ = video;
  return true;
}

bool MockPeerConnectionImpl::RemoveStream(const std::string& stream_id) {
  stream_id_.clear();
  video_stream_ = false;
  return true;
}

bool MockPeerConnectionImpl::Connect() {
  connected_ = true;
  return true;
}

bool MockPeerConnectionImpl::Close() {
  observer_ = NULL;
  signaling_message_.clear();
  stream_id_.clear();
  video_stream_ = false;
  connected_ = false;
  video_capture_set_ = false;
  return true;
}

bool MockPeerConnectionImpl::SetAudioDevice(
    const std::string& wave_in_device,
    const std::string& wave_out_device,
    int opts) {
  NOTIMPLEMENTED();
  return false;
}

bool MockPeerConnectionImpl::SetLocalVideoRenderer(
    cricket::VideoRenderer* renderer) {
  NOTIMPLEMENTED();
  return false;
}

bool MockPeerConnectionImpl::SetVideoRenderer(
    const std::string& stream_id,
    cricket::VideoRenderer* renderer) {
  video_renderer_stream_id_ = stream_id;
  return true;
}

bool MockPeerConnectionImpl::SetVideoCapture(const std::string& cam_device) {
  video_capture_set_ = true;
  return true;
}

MockPeerConnectionImpl::ReadyState MockPeerConnectionImpl::GetReadyState() {
  NOTIMPLEMENTED();
  return NEW;
}

}  // namespace webrtc
