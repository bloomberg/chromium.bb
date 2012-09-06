// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "content/renderer/media/mock_media_stream_dependency_factory.h"
#include "content/renderer/media/mock_peer_connection_impl.h"
#include "third_party/libjingle/source/talk/app/webrtc/mediastreaminterface.h"
#include "third_party/libjingle/source/talk/base/scoped_ref_ptr.h"

namespace webrtc {

template <class TrackType>
class MockMediaStreamTrackList
    : public MediaStreamTrackListInterface<TrackType> {
 public:
  virtual size_t count() const OVERRIDE {
    return tracks_.size();
  }
  virtual TrackType* at(size_t index) OVERRIDE {
    return tracks_[index];
  }
  void AddTrack(TrackType* track) {
    tracks_.push_back(track);
  }

 protected:
  virtual ~MockMediaStreamTrackList() {}

 private:
  std::vector<talk_base::scoped_refptr<TrackType> > tracks_;
};

typedef MockMediaStreamTrackList<AudioTrackInterface> MockAudioTracks;
typedef MockMediaStreamTrackList<VideoTrackInterface> MockVideoTracks;

class MockLocalMediaStream : public LocalMediaStreamInterface {
 public:
  explicit MockLocalMediaStream(std::string label)
    : label_(label),
      audio_tracks_(new talk_base::RefCountedObject<MockAudioTracks>),
      video_tracks_(new talk_base::RefCountedObject<MockVideoTracks>) {
  }
  virtual bool AddTrack(AudioTrackInterface* track) OVERRIDE {
    audio_tracks_->AddTrack(track);
    return true;
  }
  virtual bool AddTrack(VideoTrackInterface* track) OVERRIDE {
    video_tracks_->AddTrack(track);
    return true;
  }
  virtual std::string label() const OVERRIDE { return label_; }
  virtual AudioTracks* audio_tracks() OVERRIDE {
    return audio_tracks_;
  }
  virtual VideoTracks* video_tracks() OVERRIDE {
    return video_tracks_;
  }
  virtual ReadyState ready_state() const OVERRIDE {
    NOTIMPLEMENTED();
    return kInitializing;
  }
  virtual void set_ready_state(ReadyState state) OVERRIDE { NOTIMPLEMENTED(); }
  virtual void RegisterObserver(ObserverInterface* observer) OVERRIDE {
    NOTIMPLEMENTED();
  }
  virtual void UnregisterObserver(ObserverInterface* observer) {
    NOTIMPLEMENTED();
  }

 protected:
  virtual ~MockLocalMediaStream() {}

 private:
  std::string label_;
  talk_base::scoped_refptr<MockAudioTracks> audio_tracks_;
  talk_base::scoped_refptr<MockVideoTracks> video_tracks_;
};

cricket::VideoCapturer* MockLocalVideoTrack::GetVideoCapture() {
  NOTIMPLEMENTED();
  return NULL;
}

void MockLocalVideoTrack::AddRenderer(VideoRendererInterface* renderer) {
  NOTIMPLEMENTED();
}

void MockLocalVideoTrack::RemoveRenderer(VideoRendererInterface* renderer) {
  NOTIMPLEMENTED();
}

cricket::VideoRenderer* MockLocalVideoTrack::FrameInput() {
  NOTIMPLEMENTED();
  return NULL;
}

std::string MockLocalVideoTrack::kind() const {
  NOTIMPLEMENTED();
  return "";
}

std::string MockLocalVideoTrack::label() const { return label_; }

bool MockLocalVideoTrack::enabled() const { return enabled_; }

MockLocalVideoTrack::TrackState MockLocalVideoTrack::state() const {
  NOTIMPLEMENTED();
  return kInitializing;
}

bool MockLocalVideoTrack::set_enabled(bool enable) {
  enabled_ = enable;
  return true;
}

bool MockLocalVideoTrack::set_state(TrackState new_state) {
  NOTIMPLEMENTED();
  return false;
}

void MockLocalVideoTrack::RegisterObserver(ObserverInterface* observer) {
  NOTIMPLEMENTED();
}

void MockLocalVideoTrack::UnregisterObserver(ObserverInterface* observer) {
  NOTIMPLEMENTED();
}

AudioDeviceModule* MockLocalAudioTrack::GetAudioDevice() {
  NOTIMPLEMENTED();
  return NULL;
}

std::string MockLocalAudioTrack::kind() const {
  NOTIMPLEMENTED();
  return "";
}

std::string MockLocalAudioTrack::label() const { return label_; }

bool MockLocalAudioTrack::enabled() const { return enabled_; }

MockLocalVideoTrack::TrackState MockLocalAudioTrack::state() const {
  NOTIMPLEMENTED();
  return kInitializing;
}

bool MockLocalAudioTrack::set_enabled(bool enable) {
  enabled_ = enable;
  return true;
}

bool MockLocalAudioTrack::set_state(TrackState new_state) {
  NOTIMPLEMENTED();
  return false;
}

void MockLocalAudioTrack::RegisterObserver(ObserverInterface* observer) {
  NOTIMPLEMENTED();
}

void MockLocalAudioTrack::UnregisterObserver(ObserverInterface* observer) {
  NOTIMPLEMENTED();
}

class MockSessionDescription : public SessionDescriptionInterface {
 public:
  MockSessionDescription(const std::string& sdp)
      : sdp_(sdp) {
  }
  virtual ~MockSessionDescription() {}
  virtual const cricket::SessionDescription* description() const OVERRIDE {
    NOTIMPLEMENTED();
    return NULL;
  }
  virtual std::string session_id() const OVERRIDE {
    NOTIMPLEMENTED();
    return "";
  }
  virtual std::string session_version() const OVERRIDE {
    NOTIMPLEMENTED();
    return "";
  }
  virtual std::string type() const OVERRIDE {
    NOTIMPLEMENTED();
    return "";
  }
  virtual bool AddCandidate(const IceCandidateInterface* candidate) OVERRIDE {
    NOTIMPLEMENTED();
    return false;
  }
  virtual size_t number_of_mediasections() const OVERRIDE {
    NOTIMPLEMENTED();
    return 0;
  }
  virtual const IceCandidateCollection* candidates(
      size_t mediasection_index) const OVERRIDE {
    NOTIMPLEMENTED();
    return NULL;
  }

