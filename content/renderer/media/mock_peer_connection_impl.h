// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MOCK_PEER_CONNECTION_IMPL_H_
#define CONTENT_RENDERER_MEDIA_MOCK_PEER_CONNECTION_IMPL_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "third_party/libjingle/source/talk/app/webrtc/peerconnection.h"

namespace webrtc {

class MockPeerConnectionImpl : public PeerConnection {
 public:
  MockPeerConnectionImpl();
  virtual ~MockPeerConnectionImpl();

  // PeerConnection implementation.
  virtual void RegisterObserver(PeerConnectionObserver* observer) OVERRIDE;
  virtual bool SignalingMessage(const std::string& msg) OVERRIDE;
  virtual bool AddStream(const std::string& stream_id, bool video) OVERRIDE;
  virtual bool RemoveStream(const std::string& stream_id) OVERRIDE;
  virtual bool Connect() OVERRIDE;
  virtual bool Close() OVERRIDE;
  virtual bool SetAudioDevice(
      const std::string& wave_in_device,
      const std::string& wave_out_device, int opts) OVERRIDE;
  virtual bool SetLocalVideoRenderer(cricket::VideoRenderer* renderer) OVERRIDE;
  virtual bool SetVideoRenderer(
      const std::string& stream_id,
      cricket::VideoRenderer* renderer) OVERRIDE;
  virtual bool SetVideoCapture(const std::string& cam_device) OVERRIDE;
  virtual ReadyState GetReadyState() OVERRIDE;

  PeerConnectionObserver* observer() const { return observer_; }
  const std::string& signaling_message() const { return signaling_message_; }
  const std::string& stream_id() const { return stream_id_; }
  bool video_stream() const { return video_stream_; }
  bool connected() const { return connected_; }
  bool video_capture_set() const { return video_capture_set_; }
  const std::string& video_renderer_stream_id() const {
    return video_renderer_stream_id_;
  }

 private:
  PeerConnectionObserver* observer_;
  std::string signaling_message_;
  std::string stream_id_;
  bool video_stream_;
  bool connected_;
  bool video_capture_set_;
  std::string video_renderer_stream_id_;

  DISALLOW_COPY_AND_ASSIGN(MockPeerConnectionImpl);
};

}  // namespace webrtc

#endif  // CONTENT_RENDERER_MEDIA_MOCK_PEER_CONNECTION_IMPL_H_
