// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/mock_media_stream_dependency_factory.h"
#include "content/renderer/media/mock_peer_connection_impl.h"

#include <vector>

#include "base/logging.h"

namespace webrtc {

class MockStreamCollection : public StreamCollectionInterface {
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
  void AddStream(MediaStreamInterface* stream) {
    streams_.push_back(stream);
  }

 protected:
  virtual ~MockStreamCollection() {}

 private:
  std::vector<talk_base::scoped_refptr<MediaStreamInterface> > streams_;
};

const char MockPeerConnectionImpl::kDummyOffer[] = "dummy offer";

MockPeerConnectionImpl::MockPeerConnectionImpl(
    MockMediaStreamDependencyFactory* factory)
    : dependency_factory_(factory),
      stream_changes_committed_(false),
      local_streams_(new talk_base::RefCountedObject<MockStreamCollection>),
      remote_streams_(new talk_base::RefCountedObject<MockStreamCollection>),
      hint_audio_(false),
      hint_video_(false),
      action_(kAnswer),
      ice_options_(kOnlyRelay),
      ready_state_(kNew) {
}

MockPeerConnectionImpl::~MockPeerConnectionImpl() {}

void MockPeerConnectionImpl::ProcessSignalingMessage(const std::string& msg) {
  signaling_message_ = msg;
}

bool MockPeerConnectionImpl::Send(const std::string& msg) {
  NOTIMPLEMENTED();
  return false;
}

talk_base::scoped_refptr<StreamCollectionInterface>
MockPeerConnectionImpl::local_streams() {
  return local_streams_;
}

talk_base::scoped_refptr<StreamCollectionInterface>
MockPeerConnectionImpl::remote_streams() {
  return remote_streams_;
}

void MockPeerConnectionImpl::AddStream(LocalMediaStreamInterface* stream) {
  DCHECK(stream_label_.empty());
  stream_label_ = stream->label();
  local_streams_->AddStream(stream);
}

void MockPeerConnectionImpl::RemoveStream(LocalMediaStreamInterface* stream) {
  DCHECK_EQ(stream_label_, stream->label());
  stream_label_.clear();
}

bool MockPeerConnectionImpl::RemoveStream(const std::string& label) {
  if (stream_label_ != label)
    return false;
  stream_label_.clear();
  return true;
}

void MockPeerConnectionImpl::CommitStreamChanges() {
  stream_changes_committed_ = true;
}

void MockPeerConnectionImpl::Close() {
  signaling_message_.clear();
  stream_label_.clear();
  stream_changes_committed_ = false;
}

MockPeerConnectionImpl::ReadyState MockPeerConnectionImpl::ready_state() {
  return ready_state_;
}

MockPeerConnectionImpl::SdpState MockPeerConnectionImpl::sdp_state() {
  NOTIMPLEMENTED();
  return kSdpNew;
}

bool MockPeerConnectionImpl::StartIce(IceOptions options) {
  ice_options_ = options;
  return true;
}

webrtc::SessionDescriptionInterface* MockPeerConnectionImpl::CreateOffer(
    const webrtc::MediaHints& hints) {
  hint_audio_ = hints.has_audio();
  hint_video_ = hints.has_video();
  return dependency_factory_->CreateSessionDescription(kDummyOffer);
}

webrtc::SessionDescriptionInterface* MockPeerConnectionImpl::CreateAnswer(
    const webrtc::MediaHints& hints,
    const webrtc::SessionDescriptionInterface* offer) {
  hint_audio_ = hints.has_audio();
  hint_video_ = hints.has_video();
  offer->ToString(&description_sdp_);
  return dependency_factory_->CreateSessionDescription(description_sdp_);
}

bool MockPeerConnectionImpl::SetLocalDescription(
    Action action,
    webrtc::SessionDescriptionInterface* desc) {
  action_ = action;
  local_desc_.reset(desc);
  return desc->ToString(&description_sdp_);
}

bool MockPeerConnectionImpl::SetRemoteDescription(
    Action action,
    webrtc::SessionDescriptionInterface* desc) {
  action_ = action;
  remote_desc_.reset(desc);
  return desc->ToString(&description_sdp_);
}

bool MockPeerConnectionImpl::ProcessIceMessage(
    const webrtc::IceCandidateInterface* ice_candidate) {
  ice_label_ = ice_candidate->label();
  return ice_candidate->ToString(&ice_sdp_);
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

}  // namespace webrtc
