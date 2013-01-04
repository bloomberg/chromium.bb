// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/mock_media_stream_dependency_factory.h"

#include "base/logging.h"
#include "content/renderer/media/mock_peer_connection_impl.h"
#include "third_party/libjingle/source/talk/app/webrtc/mediastreaminterface.h"
#include "third_party/libjingle/source/talk/base/scoped_ref_ptr.h"

using webrtc::AudioSourceInterface;
using webrtc::AudioTrackInterface;
using webrtc::IceCandidateCollection;
using webrtc::IceCandidateInterface;
using webrtc::LocalMediaStreamInterface;
using webrtc::ObserverInterface;
using webrtc::SessionDescriptionInterface;
using webrtc::VideoRendererInterface;
using webrtc::VideoSourceInterface;
using webrtc::VideoTrackInterface;

namespace content {

template <class TrackType>
class MockMediaStreamTrackList
    : public webrtc::MediaStreamTrackListInterface<TrackType> {
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
  std::vector<scoped_refptr<TrackType> > tracks_;
};

typedef MockMediaStreamTrackList<AudioTrackInterface> MockAudioTracks;
typedef MockMediaStreamTrackList<VideoTrackInterface> MockVideoTracks;

class MockLocalMediaStream : public webrtc::LocalMediaStreamInterface {
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
  virtual webrtc::AudioTracks* audio_tracks() OVERRIDE {
    return audio_tracks_;
  }
  virtual webrtc::VideoTracks* video_tracks() OVERRIDE {
    return video_tracks_;
  }
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
  scoped_refptr<MockAudioTracks> audio_tracks_;
  scoped_refptr<MockVideoTracks> video_tracks_;
};

MockVideoSource::MockVideoSource()
    : observer_(NULL),
      state_(MediaSourceInterface::kInitializing) {
}

MockVideoSource::~MockVideoSource() {}

cricket::VideoCapturer* MockVideoSource::GetVideoCapturer() {
  NOTIMPLEMENTED();
  return NULL;
}

void MockVideoSource::AddSink(cricket::VideoRenderer* output) {
  NOTIMPLEMENTED();
}

void MockVideoSource::RemoveSink(cricket::VideoRenderer* output) {
  NOTIMPLEMENTED();
}

void MockVideoSource::RegisterObserver(webrtc::ObserverInterface* observer) {
  observer_ = observer;
}

void MockVideoSource::UnregisterObserver(webrtc::ObserverInterface* observer) {
  DCHECK(observer_  == observer);
  observer_ = NULL;
}

void MockVideoSource::SetLive() {
  DCHECK_EQ(MediaSourceInterface::kInitializing, state_);
  state_ = MediaSourceInterface::kLive;
  if (observer_)
    observer_->OnChanged();
}

void MockVideoSource::SetEnded() {
  DCHECK_NE(MediaSourceInterface::kEnded, state_);
  state_ = MediaSourceInterface::kEnded;
  if (observer_)
    observer_->OnChanged();
}

webrtc::MediaSourceInterface::SourceState MockVideoSource::state() const {
  return state_;
}

MockLocalVideoTrack::MockLocalVideoTrack(std::string label,
                                         webrtc::VideoSourceInterface* source)
    : enabled_(false),
      label_(label),
      source_(source) {
}

MockLocalVideoTrack::~MockLocalVideoTrack() {}

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

VideoSourceInterface* MockLocalVideoTrack::GetSource() const {
  return source_;
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

AudioSourceInterface* MockLocalAudioTrack::GetSource() const {
  NOTIMPLEMENTED();
  return NULL;
}

class MockSessionDescription : public SessionDescriptionInterface {
 public:
  MockSessionDescription(const std::string& type,
                         const std::string& sdp)
      : type_(type),
        sdp_(sdp) {
  }
  virtual ~MockSessionDescription() {}
  virtual cricket::SessionDescription* description() OVERRIDE {
    NOTIMPLEMENTED();
    return NULL;
  }
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
    return type_;
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
  std::string type_;
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

MockMediaStreamDependencyFactory::MockMediaStreamDependencyFactory()
    : MediaStreamDependencyFactory(NULL, NULL),
      mock_pc_factory_created_(false) {
}

MockMediaStreamDependencyFactory::~MockMediaStreamDependencyFactory() {}

bool MockMediaStreamDependencyFactory::EnsurePeerConnectionFactory() {
  mock_pc_factory_created_ = true;
  return true;
}

bool MockMediaStreamDependencyFactory::PeerConnectionFactoryCreated() {
  return mock_pc_factory_created_;
}

scoped_refptr<webrtc::PeerConnectionInterface>
MockMediaStreamDependencyFactory::CreatePeerConnection(
    const webrtc::JsepInterface::IceServers& ice_servers,
    const webrtc::MediaConstraintsInterface* constraints,
    WebKit::WebFrame* frame,
    webrtc::PeerConnectionObserver* observer) {
  DCHECK(mock_pc_factory_created_);
  return new talk_base::RefCountedObject<MockPeerConnectionImpl>(this);
}

scoped_refptr<webrtc::VideoSourceInterface>
MockMediaStreamDependencyFactory::CreateVideoSource(
    int video_session_id,
    bool is_screencast,
    const webrtc::MediaConstraintsInterface* constraints) {
  last_video_source_ = new talk_base::RefCountedObject<MockVideoSource>();
  return last_video_source_;
}

scoped_refptr<webrtc::LocalMediaStreamInterface>
MockMediaStreamDependencyFactory::CreateLocalMediaStream(
    const std::string& label) {
  DCHECK(mock_pc_factory_created_);
  return new talk_base::RefCountedObject<MockLocalMediaStream>(label);
}

scoped_refptr<webrtc::VideoTrackInterface>
MockMediaStreamDependencyFactory::CreateLocalVideoTrack(
    const std::string& label,
    webrtc::VideoSourceInterface* source) {
  DCHECK(mock_pc_factory_created_);
  scoped_refptr<webrtc::VideoTrackInterface> track(
      new talk_base::RefCountedObject<MockLocalVideoTrack>(
          label, source));
  return track;
}

scoped_refptr<webrtc::LocalAudioTrackInterface>
MockMediaStreamDependencyFactory::CreateLocalAudioTrack(
    const std::string& label,
    webrtc::AudioDeviceModule* audio_device) {
  DCHECK(mock_pc_factory_created_);
  scoped_refptr<webrtc::LocalAudioTrackInterface> track(
      new talk_base::RefCountedObject<MockLocalAudioTrack>(label));
  return track;
}

SessionDescriptionInterface*
MockMediaStreamDependencyFactory::CreateSessionDescription(
    const std::string& sdp) {
  return CreateSessionDescription ("unknown", sdp);
}

SessionDescriptionInterface*
MockMediaStreamDependencyFactory::CreateSessionDescription(
      const std::string& type,
      const std::string& sdp) {
  return new MockSessionDescription(type, sdp);
}

webrtc::IceCandidateInterface*
MockMediaStreamDependencyFactory::CreateIceCandidate(
    const std::string& sdp_mid,
    int sdp_mline_index,
    const std::string& sdp) {
  return new MockIceCandidate(sdp_mid, sdp_mline_index, sdp);
}

void MockMediaStreamDependencyFactory::SetAudioDeviceSessionId(int session_id) {
}

}  // namespace content
