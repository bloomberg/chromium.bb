// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

MockPeerConnectionImpl::MockPeerConnectionImpl()
    : stream_changes_committed_(false),
      local_streams_(new talk_base::RefCountedObject<MockStreamCollection>),
      remote_streams_(new talk_base::RefCountedObject<MockStreamCollection>) {
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

void MockPeerConnectionImpl::CommitStreamChanges() {
  stream_changes_committed_ = true;
}

void MockPeerConnectionImpl::Close() {
  signaling_message_.clear();
  stream_label_.clear();
  stream_changes_committed_ = false;
}

MockPeerConnectionImpl::ReadyState MockPeerConnectionImpl::ready_state() {
  NOTIMPLEMENTED();
  return kNew;
}

MockPeerConnectionImpl::SdpState MockPeerConnectionImpl::sdp_state() {
  NOTIMPLEMENTED();
  return kSdpNew;
}

bool MockPeerConnectionImpl::StartIce(IceOptions options) {
  NOTIMPLEMENTED();
  return false;
}

webrtc::SessionDescriptionInterface* MockPeerConnectionImpl::CreateOffer(
    const webrtc::MediaHints& hints) {
  NOTIMPLEMENTED();
  return NULL;
}

webrtc::SessionDescriptionInterface* MockPeerConnectionImpl::CreateAnswer(
    const webrtc::MediaHints& hints,
    const webrtc::SessionDescriptionInterface* offer) {
  NOTIMPLEMENTED();
  return NULL;
}

bool MockPeerConnectionImpl::SetLocalDescription(
    Action action,
    webrtc::SessionDescriptionInterface* desc) {
  NOTIMPLEMENTED();
  return false;
}

bool MockPeerConnectionImpl::SetRemoteDescription(
    Action action,
    webrtc::SessionDescriptionInterface* desc) {
  NOTIMPLEMENTED();
  return false;
}

bool MockPeerConnectionImpl::ProcessIceMessage(
    const webrtc::IceCandidateInterface* ice_candidate) {
  NOTIMPLEMENTED();
  return false;
}

const webrtc::SessionDescriptionInterface*
MockPeerConnectionImpl::local_description() const {
  NOTIMPLEMENTED();
  return NULL;
}

const webrtc::SessionDescriptionInterface*
MockPeerConnectionImpl::remote_description() const {
  NOTIMPLEMENTED();
  return NULL;
}

void MockPeerConnectionImpl::AddRemoteStream(MediaStreamInterface* stream) {
  remote_streams_->AddStream(stream);
}

}  // namespace webrtc
