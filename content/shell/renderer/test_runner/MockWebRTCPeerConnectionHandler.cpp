// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/test_runner/MockWebRTCPeerConnectionHandler.h"

#include "content/shell/renderer/test_runner/MockConstraints.h"
#include "content/shell/renderer/test_runner/MockWebRTCDTMFSenderHandler.h"
#include "content/shell/renderer/test_runner/MockWebRTCDataChannelHandler.h"
#include "content/shell/renderer/test_runner/TestInterfaces.h"
#include "content/shell/renderer/test_runner/WebTestDelegate.h"
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

namespace WebTestRunner {

class RTCSessionDescriptionRequestSuccededTask : public WebMethodTask<MockWebRTCPeerConnectionHandler> {
public:
    RTCSessionDescriptionRequestSuccededTask(MockWebRTCPeerConnectionHandler* object, const WebRTCSessionDescriptionRequest& request, const WebRTCSessionDescription& result)
        : WebMethodTask<MockWebRTCPeerConnectionHandler>(object)
        , m_request(request)
        , m_result(result)
    {
    }

    virtual void runIfValid() OVERRIDE
    {
        m_request.requestSucceeded(m_result);
    }

private:
    WebRTCSessionDescriptionRequest m_request;
    WebRTCSessionDescription m_result;
};

class RTCSessionDescriptionRequestFailedTask : public WebMethodTask<MockWebRTCPeerConnectionHandler> {
public:
    RTCSessionDescriptionRequestFailedTask(MockWebRTCPeerConnectionHandler* object, const WebRTCSessionDescriptionRequest& request)
        : WebMethodTask<MockWebRTCPeerConnectionHandler>(object)
        , m_request(request)
    {
    }

    virtual void runIfValid() OVERRIDE
    {
        m_request.requestFailed("TEST_ERROR");
    }

private:
    WebRTCSessionDescriptionRequest m_request;
};

class RTCStatsRequestSucceededTask : public WebMethodTask<MockWebRTCPeerConnectionHandler> {
public:
    RTCStatsRequestSucceededTask(MockWebRTCPeerConnectionHandler* object, const blink::WebRTCStatsRequest& request, const blink::WebRTCStatsResponse& response)
        : WebMethodTask<MockWebRTCPeerConnectionHandler>(object)
        , m_request(request)
        , m_response(response)
    {
    }

    virtual void runIfValid() OVERRIDE
    {
        m_request.requestSucceeded(m_response);
    }

private:
    blink::WebRTCStatsRequest m_request;
    blink::WebRTCStatsResponse m_response;
};

class RTCVoidRequestTask : public WebMethodTask<MockWebRTCPeerConnectionHandler> {
public:
    RTCVoidRequestTask(MockWebRTCPeerConnectionHandler* object, const WebRTCVoidRequest& request, bool succeeded)
        : WebMethodTask<MockWebRTCPeerConnectionHandler>(object)
        , m_request(request)
        , m_succeeded(succeeded)
    {
    }

    virtual void runIfValid() OVERRIDE
    {
        if (m_succeeded)
            m_request.requestSucceeded();
        else
            m_request.requestFailed("TEST_ERROR");
    }

private:
    WebRTCVoidRequest m_request;
    bool m_succeeded;
};

class RTCPeerConnectionStateTask : public WebMethodTask<MockWebRTCPeerConnectionHandler> {
public:
    RTCPeerConnectionStateTask(MockWebRTCPeerConnectionHandler* object, WebRTCPeerConnectionHandlerClient* client, WebRTCPeerConnectionHandlerClient::ICEConnectionState connectionState, WebRTCPeerConnectionHandlerClient::ICEGatheringState gatheringState)
        : WebMethodTask<MockWebRTCPeerConnectionHandler>(object)
        , m_client(client)
        , m_connectionState(connectionState)
        , m_gatheringState(gatheringState)
    {
    }

    virtual void runIfValid() OVERRIDE
    {
        m_client->didChangeICEGatheringState(m_gatheringState);
        m_client->didChangeICEConnectionState(m_connectionState);
    }

private:
    WebRTCPeerConnectionHandlerClient* m_client;
    WebRTCPeerConnectionHandlerClient::ICEConnectionState m_connectionState;
    WebRTCPeerConnectionHandlerClient::ICEGatheringState m_gatheringState;
};

class RemoteDataChannelTask : public WebMethodTask<MockWebRTCPeerConnectionHandler> {
public:
    RemoteDataChannelTask(MockWebRTCPeerConnectionHandler* object, WebRTCPeerConnectionHandlerClient* client, WebTestDelegate* delegate)
        : WebMethodTask<MockWebRTCPeerConnectionHandler>(object)
        , m_client(client)
        , m_delegate(delegate)
    {
    }

