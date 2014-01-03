// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_TEST_RUNNER_MOCKWEBRTCDTMFSENDERHANDLER_H_
#define CONTENT_SHELL_RENDERER_TEST_RUNNER_MOCKWEBRTCDTMFSENDERHANDLER_H_

#include "base/basictypes.h"
#include "content/shell/renderer/test_runner/TestCommon.h"
#include "content/shell/renderer/test_runner/WebTask.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"
#include "third_party/WebKit/public/platform/WebRTCDTMFSenderHandler.h"
#include "third_party/WebKit/public/platform/WebString.h"

namespace WebTestRunner {

class WebTestDelegate;

class MockWebRTCDTMFSenderHandler : public blink::WebRTCDTMFSenderHandler {
public:
    MockWebRTCDTMFSenderHandler(const blink::WebMediaStreamTrack&, WebTestDelegate*);

    virtual void setClient(blink::WebRTCDTMFSenderHandlerClient*) OVERRIDE;

    virtual blink::WebString currentToneBuffer() OVERRIDE;

    virtual bool canInsertDTMF() OVERRIDE;
    virtual bool insertDTMF(const blink::WebString& tones, long duration, long interToneGap) OVERRIDE;

    // WebTask related methods
    WebTaskList* taskList() { return &m_taskList; }
    void clearToneBuffer() { m_toneBuffer.reset(); }

private:
    MockWebRTCDTMFSenderHandler();

    blink::WebRTCDTMFSenderHandlerClient* m_client;
    blink::WebMediaStreamTrack m_track;
    blink::WebString m_toneBuffer;
    WebTaskList m_taskList;
    WebTestDelegate* m_delegate;

    DISALLOW_COPY_AND_ASSIGN(MockWebRTCDTMFSenderHandler);
};

}

#endif  // CONTENT_SHELL_RENDERER_TEST_RUNNER_MOCKWEBRTCDTMFSENDERHANDLER_H_
