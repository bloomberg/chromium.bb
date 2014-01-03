// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/test_runner/MockWebRTCDataChannelHandler.h"

#include <assert.h>

#include "content/shell/renderer/test_runner/WebTestDelegate.h"
#include "third_party/WebKit/public/platform/WebRTCDataChannelHandlerClient.h"

using namespace blink;

namespace WebTestRunner {

class DataChannelReadyStateTask : public WebMethodTask<MockWebRTCDataChannelHandler> {
public:
    DataChannelReadyStateTask(MockWebRTCDataChannelHandler* object, WebRTCDataChannelHandlerClient* dataChannelClient, WebRTCDataChannelHandlerClient::ReadyState state)
        : WebMethodTask<MockWebRTCDataChannelHandler>(object)
        , m_dataChannelClient(dataChannelClient)
        , m_state(state)
    {
    }

    virtual void runIfValid() OVERRIDE
    {
        m_dataChannelClient->didChangeReadyState(m_state);
    }

private:
    WebRTCDataChannelHandlerClient* m_dataChannelClient;
    WebRTCDataChannelHandlerClient::ReadyState m_state;
};

/////////////////////

MockWebRTCDataChannelHandler::MockWebRTCDataChannelHandler(WebString label, const WebRTCDataChannelInit& init, WebTestDelegate* delegate)
    : m_client(0)
    , m_label(label)
    , m_init(init)
    , m_delegate(delegate)
{
    m_reliable = (init.ordered && init.maxRetransmits == -1 && init.maxRetransmitTime == -1);
}

void MockWebRTCDataChannelHandler::setClient(WebRTCDataChannelHandlerClient* client)
{
    m_client = client;
    if (m_client)
        m_delegate->postTask(new DataChannelReadyStateTask(this, m_client, WebRTCDataChannelHandlerClient::ReadyStateOpen));
}

blink::WebString MockWebRTCDataChannelHandler::label()
{
    return m_label;
}

bool MockWebRTCDataChannelHandler::isReliable()
{
    return m_reliable;
}

bool MockWebRTCDataChannelHandler::ordered() const
{
    return m_init.ordered;
}

unsigned short MockWebRTCDataChannelHandler::maxRetransmitTime() const
{
    return m_init.maxRetransmitTime;
}

unsigned short MockWebRTCDataChannelHandler::maxRetransmits() const
{
    return m_init.maxRetransmits;
}

WebString MockWebRTCDataChannelHandler::protocol() const
{
    return m_init.protocol;
}

bool MockWebRTCDataChannelHandler::negotiated() const
{
    return m_init.negotiated;
}

unsigned short MockWebRTCDataChannelHandler::id() const
{
    return m_init.id;
}

unsigned long MockWebRTCDataChannelHandler::bufferedAmount()
{
    return 0;
}

bool MockWebRTCDataChannelHandler::sendStringData(const WebString& data)
{
    assert(m_client);
    m_client->didReceiveStringData(data);
    return true;
}

bool MockWebRTCDataChannelHandler::sendRawData(const char* data, size_t size)
{
    assert(m_client);
    m_client->didReceiveRawData(data, size);
    return true;
}

void MockWebRTCDataChannelHandler::close()
{
    assert(m_client);
    m_delegate->postTask(new DataChannelReadyStateTask(this, m_client, WebRTCDataChannelHandlerClient::ReadyStateClosed));
}

}