    virtual void runIfValid() OVERRIDE
    {
        WebRTCDataChannelInit init;
        WebRTCDataChannelHandler* remoteDataChannel = new MockWebRTCDataChannelHandler("MockRemoteDataChannel", init, m_delegate);
        m_client->didAddRemoteDataChannel(remoteDataChannel);
    }

private:
    WebRTCPeerConnectionHandlerClient* m_client;
    WebTestDelegate* m_delegate;
};

/////////////////////

MockWebRTCPeerConnectionHandler::MockWebRTCPeerConnectionHandler()
{
}

MockWebRTCPeerConnectionHandler::MockWebRTCPeerConnectionHandler(WebRTCPeerConnectionHandlerClient* client, TestInterfaces* interfaces)
    : m_client(client)
    , m_stopped(false)
    , m_streamCount(0)
    , m_interfaces(interfaces)
{
}

bool MockWebRTCPeerConnectionHandler::initialize(const WebRTCConfiguration&, const WebMediaConstraints& constraints)
{
    if (MockConstraints::verifyConstraints(constraints)) {
        m_interfaces->delegate()->postTask(new RTCPeerConnectionStateTask(this, m_client, WebRTCPeerConnectionHandlerClient::ICEConnectionStateCompleted, WebRTCPeerConnectionHandlerClient::ICEGatheringStateComplete));
        return true;
    }

    return false;
}

void MockWebRTCPeerConnectionHandler::createOffer(const WebRTCSessionDescriptionRequest& request, const WebMediaConstraints& constraints)
{
    WebString shouldSucceed;
    if (constraints.getMandatoryConstraintValue("succeed", shouldSucceed) && shouldSucceed == "true") {
        WebRTCSessionDescription sessionDescription;
        sessionDescription.initialize("offer", "local");
        m_interfaces->delegate()->postTask(new RTCSessionDescriptionRequestSuccededTask(this, request, sessionDescription));
    } else
        m_interfaces->delegate()->postTask(new RTCSessionDescriptionRequestFailedTask(this, request));
}

void MockWebRTCPeerConnectionHandler::createAnswer(const WebRTCSessionDescriptionRequest& request, const WebMediaConstraints&)
{
    if (!m_remoteDescription.isNull()) {
        WebRTCSessionDescription sessionDescription;
        sessionDescription.initialize("answer", "local");
        m_interfaces->delegate()->postTask(new RTCSessionDescriptionRequestSuccededTask(this, request, sessionDescription));
    } else
        m_interfaces->delegate()->postTask(new RTCSessionDescriptionRequestFailedTask(this, request));
}

void MockWebRTCPeerConnectionHandler::setLocalDescription(const WebRTCVoidRequest& request, const WebRTCSessionDescription& localDescription)
{
    if (!localDescription.isNull() && localDescription.sdp() == "local") {
        m_localDescription = localDescription;
        m_interfaces->delegate()->postTask(new RTCVoidRequestTask(this, request, true));
    } else
        m_interfaces->delegate()->postTask(new RTCVoidRequestTask(this, request, false));
}

void MockWebRTCPeerConnectionHandler::setRemoteDescription(const WebRTCVoidRequest& request, const WebRTCSessionDescription& remoteDescription)
{
    if (!remoteDescription.isNull() && remoteDescription.sdp() == "remote") {
        m_remoteDescription = remoteDescription;
        m_interfaces->delegate()->postTask(new RTCVoidRequestTask(this, request, true));
    } else
        m_interfaces->delegate()->postTask(new RTCVoidRequestTask(this, request, false));
}

WebRTCSessionDescription MockWebRTCPeerConnectionHandler::localDescription()
{
    return m_localDescription;
}

WebRTCSessionDescription MockWebRTCPeerConnectionHandler::remoteDescription()
{
    return m_remoteDescription;
}

bool MockWebRTCPeerConnectionHandler::updateICE(const WebRTCConfiguration&, const WebMediaConstraints&)
{
    return true;
}

bool MockWebRTCPeerConnectionHandler::addICECandidate(const WebRTCICECandidate& iceCandidate)
{
    m_client->didGenerateICECandidate(iceCandidate);
    return true;
}

bool MockWebRTCPeerConnectionHandler::addICECandidate(const WebRTCVoidRequest& request, const WebRTCICECandidate& iceCandidate)
{
    m_interfaces->delegate()->postTask(new RTCVoidRequestTask(this, request, true));
    return true;
}

bool MockWebRTCPeerConnectionHandler::addStream(const WebMediaStream& stream, const WebMediaConstraints&)
{
    ++m_streamCount;
    m_client->negotiationNeeded();
    return true;
}

void MockWebRTCPeerConnectionHandler::removeStream(const WebMediaStream& stream)
{
    --m_streamCount;
    m_client->negotiationNeeded();
}

void MockWebRTCPeerConnectionHandler::getStats(const WebRTCStatsRequest& request)
{
    WebRTCStatsResponse response = request.createResponse();
    double currentDate = m_interfaces->delegate()->getCurrentTimeInMillisecond();
    if (request.hasSelector()) {
        // FIXME: There is no check that the fetched values are valid.
        size_t reportIndex = response.addReport("Mock video", "ssrc", currentDate);
        response.addStatistic(reportIndex, "type", "video");
    } else {
        for (int i = 0; i < m_streamCount; ++i) {
            size_t reportIndex = response.addReport("Mock audio", "ssrc", currentDate);
            response.addStatistic(reportIndex, "type", "audio");
            reportIndex = response.addReport("Mock video", "ssrc", currentDate);
            response.addStatistic(reportIndex, "type", "video");
        }
    }
    m_interfaces->delegate()->postTask(new RTCStatsRequestSucceededTask(this, request, response));
}

WebRTCDataChannelHandler* MockWebRTCPeerConnectionHandler::createDataChannel(const WebString& label, const blink::WebRTCDataChannelInit& init)
{
    m_interfaces->delegate()->postTask(new RemoteDataChannelTask(this, m_client, m_interfaces->delegate()));

    return new MockWebRTCDataChannelHandler(label, init, m_interfaces->delegate());
}

WebRTCDTMFSenderHandler* MockWebRTCPeerConnectionHandler::createDTMFSender(const WebMediaStreamTrack& track)
{
    return new MockWebRTCDTMFSenderHandler(track, m_interfaces->delegate());
}

void MockWebRTCPeerConnectionHandler::stop()
{
    m_stopped = true;
}

}
