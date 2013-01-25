// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/mock_media_stream_dependency_factory.h"
#include "content/renderer/media/mock_peer_connection_impl.h"

#include <vector>

#include "base/logging.h"

using webrtc::CreateSessionDescriptionObserver;
using webrtc::IceCandidateInterface;
using webrtc::LocalMediaStreamInterface;
using webrtc::MediaConstraintsInterface;
using webrtc::MediaStreamInterface;
using webrtc::PeerConnectionInterface;
using webrtc::SessionDescriptionInterface;
using webrtc::SetSessionDescriptionObserver;

namespace content {

class MockStreamCollection : public webrtc::StreamCollectionInterface {
 public:
  virtual size_t count() OVERRIDE {
    return streams_.size();
  }
  virtual MediaStreamInterface* at(size_t index) OVERRIDE {
    return streams_[index];
  }
  virtual MediaStreamInterface* find(const std::string& label) OVERRIDE {
    for (size_t i = 0; i < streams_.size(); ++i) {
      if (streams_[i]->label() == label)
        return streams_[i];
    }
    return NULL;
  }
  virtual webrtc::MediaStreamTrackInterface* FindAudioTrack(
      const std::string& id) OVERRIDE {
    for (size_t i = 0; i < streams_.size(); ++i) {
      webrtc::MediaStreamTrackInterface* track =
          streams_.at(i)->audio_tracks()->Find(id);
      if (track)
        return track;
    }
    return NULL;
  }
  virtual webrtc::MediaStreamTrackInterface* FindVideoTrack(
      const std::string& id) OVERRIDE{
    for (size_t i = 0; i < streams_.size(); ++i) {
      webrtc::MediaStreamTrackInterface* track =
          streams_.at(i)->video_tracks()->Find(id);
      if (track)
        return track;
    }
    return NULL;
  }
  void AddStream(MediaStreamInterface* stream) {
    streams_.push_back(stream);
  }
  void RemoveStream(MediaStreamInterface* stream) {
    StreamVector::iterator it = streams_.begin();
    for (; it != streams_.end(); ++it) {
      if (it->get() == stream) {
        streams_.erase(it);
        break;
      }
    }
  }

 protected:
  virtual ~MockStreamCollection() {}

 private:
  typedef std::vector<talk_base::scoped_refptr<MediaStreamInterface> >
      StreamVector;
  StreamVector streams_;
};

class MockDataChannel : public webrtc::DataChannelInterface {
 public:
  MockDataChannel(const std::string& label,
                  const webrtc::DataChannelInit* config)
      : label_(label),
        reliable_(config->reliable),
        state_(webrtc::DataChannelInterface::kConnecting) {
  }

  virtual void RegisterObserver(
      webrtc::DataChannelObserver* observer) OVERRIDE {
  }

  virtual void UnregisterObserver() OVERRIDE {
  }

  virtual const std::string& label() const OVERRIDE {
    return label_;
  }

  virtual bool reliable() const OVERRIDE {
    return reliable_;
  }

  virtual DataState state() const OVERRIDE {
    return state_;
  }

  virtual uint64 buffered_amount() const OVERRIDE {
    NOTIMPLEMENTED();
    return 0;
  }

  virtual void Close() OVERRIDE {
    state_ = webrtc::DataChannelInterface::kClosing;
  }

  virtual bool Send(const webrtc::DataBuffer& buffer) OVERRIDE {
    return state_ == webrtc::DataChannelInterface::kOpen;
  }

 protected:
  ~MockDataChannel() {}

