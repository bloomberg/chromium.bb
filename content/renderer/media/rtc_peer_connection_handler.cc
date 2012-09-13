// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/rtc_peer_connection_handler.h"

#include <string>
#include <utility>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/utf_string_conversions.h"
#include "content/renderer/media/media_stream_dependency_factory.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebMediaConstraints.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebRTCConfiguration.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebRTCICECandidate.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebRTCPeerConnectionHandlerClient.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebRTCSessionDescription.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebRTCSessionDescriptionRequest.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebRTCVoidRequest.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebURL.h"

// Converter functions from  libjingle types to WebKit types.

static WebKit::WebRTCPeerConnectionHandlerClient::ICEState
GetWebKitIceState(webrtc::PeerConnectionInterface::IceState ice_state) {
  switch (ice_state) {
    case webrtc::PeerConnectionInterface::kIceNew:
      return WebKit::WebRTCPeerConnectionHandlerClient::ICEStateNew;
    case webrtc::PeerConnectionInterface::kIceGathering:
      return WebKit::WebRTCPeerConnectionHandlerClient::ICEStateGathering;
    case webrtc::PeerConnectionInterface::kIceWaiting:
      return WebKit::WebRTCPeerConnectionHandlerClient::ICEStateWaiting;
    case webrtc::PeerConnectionInterface::kIceChecking:
      return WebKit::WebRTCPeerConnectionHandlerClient::ICEStateChecking;
    case webrtc::PeerConnectionInterface::kIceConnected:
      return WebKit::WebRTCPeerConnectionHandlerClient::ICEStateConnected;
    case webrtc::PeerConnectionInterface::kIceCompleted:
      return WebKit::WebRTCPeerConnectionHandlerClient::ICEStateCompleted;
    case webrtc::PeerConnectionInterface::kIceFailed:
      return WebKit::WebRTCPeerConnectionHandlerClient::ICEStateFailed;
    case webrtc::PeerConnectionInterface::kIceClosed:
      return WebKit::WebRTCPeerConnectionHandlerClient::ICEStateClosed;
    default:
      NOTREACHED();
      return WebKit::WebRTCPeerConnectionHandlerClient::ICEStateClosed;
  }
}

static WebKit::WebRTCPeerConnectionHandlerClient::ReadyState
GetWebKitReadyState(webrtc::PeerConnectionInterface::ReadyState ready_state) {
  switch (ready_state) {
    case webrtc::PeerConnectionInterface::kNew:
      return WebKit::WebRTCPeerConnectionHandlerClient::ReadyStateNew;
    case webrtc::PeerConnectionInterface::kOpening:
      return WebKit::WebRTCPeerConnectionHandlerClient::ReadyStateOpening;
    case webrtc::PeerConnectionInterface::kActive:
      return WebKit::WebRTCPeerConnectionHandlerClient::ReadyStateActive;
    case webrtc::PeerConnectionInterface::kClosing:
      return WebKit::WebRTCPeerConnectionHandlerClient::ReadyStateClosing;
    case webrtc::PeerConnectionInterface::kClosed:
      return WebKit::WebRTCPeerConnectionHandlerClient::ReadyStateClosed;
    default:
      NOTREACHED();
      return WebKit::WebRTCPeerConnectionHandlerClient::ReadyStateClosed;
  }
}

static WebKit::WebRTCSessionDescription
CreateWebKitSessionDescription(
    const webrtc::SessionDescriptionInterface* native_desc) {
  WebKit::WebRTCSessionDescription description;
  if (!native_desc) {
    LOG(ERROR) << "Native session description is null.";
    return description;
  }

  std::string sdp;
  if (!native_desc->ToString(&sdp)) {
    LOG(ERROR) << "Failed to get SDP string of native session description.";
    return description;
  }

  description.initialize(UTF8ToUTF16(native_desc->type()), UTF8ToUTF16(sdp));
  return description;
}

// Converter functions from WebKit types to libjingle types.

static void GetNativeIceServers(
    const WebKit::WebRTCConfiguration& server_configuration,
    webrtc::JsepInterface::IceServers* servers) {
  if (server_configuration.isNull() || !servers)
    return;
  for (size_t i = 0; i < server_configuration.numberOfServers(); ++i) {
    webrtc::JsepInterface::IceServer server;
    const WebKit::WebRTCICEServer& webkit_server =
        server_configuration.server(i);
    server.password = UTF16ToUTF8(webkit_server.credential());
    server.uri = webkit_server.uri().spec();
    servers->push_back(server);
  }
}

// Class mapping responses from calls to libjingle CreateOffer/Answer and
// the WebKit::WebRTCSessionDescriptionRequest.
class CreateSessionDescriptionRequest
    : public webrtc::CreateSessionDescriptionObserver {
 public:
  explicit CreateSessionDescriptionRequest(
      const WebKit::WebRTCSessionDescriptionRequest& request)
      : webkit_request_(request) {}

  virtual void OnSuccess(webrtc::SessionDescriptionInterface* desc) OVERRIDE {
    webkit_request_.requestSucceeded(CreateWebKitSessionDescription(desc));
  }
  virtual void OnFailure(const std::string& error) OVERRIDE {
    webkit_request_.requestFailed(UTF8ToUTF16(error));
  }

 protected:
  virtual ~CreateSessionDescriptionRequest() {}

 private:
  WebKit::WebRTCSessionDescriptionRequest webkit_request_;
};