  virtual bool ToString(std::string* out) const OVERRIDE {
    *out = sdp_;
    return true;
  }

 private:
  std::string sdp_;
};

class MockIceCandidate : public IceCandidateInterface {
 public:
  MockIceCandidate(const std::string& sdp_mid,
                   int sdp_mline_index,
                   const std::string& sdp)
      : sdp_mid_(sdp_mid),
        sdp_mline_index_(sdp_mline_index),
        sdp_(sdp) {
  }
  virtual ~MockIceCandidate() {}
  virtual std::string sdp_mid() const OVERRIDE {
    return sdp_mid_;
  }
  virtual int sdp_mline_index() const OVERRIDE {
    return sdp_mline_index_;
  }
  virtual const cricket::Candidate& candidate() const OVERRIDE {
    // This function should never be called. It will intentionally crash. The
    // base class forces us to return a reference.
    NOTREACHED();
    cricket::Candidate* candidate = NULL;
    return *candidate;
  }
  virtual bool ToString(std::string* out) const OVERRIDE {
    *out = sdp_;
    return true;
  }

 private:
  std::string sdp_mid_;
  int sdp_mline_index_;
  std::string sdp_;
};

}  // namespace webrtc

MockMediaStreamDependencyFactory::MockMediaStreamDependencyFactory(
    VideoCaptureImplManager* vc_manager)
    : MediaStreamDependencyFactory(vc_manager),
      mock_pc_factory_created_(false) {
}

MockMediaStreamDependencyFactory::~MockMediaStreamDependencyFactory() {}

bool MockMediaStreamDependencyFactory::CreatePeerConnectionFactory(
    talk_base::Thread* worker_thread,
    talk_base::Thread* signaling_thread,
    content::P2PSocketDispatcher* socket_dispatcher,
    talk_base::NetworkManager* network_manager,
    talk_base::PacketSocketFactory* socket_factory) {
  mock_pc_factory_created_ = true;
  return true;
}

void MockMediaStreamDependencyFactory::ReleasePeerConnectionFactory() {
  mock_pc_factory_created_ = false;
}

bool MockMediaStreamDependencyFactory::PeerConnectionFactoryCreated() {
  return mock_pc_factory_created_;
}

talk_base::scoped_refptr<webrtc::PeerConnectionInterface>
MockMediaStreamDependencyFactory::CreatePeerConnection(
    const std::string& config,
    webrtc::PeerConnectionObserver* observer) {
  DCHECK(mock_pc_factory_created_);
  return new talk_base::RefCountedObject<webrtc::MockPeerConnectionImpl>(this);
}

talk_base::scoped_refptr<webrtc::LocalMediaStreamInterface>
MockMediaStreamDependencyFactory::CreateLocalMediaStream(
    const std::string& label) {
  talk_base::scoped_refptr<webrtc::LocalMediaStreamInterface> stream(
      new talk_base::RefCountedObject<webrtc::MockLocalMediaStream>(label));
  return stream;
}

talk_base::scoped_refptr<webrtc::LocalVideoTrackInterface>
MockMediaStreamDependencyFactory::CreateLocalVideoTrack(
    const std::string& label,
    int video_session_id) {
  talk_base::scoped_refptr<webrtc::LocalVideoTrackInterface> track(
      new talk_base::RefCountedObject<webrtc::MockLocalVideoTrack>(label));
  return track;
}

talk_base::scoped_refptr<webrtc::LocalAudioTrackInterface>
MockMediaStreamDependencyFactory::CreateLocalAudioTrack(
    const std::string& label,
    webrtc::AudioDeviceModule* audio_device) {
  talk_base::scoped_refptr<webrtc::LocalAudioTrackInterface> track(
      new talk_base::RefCountedObject<webrtc::MockLocalAudioTrack>(label));
  return track;
}

webrtc::SessionDescriptionInterface*
MockMediaStreamDependencyFactory::CreateSessionDescription(
    const std::string& sdp) {
  return new webrtc::MockSessionDescription(sdp);
}

webrtc::IceCandidateInterface*
MockMediaStreamDependencyFactory::CreateIceCandidate(
    const std::string& sdp_mid,
    int sdp_mline_index,
    const std::string& sdp) {
  return new webrtc::MockIceCandidate(sdp_mid, sdp_mline_index, sdp);
}

void MockMediaStreamDependencyFactory::SetAudioDeviceSessionId(int session_id) {
}
