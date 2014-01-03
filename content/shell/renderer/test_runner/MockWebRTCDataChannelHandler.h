// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_TEST_RUNNER_MOCKWEBRTCDATACHANNELHANDLER_H_
#define CONTENT_SHELL_RENDERER_TEST_RUNNER_MOCKWEBRTCDATACHANNELHANDLER_H_

#include "base/basictypes.h"
#include "content/shell/renderer/test_runner/TestCommon.h"
#include "content/shell/renderer/test_runner/WebTask.h"
#include "third_party/WebKit/public/platform/WebRTCDataChannelHandler.h"
#include "third_party/WebKit/public/platform/WebRTCDataChannelInit.h"
#include "third_party/WebKit/public/platform/WebString.h"

namespace WebTestRunner {

class WebTestDelegate;

class MockWebRTCDataChannelHandler : public blink::WebRTCDataChannelHandler {
public:
    MockWebRTCDataChannelHandler(blink::WebString label, const blink::WebRTCDataChannelInit&, WebTestDelegate*);

    virtual void setClient(blink::WebRTCDataChannelHandlerClient*) OVERRIDE;
    virtual blink::WebString label() OVERRIDE;
    virtual bool isReliable() OVERRIDE;
    virtual bool ordered() const OVERRIDE;
    virtual unsigned short maxRetransmitTime() const OVERRIDE;
    virtual unsigned short maxRetransmits() const OVERRIDE;
    virtual blink::WebString protocol() const OVERRIDE;
    virtual bool negotiated() const OVERRIDE;
    virtual unsigned short id() const OVERRIDE;
    virtual unsigned long bufferedAmount() OVERRIDE;
    virtual bool sendStringData(const blink::WebString&) OVERRIDE;
    virtual bool sendRawData(const char*, size_t) OVERRIDE;
    virtual void close() OVERRIDE;

    // WebTask related methods
    WebTaskList* taskList() { return &m_taskList; }

private:
    MockWebRTCDataChannelHandler();

    blink::WebRTCDataChannelHandlerClient* m_client;
    blink::WebString m_label;
    blink::WebRTCDataChannelInit m_init;
    bool m_reliable;
    WebTaskList m_taskList;
    WebTestDelegate* m_delegate;

    DISALLOW_COPY_AND_ASSIGN(MockWebRTCDataChannelHandler);
};

}

#endif  // CONTENT_SHELL_RENDERER_TEST_RUNNER_MOCKWEBRTCDATACHANNELHANDLER_H_
