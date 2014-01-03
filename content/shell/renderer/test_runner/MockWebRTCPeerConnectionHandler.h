// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_TEST_RUNNER_MOCKWEBRTCPEERCONNECTIONHANDLER_H_
#define CONTENT_SHELL_RENDERER_TEST_RUNNER_MOCKWEBRTCPEERCONNECTIONHANDLER_H_

#include "base/basictypes.h"
#include "content/shell/renderer/test_runner/TestCommon.h"
#include "content/shell/renderer/test_runner/WebTask.h"
#include "third_party/WebKit/public/platform/WebRTCPeerConnectionHandler.h"
#include "third_party/WebKit/public/platform/WebRTCSessionDescription.h"
#include "third_party/WebKit/public/platform/WebRTCSessionDescriptionRequest.h"
#include "third_party/WebKit/public/platform/WebRTCStatsRequest.h"

namespace blink {
class WebRTCPeerConnectionHandlerClient;
};

namespace WebTestRunner {

class TestInterfaces;

class MockWebRTCPeerConnectionHandler : public blink::WebRTCPeerConnectionHandler {
public:
    MockWebRTCPeerConnectionHandler(blink::WebRTCPeerConnectionHandlerClient*, TestInterfaces*);

    virtual bool initialize(const blink::WebRTCConfiguration&, const blink::WebMediaConstraints&) OVERRIDE;

    virtual void createOffer(const blink::WebRTCSessionDescriptionRequest&, const blink::WebMediaConstraints&) OVERRIDE;
    virtual void createAnswer(const blink::WebRTCSessionDescriptionRequest&, const blink::WebMediaConstraints&) OVERRIDE;
    virtual void setLocalDescription(const blink::WebRTCVoidRequest&, const blink::WebRTCSessionDescription&) OVERRIDE;
    virtual void setRemoteDescription(const blink::WebRTCVoidRequest&, const blink::WebRTCSessionDescription&) OVERRIDE;
    virtual blink::WebRTCSessionDescription localDescription() OVERRIDE;
    virtual blink::WebRTCSessionDescription remoteDescription() OVERRIDE;
    virtual bool updateICE(const blink::WebRTCConfiguration&, const blink::WebMediaConstraints&) OVERRIDE;
    virtual bool addICECandidate(const blink::WebRTCICECandidate&) OVERRIDE;
    virtual bool addICECandidate(const blink::WebRTCVoidRequest&, const blink::WebRTCICECandidate&) OVERRIDE;
    virtual bool addStream(const blink::WebMediaStream&, const blink::WebMediaConstraints&) OVERRIDE;
    virtual void removeStream(const blink::WebMediaStream&) OVERRIDE;
    virtual void getStats(const blink::WebRTCStatsRequest&) OVERRIDE;
    virtual blink::WebRTCDataChannelHandler* createDataChannel(const blink::WebString& label, const blink::WebRTCDataChannelInit&) OVERRIDE;
    virtual blink::WebRTCDTMFSenderHandler* createDTMFSender(const blink::WebMediaStreamTrack&) OVERRIDE;
    virtual void stop() OVERRIDE;

    // WebTask related methods
    WebTaskList* taskList() { return &m_taskList; }

private:
    MockWebRTCPeerConnectionHandler();

    blink::WebRTCPeerConnectionHandlerClient* m_client;
    bool m_stopped;
    WebTaskList m_taskList;
    blink::WebRTCSessionDescription m_localDescription;
    blink::WebRTCSessionDescription m_remoteDescription;
    int m_streamCount;
    TestInterfaces* m_interfaces;

    DISALLOW_COPY_AND_ASSIGN(MockWebRTCPeerConnectionHandler);
};

}

#endif  // CONTENT_SHELL_RENDERER_TEST_RUNNER_MOCKWEBRTCPEERCONNECTIONHANDLER_H_