// Class mapping responses from calls to libjingle
// SetLocalDescription/SetRemoteDescription and a WebKit::WebRTCVoidRequest.
class SetSessionDescriptionRequest
    : public webrtc::SetSessionDescriptionObserver {
 public:
  explicit SetSessionDescriptionRequest(
      const WebKit::WebRTCVoidRequest& request)
      : webkit_request_(request) {}

  virtual void OnSuccess() OVERRIDE {
    webkit_request_.requestSucceeded();
  }
  virtual void OnFailure(const std::string& error) OVERRIDE {
    webkit_request_.requestFailed(UTF8ToUTF16(error));
  }

 protected:
  virtual ~SetSessionDescriptionRequest() {}

 private:
  WebKit::WebRTCVoidRequest webkit_request_;
};

// TODO(perkj): Implement MediaConstraints when WebKit have done so.
class RTCMediaConstraints : public webrtc::MediaConstraintsInterface {
 public:
  explicit RTCMediaConstraints(
      const WebKit::WebMediaConstraints& constraints) {
  }
  ~RTCMediaConstraints() {}
};

RTCPeerConnectionHandler::RTCPeerConnectionHandler(
    WebKit::WebRTCPeerConnectionHandlerClient* client,
    MediaStreamDependencyFactory* dependency_factory)
    : PeerConnectionHandlerBase(dependency_factory),
      client_(client) {
}

RTCPeerConnectionHandler::~RTCPeerConnectionHandler() {
}

bool RTCPeerConnectionHandler::initialize(
    const WebKit::WebRTCConfiguration& server_configuration,
    const WebKit::WebMediaConstraints& options ) {
  webrtc::JsepInterface::IceServers servers;
  GetNativeIceServers(server_configuration, &servers);

  RTCMediaConstraints constraints(options);
  native_peer_connection_ =
      dependency_factory_->CreatePeerConnection(
          servers, &constraints, this);
  if (!native_peer_connection_) {
    LOG(ERROR) << "Failed to initialize native PeerConnection.";
    return false;
  }
  return true;
}

void RTCPeerConnectionHandler::createOffer(
    const WebKit::WebRTCSessionDescriptionRequest& request,
    const WebKit::WebMediaConstraints& options) {
  scoped_refptr<CreateSessionDescriptionRequest> description_request(
      new talk_base::RefCountedObject<CreateSessionDescriptionRequest>(
          request));
  RTCMediaConstraints constraints(options);
  native_peer_connection_->CreateOffer(description_request, &constraints);
}

void RTCPeerConnectionHandler::createAnswer(
    const WebKit::WebRTCSessionDescriptionRequest& request,
    const WebKit::WebMediaConstraints& options) {
  scoped_refptr<CreateSessionDescriptionRequest> description_request(
      new talk_base::RefCountedObject<CreateSessionDescriptionRequest>(
          request));
  RTCMediaConstraints constraints(options);
  native_peer_connection_->CreateAnswer(description_request, &constraints);
}

void RTCPeerConnectionHandler::setLocalDescription(
    const WebKit::WebRTCVoidRequest& request,
    const WebKit::WebRTCSessionDescription& description) {
  webrtc::SessionDescriptionInterface* native_desc =
      CreateNativeSessionDescription(description);
  if (!native_desc) {
    const char kReason[] = "Failed to parse SessionDescription.";
    LOG(ERROR) << kReason;
    WebKit::WebString reason(kReason);
    request.requestFailed(reason);
    return;
  }
  scoped_refptr<SetSessionDescriptionRequest> set_request(
      new talk_base::RefCountedObject<SetSessionDescriptionRequest>(request));
  native_peer_connection_->SetLocalDescription(set_request, native_desc);
}

void RTCPeerConnectionHandler::setRemoteDescription(
    const WebKit::WebRTCVoidRequest& request,
    const WebKit::WebRTCSessionDescription& description) {
  webrtc::SessionDescriptionInterface* native_desc =
      CreateNativeSessionDescription(description);
  if (!native_desc) {
    const char kReason[] = "Failed to parse SessionDescription.";
    LOG(ERROR) << kReason;
    WebKit::WebString reason(kReason);
    request.requestFailed(reason);
    return;
  }
  scoped_refptr<SetSessionDescriptionRequest> set_request(
      new talk_base::RefCountedObject<SetSessionDescriptionRequest>(request));
  native_peer_connection_->SetRemoteDescription(set_request, native_desc);
}

WebKit::WebRTCSessionDescription
RTCPeerConnectionHandler::localDescription() {
  const webrtc::SessionDescriptionInterface* native_desc =
      native_peer_connection_->local_description();
  WebKit::WebRTCSessionDescription description =
      CreateWebKitSessionDescription(native_desc);
  return description;
}

