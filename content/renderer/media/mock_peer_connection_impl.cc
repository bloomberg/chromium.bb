// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/mock_media_stream_dependency_factory.h"
#include "content/renderer/media/mock_peer_connection_impl.h"

#include <vector>

#include "base/logging.h"

using webrtc::AudioTrackInterface;
using webrtc::CreateSessionDescriptionObserver;
using webrtc::DtmfSenderInterface;
using webrtc::DtmfSenderObserverInterface;
using webrtc::IceCandidateInterface;
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
          streams_.at(i)->FindAudioTrack(id);
      if (track)
        return track;
    }
    return NULL;
  }
  virtual webrtc::MediaStreamTrackInterface* FindVideoTrack(
      const std::string& id) OVERRIDE {
    for (size_t i = 0; i < streams_.size(); ++i) {
      webrtc::MediaStreamTrackInterface* track =
          streams_.at(i)->FindVideoTrack(id);
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

  virtual std::string label() const OVERRIDE {
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
  virtual ~MockDataChannel() {}

 private:
  std::string label_;
  bool reliable_;
  webrtc::DataChannelInterface::DataState state_;
};

class MockDtmfSender : public DtmfSenderInterface {
 public:
  explicit MockDtmfSender(AudioTrackInterface* track)
      : track_(track),
        observer_(NULL),
        duration_(0),
        inter_tone_gap_(0) {}
  virtual void RegisterObserver(
      DtmfSenderObserverInterface* observer) OVERRIDE {
    observer_ = observer;
  }
  virtual void UnregisterObserver() OVERRIDE {
    observer_ = NULL;
  }
  virtual bool CanInsertDtmf() OVERRIDE {
    return true;
  }
  virtual bool InsertDtmf(const std::string& tones, int duration,
                          int inter_tone_gap) OVERRIDE {
    tones_ = tones;
    duration_ = duration;
    inter_tone_gap_ = inter_tone_gap;
    return true;
  }
  virtual const AudioTrackInterface* track() const OVERRIDE {
    return track_.get();
  }
  virtual std::string tones() const OVERRIDE {
    return tones_;
  }
  virtual int duration() const OVERRIDE { return duration_; }
  virtual int inter_tone_gap() const OVERRIDE { return inter_tone_gap_; }

 protected:
  virtual ~MockDtmfSender() {}

 private:
  talk_base::scoped_refptr<AudioTrackInterface> track_;
  DtmfSenderObserverInterface* observer_;
  std::string tones_;
  int duration_;
  int inter_tone_gap_;
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
      getstats_result_(true),
      sdp_mline_index_(-1) {
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

talk_base::scoped_refptr<DtmfSenderInterface>
MockPeerConnectionImpl::CreateDtmfSender(AudioTrackInterface* track) {
  if (!track) {
    return NULL;
  }
  return new talk_base::RefCountedObject<MockDtmfSender>(track);
}

talk_base::scoped_refptr<webrtc::DataChannelInterface>
MockPeerConnectionImpl::CreateDataChannel(const std::string& label,
                      const webrtc::DataChannelInit* config) {
  return new talk_base::RefCountedObject<MockDataChannel>(label, config);
}

bool MockPeerConnectionImpl::GetStats(
    webrtc::StatsObserver* observer,
    webrtc::MediaStreamTrackInterface* track) {
  if (!getstats_result_)
    return false;

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
      dependency_factory_->CreateSessionDescription("unknown", kDummyOffer,
                                                    NULL));
}

void MockPeerConnectionImpl::CreateAnswer(
    CreateSessionDescriptionObserver* observer,
    const MediaConstraintsInterface* constraints) {
  DCHECK(observer);
  created_sessiondescription_.reset(
      dependency_factory_->CreateSessionDescription("unknown", kDummyAnswer,
                                                    NULL));
}

void MockPeerConnectionImpl::SetLocalDescriptionWorker(
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

}  // namespace content
