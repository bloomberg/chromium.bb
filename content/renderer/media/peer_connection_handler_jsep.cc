// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/peer_connection_handler_jsep.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "content/renderer/media/media_stream_dependency_factory.h"
#include "content/renderer/media/media_stream_impl.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebICECandidateDescriptor.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebICEOptions.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebPeerConnection00HandlerClient.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebMediaHints.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebMediaStreamDescriptor.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebMediaStreamSource.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebSessionDescriptionDescriptor.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebVector.h"

PeerConnectionHandlerJsep::PeerConnectionHandlerJsep(
    WebKit::WebPeerConnection00HandlerClient* client,
    MediaStreamDependencyFactory* dependency_factory)
    : PeerConnectionHandlerBase(dependency_factory),
      client_(client) {
}

PeerConnectionHandlerJsep::~PeerConnectionHandlerJsep() {
}

void PeerConnectionHandlerJsep::initialize(
    const WebKit::WebString& server_configuration,
    const WebKit::WebString& username) {
  native_peer_connection_ = dependency_factory_->CreatePeerConnection(
      UTF16ToUTF8(server_configuration),
      this);
  CHECK(native_peer_connection_);
}

WebKit::WebSessionDescriptionDescriptor PeerConnectionHandlerJsep::createOffer(
    const WebKit::WebMediaHints& hints) {
  WebKit::WebSessionDescriptionDescriptor offer;

  webrtc::MediaHints native_hints(hints.audio(), hints.video());
  scoped_ptr<webrtc::SessionDescriptionInterface> native_offer(
      native_peer_connection_->CreateOffer(native_hints));
  if (!native_offer.get()) {
    LOG(ERROR) << "Failed to create native offer";
    return offer;
  }

  offer = CreateWebKitSessionDescription(native_offer.get());
  return offer;
}

WebKit::WebSessionDescriptionDescriptor PeerConnectionHandlerJsep::createAnswer(
    const WebKit::WebString& offer,
    const WebKit::WebMediaHints& hints) {
  WebKit::WebSessionDescriptionDescriptor answer;

  webrtc::MediaHints native_hints(hints.audio(), hints.video());
  scoped_ptr<webrtc::SessionDescriptionInterface> native_offer(
      dependency_factory_->CreateSessionDescription(UTF16ToUTF8(offer)));
  if (!native_offer.get()) {
    LOG(ERROR) << "Failed to create native offer";
    return answer;
  }

  scoped_ptr<webrtc::SessionDescriptionInterface> native_answer(
      native_peer_connection_->CreateAnswer(native_hints, native_offer.get()));
  if (!native_answer.get()) {
    LOG(ERROR) << "Failed to create native answer";
    return answer;
  }

  answer = CreateWebKitSessionDescription(native_answer.get());
  return answer;
}

bool PeerConnectionHandlerJsep::setLocalDescription(
    Action action,
    const WebKit::WebSessionDescriptionDescriptor& description) {
  webrtc::PeerConnectionInterface::Action native_action;
  if (!GetNativeAction(action, &native_action))
    return false;

  webrtc::SessionDescriptionInterface* native_desc =
      CreateNativeSessionDescription(description);
  if (!native_desc)
    return false;

  return native_peer_connection_->SetLocalDescription(native_action,
                                                      native_desc);
}

bool PeerConnectionHandlerJsep::setRemoteDescription(
    Action action,
    const WebKit::WebSessionDescriptionDescriptor& description) {
  webrtc::PeerConnectionInterface::Action native_action;
  if (!GetNativeAction(action, &native_action))
    return false;

  webrtc::SessionDescriptionInterface* native_desc =
      CreateNativeSessionDescription(description);
  if (!native_desc)
    return false;

  return native_peer_connection_->SetRemoteDescription(native_action,
                                                       native_desc);
}

WebKit::WebSessionDescriptionDescriptor
PeerConnectionHandlerJsep::localDescription() {
  const webrtc::SessionDescriptionInterface* native_desc =
      native_peer_connection_->local_description();
  WebKit::WebSessionDescriptionDescriptor description =
      CreateWebKitSessionDescription(native_desc);
  return description;
}

