// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_TEST_RUNNER_MOCKWEBRTCPEERCONNECTIONHANDLER_H_
#define CONTENT_SHELL_RENDERER_TEST_RUNNER_MOCKWEBRTCPEERCONNECTIONHANDLER_H_

#include "base/basictypes.h"
#include "content/shell/renderer/test_runner/web_task.h"
#include "third_party/WebKit/public/platform/WebRTCPeerConnectionHandler.h"
#include "third_party/WebKit/public/platform/WebRTCSessionDescription.h"
#include "third_party/WebKit/public/platform/WebRTCSessionDescriptionRequest.h"
#include "third_party/WebKit/public/platform/WebRTCStatsRequest.h"

namespace blink {
class WebRTCPeerConnectionHandlerClient;
};

namespace content {

class TestInterfaces;

class MockWebRTCPeerConnectionHandler
    : public blink::WebRTCPeerConnectionHandler {
 public:
  MockWebRTCPeerConnectionHandler(
      blink::WebRTCPeerConnectionHandlerClient* client,
      TestInterfaces* interfaces);

  // WebRTCPeerConnectionHandler related methods
  virtual bool initialize(
      const blink::WebRTCConfiguration& configuration,
      const blink::WebMediaConstraints& constraints) OVERRIDE;

  virtual void createOffer(
      const blink::WebRTCSessionDescriptionRequest& request,
      const blink::WebMediaConstraints& constraints) OVERRIDE;
  virtual void createOffer(
      const blink::WebRTCSessionDescriptionRequest& request,
      const blink::WebRTCOfferOptions& options) OVERRIDE;
  virtual void createAnswer(
      const blink::WebRTCSessionDescriptionRequest& request,
      const blink::WebMediaConstraints& constraints) OVERRIDE;
  virtual void setLocalDescription(
      const blink::WebRTCVoidRequest& request,
      const blink::WebRTCSessionDescription& local_description) OVERRIDE;
  virtual void setRemoteDescription(
      const blink::WebRTCVoidRequest& request,
      const blink::WebRTCSessionDescription& remote_description) OVERRIDE;
  virtual blink::WebRTCSessionDescription localDescription() OVERRIDE;
  virtual blink::WebRTCSessionDescription remoteDescription() OVERRIDE;
  virtual bool updateICE(
      const blink::WebRTCConfiguration& configuration,
      const blink::WebMediaConstraints& constraints) OVERRIDE;
  virtual bool addICECandidate(
      const blink::WebRTCICECandidate& ice_candidate) OVERRIDE;
  virtual bool addICECandidate(
      const blink::WebRTCVoidRequest& request,
      const blink::WebRTCICECandidate& ice_candidate) OVERRIDE;
  virtual bool addStream(
      const blink::WebMediaStream& stream,
      const blink::WebMediaConstraints& constraints) OVERRIDE;
  virtual void removeStream(const blink::WebMediaStream& stream) OVERRIDE;
  virtual void getStats(const blink::WebRTCStatsRequest& request) OVERRIDE;
  virtual blink::WebRTCDataChannelHandler* createDataChannel(
      const blink::WebString& label,
      const blink::WebRTCDataChannelInit& init) OVERRIDE;
  virtual blink::WebRTCDTMFSenderHandler* createDTMFSender(
      const blink::WebMediaStreamTrack& track) OVERRIDE;
  virtual void stop() OVERRIDE;

  // WebTask related methods
  WebTaskList* mutable_task_list() { return &task_list_; }

 private:
  MockWebRTCPeerConnectionHandler();

  blink::WebRTCPeerConnectionHandlerClient* client_;
  bool stopped_;
  WebTaskList task_list_;
  blink::WebRTCSessionDescription local_description_;
  blink::WebRTCSessionDescription remote_description_;
  int stream_count_;
  TestInterfaces* interfaces_;

  DISALLOW_COPY_AND_ASSIGN(MockWebRTCPeerConnectionHandler);
};

}  // namespace content

#endif  // CONTENT_SHELL_RENDERER_TEST_RUNNER_MOCKWEBRTCPEERCONNECTIONHANDLER_H_