WebKit::WebRTCSessionDescription
RTCPeerConnectionHandler::remoteDescription() {
  const webrtc::SessionDescriptionInterface* native_desc =
      native_peer_connection_->remote_description();
  WebKit::WebRTCSessionDescription description =
      CreateWebKitSessionDescription(native_desc);
  return description;
}

bool RTCPeerConnectionHandler::updateICE(
    const WebKit::WebRTCConfiguration& server_configuration,
    const WebKit::WebMediaConstraints& options) {
  webrtc::JsepInterface::IceServers servers;
  GetNativeIceServers(server_configuration, &servers);
  RTCMediaConstraints constraints(options);
  return native_peer_connection_->UpdateIce(servers,
                                            &constraints);
}

bool RTCPeerConnectionHandler::addICECandidate(
    const WebKit::WebRTCICECandidate& candidate) {
  scoped_ptr<webrtc::IceCandidateInterface> native_candidate(
      dependency_factory_->CreateIceCandidate(
          UTF16ToUTF8(candidate.sdpMid()),
          candidate.sdpMLineIndex(),
          UTF16ToUTF8(candidate.candidate())));
  if (!native_candidate.get()) {
    LOG(ERROR) << "Could not create native ICE candidate.";
    return false;
  }

  bool return_value =
      native_peer_connection_->AddIceCandidate(native_candidate.get());
  LOG_IF(ERROR, !return_value) << "Error processing ICE candidate.";
  return return_value;
}

bool RTCPeerConnectionHandler::addStream(
    const WebKit::WebMediaStreamDescriptor& stream,
    const WebKit::WebMediaConstraints& options) {
  RTCMediaConstraints constraints(options);
  return AddStream(stream, &constraints);
}

void RTCPeerConnectionHandler::removeStream(
    const WebKit::WebMediaStreamDescriptor& stream) {
  RemoveStream(stream);
}

void RTCPeerConnectionHandler::stop() {
  DVLOG(1) << "RTCPeerConnectionHandler::stop";
  native_peer_connection_ = NULL;
}

void RTCPeerConnectionHandler::OnError() {
  // TODO(perkj): Implement.
  NOTIMPLEMENTED();
}

void RTCPeerConnectionHandler::OnStateChange(StateType state_changed) {
  switch (state_changed) {
    case kReadyState: {
      WebKit::WebRTCPeerConnectionHandlerClient::ReadyState ready_state =
          GetWebKitReadyState(native_peer_connection_->ready_state());
      client_->didChangeReadyState(ready_state);
      break;
    }
    case kIceState: {
      WebKit::WebRTCPeerConnectionHandlerClient::ICEState ice_state =
          GetWebKitIceState(native_peer_connection_->ice_state());
      client_->didChangeICEState(ice_state);
      break;
    }
    default:
      NOTREACHED();
      break;
  }
}

void RTCPeerConnectionHandler::OnAddStream(
    webrtc::MediaStreamInterface* stream) {
  DCHECK(stream);
  DCHECK(remote_streams_.find(stream) == remote_streams_.end());
  WebKit::WebMediaStreamDescriptor descriptor =
      CreateWebKitStreamDescriptor(stream);
  remote_streams_.insert(
      std::pair<webrtc::MediaStreamInterface*,
                WebKit::WebMediaStreamDescriptor>(stream, descriptor));
  client_->didAddRemoteStream(descriptor);
}

void RTCPeerConnectionHandler::OnRemoveStream(
    webrtc::MediaStreamInterface* stream) {
  DCHECK(stream);
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

void RTCPeerConnectionHandler::OnIceCandidate(
    const webrtc::IceCandidateInterface* candidate) {
  DCHECK(candidate);
  std::string sdp;
  if (!candidate->ToString(&sdp)) {
    NOTREACHED() << "OnIceCandidate: Could not get SDP string.";
    return;
  }
  WebKit::WebRTCICECandidate web_candidate;
  web_candidate.initialize(UTF8ToUTF16(sdp),
                           UTF8ToUTF16(candidate->sdp_mid()),
                           candidate->sdp_mline_index());
  client_->didGenerateICECandidate(web_candidate);
}

void RTCPeerConnectionHandler::OnIceComplete() {
  // Generates a NULL ice candidate object.
  WebKit::WebRTCICECandidate web_candidate;
  client_->didGenerateICECandidate(web_candidate);
}

void RTCPeerConnectionHandler::OnRenegotiationNeeded() {
  client_->negotiationNeeded();
}

webrtc::SessionDescriptionInterface*
RTCPeerConnectionHandler::CreateNativeSessionDescription(
    const WebKit::WebRTCSessionDescription& description) {
  std::string sdp = UTF16ToUTF8(description.sdp());
  std::string type = UTF16ToUTF8(description.type());
  webrtc::SessionDescriptionInterface* native_desc =
      dependency_factory_->CreateSessionDescription(type, sdp);

  LOG_IF(ERROR, !native_desc) << "Failed to create native session description."
                              << " Type: " << type << " SDP: " << sdp;

  return native_desc;
}