WebKit::WebSessionDescriptionDescriptor
PeerConnectionHandlerJsep::remoteDescription() {
  const webrtc::SessionDescriptionInterface* native_desc =
      native_peer_connection_->remote_description();
  WebKit::WebSessionDescriptionDescriptor description =
      CreateWebKitSessionDescription(native_desc);
  return description;
}

bool PeerConnectionHandlerJsep::startIce(const WebKit::WebICEOptions& options) {
  webrtc::PeerConnectionInterface::IceOptions native_options;
  switch (options.candidateTypeToUse()) {
    case WebKit::WebICEOptions::CandidateTypeAll:
      native_options = webrtc::PeerConnectionInterface::kUseAll;
      break;
    case WebKit::WebICEOptions::CandidateTypeNoRelay:
      native_options = webrtc::PeerConnectionInterface::kNoRelay;
      break;
    case WebKit::WebICEOptions::CandidateTypeOnlyRelay:
      native_options = webrtc::PeerConnectionInterface::kOnlyRelay;
      break;
    default:
      NOTREACHED();
      return false;
  }
  native_peer_connection_->StartIce(native_options);
  return true;
}

bool PeerConnectionHandlerJsep::processIceMessage(
    const WebKit::WebICECandidateDescriptor& candidate) {
  scoped_ptr<webrtc::IceCandidateInterface> native_candidate(
      dependency_factory_->CreateIceCandidate(
          UTF16ToUTF8(candidate.label()),
          UTF16ToUTF8(candidate.candidateLine())));
  if (!native_candidate.get()) {
    LOG(ERROR) << "Could not create native ICE candidate";
    return false;
  }

  bool return_value =
      native_peer_connection_->ProcessIceMessage(native_candidate.get());
  if (!return_value)
    LOG(ERROR) << "Error processing ICE message";
  return return_value;
}

void PeerConnectionHandlerJsep::addStream(
    const WebKit::WebMediaStreamDescriptor& stream) {
  AddStream(stream);
  native_peer_connection_->CommitStreamChanges();
}

void PeerConnectionHandlerJsep::removeStream(
    const WebKit::WebMediaStreamDescriptor& stream) {
  RemoveStream(stream);
  native_peer_connection_->CommitStreamChanges();
}

void PeerConnectionHandlerJsep::stop() {
  DVLOG(1) << "PeerConnectionHandlerJsep::stop";
  native_peer_connection_ = NULL;
}

void PeerConnectionHandlerJsep::OnError() {
  // TODO(grunell): Implement.
  NOTIMPLEMENTED();
}

void PeerConnectionHandlerJsep::OnMessage(const std::string& msg) {
  // TODO(grunell): Implement.
  NOTIMPLEMENTED();
}

void PeerConnectionHandlerJsep::OnSignalingMessage(const std::string& msg) {
  // Not used by JSEP PeerConnection.
  NOTREACHED();
}

void PeerConnectionHandlerJsep::OnStateChange(StateType state_changed) {
  switch (state_changed) {
    case kReadyState:
      WebKit::WebPeerConnection00HandlerClient::ReadyState ready_state;
      switch (native_peer_connection_->ready_state()) {
        case webrtc::PeerConnectionInterface::kNew:
          ready_state = WebKit::WebPeerConnection00HandlerClient::ReadyStateNew;
          break;
        case webrtc::PeerConnectionInterface::kNegotiating:
          ready_state =
              WebKit::WebPeerConnection00HandlerClient::ReadyStateNegotiating;
          break;
        case webrtc::PeerConnectionInterface::kActive:
          ready_state =
              WebKit::WebPeerConnection00HandlerClient::ReadyStateActive;
          break;
        case webrtc::PeerConnectionInterface::kClosing:
          // Not used by JSEP.
          NOTREACHED();
          return;
        case webrtc::PeerConnectionInterface::kClosed:
          ready_state =
              WebKit::WebPeerConnection00HandlerClient::ReadyStateClosed;
          break;
        default:
          NOTREACHED();
          return;
      }
      client_->didChangeReadyState(ready_state);
      break;
    case kIceState:
      // TODO(grunell): Implement when available in native PeerConnection.
      NOTIMPLEMENTED();
      break;
    case kSdpState:
      // Not used by JSEP.
      NOTREACHED();
      break;
    default:
      NOTREACHED();
      break;
  }
}

