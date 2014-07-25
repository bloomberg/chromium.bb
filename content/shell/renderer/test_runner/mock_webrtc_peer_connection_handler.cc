// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/test_runner/mock_webrtc_peer_connection_handler.h"

#include "content/shell/renderer/test_runner/TestInterfaces.h"
#include "content/shell/renderer/test_runner/WebTestDelegate.h"
#include "content/shell/renderer/test_runner/mock_constraints.h"
#include "content/shell/renderer/test_runner/mock_webrtc_data_channel_handler.h"
#include "content/shell/renderer/test_runner/mock_webrtc_dtmf_sender_handler.h"
#include "third_party/WebKit/public/platform/WebMediaConstraints.h"
#include "third_party/WebKit/public/platform/WebMediaStream.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"
#include "third_party/WebKit/public/platform/WebRTCDataChannelInit.h"
#include "third_party/WebKit/public/platform/WebRTCPeerConnectionHandlerClient.h"
#include "third_party/WebKit/public/platform/WebRTCStatsResponse.h"
#include "third_party/WebKit/public/platform/WebRTCVoidRequest.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebVector.h"

using namespace blink;

namespace content {

class RTCSessionDescriptionRequestSuccededTask
    : public WebMethodTask<MockWebRTCPeerConnectionHandler> {
 public:
  RTCSessionDescriptionRequestSuccededTask(
      MockWebRTCPeerConnectionHandler* object,
      const WebRTCSessionDescriptionRequest& request,
      const WebRTCSessionDescription& result)
      : WebMethodTask<MockWebRTCPeerConnectionHandler>(object),
        request_(request),
        result_(result) {}

  virtual void runIfValid() OVERRIDE { request_.requestSucceeded(result_); }

 private:
  WebRTCSessionDescriptionRequest request_;
  WebRTCSessionDescription result_;
};

class RTCSessionDescriptionRequestFailedTask
    : public WebMethodTask<MockWebRTCPeerConnectionHandler> {
 public:
  RTCSessionDescriptionRequestFailedTask(
      MockWebRTCPeerConnectionHandler* object,
      const WebRTCSessionDescriptionRequest& request)
      : WebMethodTask<MockWebRTCPeerConnectionHandler>(object),
        request_(request) {}

  virtual void runIfValid() OVERRIDE { request_.requestFailed("TEST_ERROR"); }

 private:
  WebRTCSessionDescriptionRequest request_;
};

class RTCStatsRequestSucceededTask
    : public WebMethodTask<MockWebRTCPeerConnectionHandler> {
 public:
  RTCStatsRequestSucceededTask(MockWebRTCPeerConnectionHandler* object,
                               const blink::WebRTCStatsRequest& request,
                               const blink::WebRTCStatsResponse& response)
      : WebMethodTask<MockWebRTCPeerConnectionHandler>(object),
        request_(request),
        response_(response) {}

  virtual void runIfValid() OVERRIDE { request_.requestSucceeded(response_); }

 private:
  blink::WebRTCStatsRequest request_;
  blink::WebRTCStatsResponse response_;
};

class RTCVoidRequestTask
    : public WebMethodTask<MockWebRTCPeerConnectionHandler> {
 public:
  RTCVoidRequestTask(MockWebRTCPeerConnectionHandler* object,
                     const WebRTCVoidRequest& request,
                     bool succeeded)
      : WebMethodTask<MockWebRTCPeerConnectionHandler>(object),
        request_(request),
        succeeded_(succeeded) {}

  virtual void runIfValid() OVERRIDE {
    if (succeeded_)
      request_.requestSucceeded();
    else
      request_.requestFailed("TEST_ERROR");
  }

 private:
  WebRTCVoidRequest request_;
  bool succeeded_;
};

class RTCPeerConnectionStateTask
    : public WebMethodTask<MockWebRTCPeerConnectionHandler> {
 public:
  RTCPeerConnectionStateTask(
      MockWebRTCPeerConnectionHandler* object,
      WebRTCPeerConnectionHandlerClient* client,
      WebRTCPeerConnectionHandlerClient::ICEConnectionState connection_state,
      WebRTCPeerConnectionHandlerClient::ICEGatheringState gathering_state)
      : WebMethodTask<MockWebRTCPeerConnectionHandler>(object),
        client_(client),
        connection_state_(connection_state),
        gathering_state_(gathering_state) {}

  virtual void runIfValid() OVERRIDE {
    client_->didChangeICEGatheringState(gathering_state_);
    client_->didChangeICEConnectionState(connection_state_);
  }

 private:
  WebRTCPeerConnectionHandlerClient* client_;
  WebRTCPeerConnectionHandlerClient::ICEConnectionState connection_state_;
  WebRTCPeerConnectionHandlerClient::ICEGatheringState gathering_state_;
};

class RemoteDataChannelTask
    : public WebMethodTask<MockWebRTCPeerConnectionHandler> {
 public:
  RemoteDataChannelTask(MockWebRTCPeerConnectionHandler* object,
                        WebRTCPeerConnectionHandlerClient* client,
                        WebTestDelegate* delegate)
      : WebMethodTask<MockWebRTCPeerConnectionHandler>(object),
        client_(client),
        delegate_(delegate) {}

  virtual void runIfValid() OVERRIDE {
    WebRTCDataChannelInit init;
    WebRTCDataChannelHandler* remote_data_channel =
        new MockWebRTCDataChannelHandler(
            "MockRemoteDataChannel", init, delegate_);
    client_->didAddRemoteDataChannel(remote_data_channel);
  }

 private:
  WebRTCPeerConnectionHandlerClient* client_;
  WebTestDelegate* delegate_;
};

/////////////////////

MockWebRTCPeerConnectionHandler::MockWebRTCPeerConnectionHandler() {
}

MockWebRTCPeerConnectionHandler::MockWebRTCPeerConnectionHandler(
    WebRTCPeerConnectionHandlerClient* client,
    TestInterfaces* interfaces)
    : client_(client),
      stopped_(false),
      stream_count_(0),
      interfaces_(interfaces) {
}

bool MockWebRTCPeerConnectionHandler::initialize(
    const WebRTCConfiguration& configuration,
    const WebMediaConstraints& constraints) {
  if (MockConstraints::VerifyConstraints(constraints)) {
    interfaces_->delegate()->postTask(new RTCPeerConnectionStateTask(
        this,
        client_,
        WebRTCPeerConnectionHandlerClient::ICEConnectionStateCompleted,
        WebRTCPeerConnectionHandlerClient::ICEGatheringStateComplete));
    return true;
  }

  return false;
}

void MockWebRTCPeerConnectionHandler::createOffer(
    const WebRTCSessionDescriptionRequest& request,
    const WebMediaConstraints& constraints) {
  WebString should_succeed;
  if (constraints.getMandatoryConstraintValue("succeed", should_succeed) &&
      should_succeed == "true") {
    WebRTCSessionDescription session_description;
    session_description.initialize("offer", "local");
    interfaces_->delegate()->postTask(
        new RTCSessionDescriptionRequestSuccededTask(
            this, request, session_description));
  } else
    interfaces_->delegate()->postTask(
        new RTCSessionDescriptionRequestFailedTask(this, request));
}

void MockWebRTCPeerConnectionHandler::createOffer(
    const WebRTCSessionDescriptionRequest& request,
    const blink::WebRTCOfferOptions& options) {
  interfaces_->delegate()->postTask(
      new RTCSessionDescriptionRequestFailedTask(this, request));
}

void MockWebRTCPeerConnectionHandler::createAnswer(
    const WebRTCSessionDescriptionRequest& request,
    const WebMediaConstraints& constraints) {
  if (!remote_description_.isNull()) {
    WebRTCSessionDescription session_description;
    session_description.initialize("answer", "local");
    interfaces_->delegate()->postTask(
        new RTCSessionDescriptionRequestSuccededTask(
            this, request, session_description));
  } else
    interfaces_->delegate()->postTask(
        new RTCSessionDescriptionRequestFailedTask(this, request));
}

void MockWebRTCPeerConnectionHandler::setLocalDescription(
    const WebRTCVoidRequest& request,
    const WebRTCSessionDescription& local_description) {
  if (!local_description.isNull() && local_description.sdp() == "local") {
    local_description_ = local_description;
    interfaces_->delegate()->postTask(
        new RTCVoidRequestTask(this, request, true));
  } else
    interfaces_->delegate()->postTask(
        new RTCVoidRequestTask(this, request, false));
}

void MockWebRTCPeerConnectionHandler::setRemoteDescription(
    const WebRTCVoidRequest& request,
    const WebRTCSessionDescription& remote_description) {
  if (!remote_description.isNull() && remote_description.sdp() == "remote") {
    remote_description_ = remote_description;
    interfaces_->delegate()->postTask(
        new RTCVoidRequestTask(this, request, true));
  } else
    interfaces_->delegate()->postTask(
        new RTCVoidRequestTask(this, request, false));
}

WebRTCSessionDescription MockWebRTCPeerConnectionHandler::localDescription() {
  return local_description_;
}

WebRTCSessionDescription MockWebRTCPeerConnectionHandler::remoteDescription() {
  return remote_description_;
}

bool MockWebRTCPeerConnectionHandler::updateICE(
    const WebRTCConfiguration& configuration,
    const WebMediaConstraints& constraints) {
  return true;
}

bool MockWebRTCPeerConnectionHandler::addICECandidate(
    const WebRTCICECandidate& ice_candidate) {
  client_->didGenerateICECandidate(ice_candidate);
  return true;
}

bool MockWebRTCPeerConnectionHandler::addICECandidate(
    const WebRTCVoidRequest& request,
    const WebRTCICECandidate& ice_candidate) {
  interfaces_->delegate()->postTask(
      new RTCVoidRequestTask(this, request, true));
  return true;
}

bool MockWebRTCPeerConnectionHandler::addStream(
    const WebMediaStream& stream,
    const WebMediaConstraints& constraints) {
  ++stream_count_;
  client_->negotiationNeeded();
  return true;
}

void MockWebRTCPeerConnectionHandler::removeStream(
    const WebMediaStream& stream) {
  --stream_count_;
  client_->negotiationNeeded();
}

void MockWebRTCPeerConnectionHandler::getStats(
    const WebRTCStatsRequest& request) {
  WebRTCStatsResponse response = request.createResponse();
  double current_date = interfaces_->delegate()->getCurrentTimeInMillisecond();
  if (request.hasSelector()) {
    // FIXME: There is no check that the fetched values are valid.
    size_t report_index =
        response.addReport("Mock video", "ssrc", current_date);
    response.addStatistic(report_index, "type", "video");
  } else {
    for (int i = 0; i < stream_count_; ++i) {
      size_t report_index =
          response.addReport("Mock audio", "ssrc", current_date);
      response.addStatistic(report_index, "type", "audio");
      report_index = response.addReport("Mock video", "ssrc", current_date);
      response.addStatistic(report_index, "type", "video");
    }
  }
  interfaces_->delegate()->postTask(
      new RTCStatsRequestSucceededTask(this, request, response));
}

WebRTCDataChannelHandler* MockWebRTCPeerConnectionHandler::createDataChannel(
    const WebString& label,
    const blink::WebRTCDataChannelInit& init) {
  interfaces_->delegate()->postTask(
      new RemoteDataChannelTask(this, client_, interfaces_->delegate()));

  return new MockWebRTCDataChannelHandler(label, init, interfaces_->delegate());
}

WebRTCDTMFSenderHandler* MockWebRTCPeerConnectionHandler::createDTMFSender(
    const WebMediaStreamTrack& track) {
  return new MockWebRTCDTMFSenderHandler(track, interfaces_->delegate());
}

void MockWebRTCPeerConnectionHandler::stop() {
  stopped_ = true;
  task_list_.revokeAll();
}

}  // namespace content