 private:
  std::string label_;
  bool reliable_;
  webrtc::DataChannelInterface::DataState state_;
};

const char MockPeerConnectionImpl::kDummyOffer[] = "dummy offer";
const char MockPeerConnectionImpl::kDummyAnswer[] = "dummy answer";

MockPeerConnectionImpl::MockPeerConnectionImpl(
    MockMediaStreamDependencyFactory* factory)
    : dependency_factory_(factory),
      local_streams_(new talk_base::RefCountedObject<MockStreamCollection>),
      remote_streams_(new talk_base::RefCountedObject<MockStreamCollection>),
      hint_audio_(false),
      hint_video_(false),
      sdp_mline_index_(-1),
      signaling_state_(kStable),
      ice_state_(kIceNew) {
}

MockPeerConnectionImpl::~MockPeerConnectionImpl() {}

talk_base::scoped_refptr<webrtc::StreamCollectionInterface>
MockPeerConnectionImpl::local_streams() {
  return local_streams_;
}

talk_base::scoped_refptr<webrtc::StreamCollectionInterface>
MockPeerConnectionImpl::remote_streams() {
  return remote_streams_;
}

bool MockPeerConnectionImpl::AddStream(
    MediaStreamInterface* local_stream,
    const MediaConstraintsInterface* constraints) {
  DCHECK(stream_label_.empty());
  stream_label_ = local_stream->label();
  local_streams_->AddStream(local_stream);
  return true;
}

void MockPeerConnectionImpl::RemoveStream(
    MediaStreamInterface* local_stream) {
  DCHECK_EQ(stream_label_, local_stream->label());
  stream_label_.clear();
  local_streams_->RemoveStream(local_stream);
}


webrtc::DtmfSender* MockPeerConnectionImpl::CreateDtmfSender(
    webrtc::AudioTrackInterface* track,
    webrtc::DtmfSenderObserverInterface* observer) {
  NOTIMPLEMENTED();
  return NULL;
}

talk_base::scoped_refptr<webrtc::DataChannelInterface>
MockPeerConnectionImpl::CreateDataChannel(const std::string& label,
                      const webrtc::DataChannelInit* config) {
  return new talk_base::RefCountedObject<MockDataChannel>(label, config);
}

bool MockPeerConnectionImpl::GetStats(
    webrtc::StatsObserver* observer,
    webrtc::MediaStreamTrackInterface* track) {
  std::vector<webrtc::StatsReport> reports;
  webrtc::StatsReport report;
  report.id = "1234";
  report.type = "ssrc";
  report.local.timestamp = 42;
  webrtc::StatsElement::Value value;
  value.name = "trackname";
  value.value = "trackvalue";
  report.local.values.push_back(value);
  reports.push_back(report);
  // If selector is given, we pass back one report.
  // If selector is not given, we pass back two.
  if (!track) {
    report.id = "nontrack";
    report.type = "generic";
    report.local.timestamp = 44;
    value.name = "somename";
    value.value = "somevalue";
    report.local.values.push_back(value);
    reports.push_back(report);
  }
  // Note that the callback is synchronous, not asynchronous; it will
  // happen before the request call completes.
  observer->OnComplete(reports);
  return true;
}

MockPeerConnectionImpl::ReadyState MockPeerConnectionImpl::ready_state() {
  return signaling_state_;
}

MockPeerConnectionImpl::ReadyState MockPeerConnectionImpl::signaling_state() {
  return signaling_state_;
}

const webrtc::SessionDescriptionInterface*
MockPeerConnectionImpl::local_description() const {
  return local_desc_.get();
}

const webrtc::SessionDescriptionInterface*
MockPeerConnectionImpl::remote_description() const {
  return remote_desc_.get();
}

void MockPeerConnectionImpl::AddRemoteStream(MediaStreamInterface* stream) {
  remote_streams_->AddStream(stream);
}

void MockPeerConnectionImpl::CreateOffer(
    CreateSessionDescriptionObserver* observer,
    const MediaConstraintsInterface* constraints) {
  DCHECK(observer);
  created_sessiondescription_.reset(
      dependency_factory_->CreateSessionDescription("unknown", kDummyOffer));
}

void MockPeerConnectionImpl::CreateAnswer(
    CreateSessionDescriptionObserver* observer,
    const MediaConstraintsInterface* constraints) {
  DCHECK(observer);
  created_sessiondescription_.reset(
      dependency_factory_->CreateSessionDescription("unknown", kDummyAnswer));
}

void MockPeerConnectionImpl::SetLocalDescription(
    SetSessionDescriptionObserver* observer,
    SessionDescriptionInterface* desc) {
  desc->ToString(&description_sdp_);
  local_desc_.reset(desc);
}

void MockPeerConnectionImpl::SetRemoteDescription(
    SetSessionDescriptionObserver* observer,
    SessionDescriptionInterface* desc) {
  desc->ToString(&description_sdp_);
  remote_desc_.reset(desc);
}

bool MockPeerConnectionImpl::UpdateIce(
    const IceServers& configuration,
    const MediaConstraintsInterface* constraints) {
  return true;
}

bool MockPeerConnectionImpl::AddIceCandidate(
    const IceCandidateInterface* candidate) {
  sdp_mid_ = candidate->sdp_mid();
  sdp_mline_index_ = candidate->sdp_mline_index();
  return candidate->ToString(&ice_sdp_);
}

PeerConnectionInterface::IceState MockPeerConnectionImpl::ice_state() {
  return ice_state_;
}

}  // namespace content
