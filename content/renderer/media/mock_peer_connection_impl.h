// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MOCK_PEER_CONNECTION_IMPL_H_
#define CONTENT_RENDERER_MEDIA_MOCK_PEER_CONNECTION_IMPL_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "third_party/libjingle/source/talk/app/webrtc/peerconnection.h"

class MockMediaStreamDependencyFactory;

namespace webrtc {

class MockStreamCollection;

class MockPeerConnectionImpl : public PeerConnectionInterface {
 public:
  explicit MockPeerConnectionImpl(MockMediaStreamDependencyFactory* factory);

  // PeerConnectionInterface implementation.
  virtual void ProcessSignalingMessage(const std::string& msg) OVERRIDE;
  virtual bool Send(const std::string& msg) OVERRIDE;
  virtual talk_base::scoped_refptr<StreamCollectionInterface>
      local_streams() OVERRIDE;
  virtual talk_base::scoped_refptr<StreamCollectionInterface>
      remote_streams() OVERRIDE;
  virtual void AddStream(LocalMediaStreamInterface* stream) OVERRIDE;
  virtual void RemoveStream(LocalMediaStreamInterface* stream) OVERRIDE;
  virtual bool RemoveStream(const std::string& label) OVERRIDE;
  virtual void CommitStreamChanges() OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual ReadyState ready_state() OVERRIDE;
  virtual SdpState sdp_state() OVERRIDE;
  virtual bool StartIce(IceOptions options) OVERRIDE;

  virtual webrtc::SessionDescriptionInterface* CreateOffer(
      const webrtc::MediaHints& hints) OVERRIDE;
  virtual webrtc::SessionDescriptionInterface* CreateAnswer(
      const webrtc::MediaHints& hints,
      const webrtc::SessionDescriptionInterface* offer) OVERRIDE;
  virtual bool SetLocalDescription(
      Action action,
      webrtc::SessionDescriptionInterface* desc) OVERRIDE;
  virtual bool SetRemoteDescription(
      Action action,
      webrtc::SessionDescriptionInterface* desc) OVERRIDE;
  virtual bool ProcessIceMessage(
      const webrtc::IceCandidateInterface* ice_candidate) OVERRIDE;
  virtual const webrtc::SessionDescriptionInterface* local_description()
      const OVERRIDE;
  virtual const webrtc::SessionDescriptionInterface* remote_description()
      const OVERRIDE;

  void AddRemoteStream(MediaStreamInterface* stream);
  void ClearStreamChangesCommitted() { stream_changes_committed_ = false; }
  void SetReadyState(ReadyState state) { ready_state_ = state; }

  const std::string& signaling_message() const { return signaling_message_; }
  const std::string& stream_label() const { return stream_label_; }
  bool stream_changes_committed() const { return stream_changes_committed_; }
  bool hint_audio() const { return hint_audio_; }
  bool hint_video() const { return hint_video_; }
  Action action() const { return action_; }
  const std::string& description_sdp() const { return description_sdp_; }
  IceOptions ice_options() const { return ice_options_; }
  const std::string& ice_label() const { return ice_label_; }
  const std::string& ice_sdp() const { return ice_sdp_; }

  static const char kDummyOffer[];

 protected:
  virtual ~MockPeerConnectionImpl();

 private:
  // Used for creating MockSessionDescription.
  MockMediaStreamDependencyFactory* dependency_factory_;

  std::string signaling_message_;
  std::string stream_label_;
  bool stream_changes_committed_;
  talk_base::scoped_refptr<MockStreamCollection> local_streams_;
  talk_base::scoped_refptr<MockStreamCollection> remote_streams_;
  scoped_ptr<webrtc::SessionDescriptionInterface> local_desc_;
  scoped_ptr<webrtc::SessionDescriptionInterface> remote_desc_;
  bool hint_audio_;
  bool hint_video_;
  Action action_;
  std::string description_sdp_;
  IceOptions ice_options_;
  std::string ice_label_;
  std::string ice_sdp_;
  ReadyState ready_state_;

  DISALLOW_COPY_AND_ASSIGN(MockPeerConnectionImpl);
};

}  // namespace webrtc

#endif  // CONTENT_RENDERER_MEDIA_MOCK_PEER_CONNECTION_IMPL_H_
