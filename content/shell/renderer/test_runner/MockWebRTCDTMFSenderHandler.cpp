// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/test_runner/MockWebRTCDTMFSenderHandler.h"

#include <assert.h>

#include "content/shell/renderer/test_runner/WebTestDelegate.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"
#include "third_party/WebKit/public/platform/WebRTCDTMFSenderHandlerClient.h"

using namespace blink;

namespace WebTestRunner {

class DTMFSenderToneTask : public WebMethodTask<MockWebRTCDTMFSenderHandler> {
public:
    DTMFSenderToneTask(MockWebRTCDTMFSenderHandler* object, WebRTCDTMFSenderHandlerClient* client)
        : WebMethodTask<MockWebRTCDTMFSenderHandler>(object)
        , m_client(client)
    {
    }

    virtual void runIfValid() OVERRIDE
    {
        WebString tones = m_object->currentToneBuffer();
        m_object->clearToneBuffer();
        m_client->didPlayTone(tones);
    }

private:
    WebRTCDTMFSenderHandlerClient* m_client;
};

/////////////////////

MockWebRTCDTMFSenderHandler::MockWebRTCDTMFSenderHandler(const WebMediaStreamTrack& track, WebTestDelegate* delegate)
    : m_client(0)
    , m_track(track)
    , m_delegate(delegate)
{
}

void MockWebRTCDTMFSenderHandler::setClient(WebRTCDTMFSenderHandlerClient* client)
{
    m_client = client;
}

WebString MockWebRTCDTMFSenderHandler::currentToneBuffer()
{
    return m_toneBuffer;
}

bool MockWebRTCDTMFSenderHandler::canInsertDTMF()
{
    assert(m_client && !m_track.isNull());
    return m_track.source().type() == WebMediaStreamSource::TypeAudio && m_track.isEnabled() && m_track.source().readyState() == WebMediaStreamSource::ReadyStateLive;
}

bool MockWebRTCDTMFSenderHandler::insertDTMF(const WebString& tones, long duration, long interToneGap)
{
    assert(m_client);
    if (!canInsertDTMF())
        return false;

    m_toneBuffer = tones;
    m_delegate->postTask(new DTMFSenderToneTask(this, m_client));
    m_delegate->postTask(new DTMFSenderToneTask(this, m_client));
    return true;
}

}