void PeerConnectionHandlerJsep::OnAddStream(
    webrtc::MediaStreamInterface* stream) {
  if (!stream)
    return;

  DCHECK(remote_streams_.find(stream) == remote_streams_.end());
  WebKit::WebMediaStreamDescriptor descriptor =
      CreateWebKitStreamDescriptor(stream);
  remote_streams_.insert(
      std::pair<webrtc::MediaStreamInterface*,
                WebKit::WebMediaStreamDescriptor>(stream, descriptor));
  client_->didAddRemoteStream(descriptor);
}

void PeerConnectionHandlerJsep::OnRemoveStream(
    webrtc::MediaStreamInterface* stream) {
  if (!stream)
    return;

  RemoteStreamMap::iterator it = remote_streams_.find(stream);
  if (it == remote_streams_.end()) {
    NOTREACHED() << "Stream not found";
    return;
  }
  WebKit::WebMediaStreamDescriptor descriptor = it->second;
  DCHECK(!descriptor.isNull());
  remote_streams_.erase(it);
  client_->didRemoveRemoteStream(descriptor);
}

void PeerConnectionHandlerJsep::OnIceCandidate(
    const webrtc::IceCandidateInterface* candidate) {
  WebKit::WebICECandidateDescriptor web_candidate;

  std::string label = candidate->label();
  std::string sdp;
  if (!candidate->ToString(&sdp)) {
    LOG(ERROR) << "Could not get SDP string";
    return;
  }

  web_candidate.initialize(UTF8ToUTF16(label), UTF8ToUTF16(sdp));

  // moreToFollow parameter isn't supported in native PeerConnection, so we
  // always use true here, and then false in OnIceComplete().
  client_->didGenerateICECandidate(web_candidate, true);
}

void PeerConnectionHandlerJsep::OnIceComplete() {
  // moreToFollow parameter isn't supported in native PeerConnection, so we
  // send an empty WebIseCandidate with moreToFollow=false.
  WebKit::WebICECandidateDescriptor web_candidate;
  client_->didGenerateICECandidate(web_candidate, false);
}

webrtc::SessionDescriptionInterface*
PeerConnectionHandlerJsep::CreateNativeSessionDescription(
    const WebKit::WebSessionDescriptionDescriptor& description) {
  std::string initial_sdp = UTF16ToUTF8(description.initialSDP());
  webrtc::SessionDescriptionInterface* native_desc =
      dependency_factory_->CreateSessionDescription(initial_sdp);
  if (!native_desc) {
    LOG(ERROR) << "Failed to create native session description";
    return NULL;
  }

  for (size_t i = 0; i < description.numberOfAddedCandidates(); ++i) {
    WebKit::WebICECandidateDescriptor candidate = description.candidate(i);
    scoped_ptr<webrtc::IceCandidateInterface> native_candidate(
        dependency_factory_->CreateIceCandidate(
            UTF16ToUTF8(candidate.label()),
            UTF16ToUTF8(candidate.candidateLine())));
    if (!native_desc->AddCandidate(native_candidate.get()))
      LOG(ERROR) << "Failed to add candidate to native session description";
  }

  return native_desc;
}

WebKit::WebSessionDescriptionDescriptor
PeerConnectionHandlerJsep::CreateWebKitSessionDescription(
    const webrtc::SessionDescriptionInterface* native_desc) {
  WebKit::WebSessionDescriptionDescriptor description;
  if (!native_desc) {
    VLOG(1) << "Native session description is null";
    return description;
  }

  std::string sdp;
  if (!native_desc->ToString(&sdp)) {
    LOG(ERROR) << "Failed to get SDP string of native session description";
    return description;
  }

  description.initialize(UTF8ToUTF16(sdp));
  return description;
}

bool PeerConnectionHandlerJsep::GetNativeAction(
    const Action action,
    webrtc::PeerConnectionInterface::Action* native_action) {
  switch (action) {
    case ActionSDPOffer:
      *native_action = webrtc::PeerConnectionInterface::kOffer;
      break;
    case ActionSDPPRanswer:
      VLOG(1) << "Action PRANSWER not supported yet";
      return false;
    case ActionSDPAnswer:
      *native_action = webrtc::PeerConnectionInterface::kAnswer;
      break;
    default:
      NOTREACHED();
      return false;
  }
  return true;
}
