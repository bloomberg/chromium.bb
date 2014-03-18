// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_TEST_RUNNER_MOCKWEBMEDIASTREAMCENTER_H_
#define CONTENT_SHELL_RENDERER_TEST_RUNNER_MOCKWEBMEDIASTREAMCENTER_H_

#include "third_party/WebKit/public/platform/WebMediaStreamCenter.h"

#include "base/basictypes.h"
#include "content/shell/renderer/test_runner/TestCommon.h"
#include "content/shell/renderer/test_runner/WebTask.h"

namespace blink {
class WebMediaStreamCenterClient;
};

namespace WebTestRunner {

class TestInterfaces;

class MockWebMediaStreamCenter : public blink::WebMediaStreamCenter {
public:
    MockWebMediaStreamCenter(blink::WebMediaStreamCenterClient*, TestInterfaces*);

    virtual bool getMediaStreamTrackSources(const blink::WebMediaStreamTrackSourcesRequest&) OVERRIDE;
    virtual void didEnableMediaStreamTrack(const blink::WebMediaStreamTrack&) OVERRIDE;
    virtual void didDisableMediaStreamTrack(const blink::WebMediaStreamTrack&) OVERRIDE;
    virtual bool didAddMediaStreamTrack(const blink::WebMediaStream&, const blink::WebMediaStreamTrack&) OVERRIDE;
    virtual bool didRemoveMediaStreamTrack(const blink::WebMediaStream&, const blink::WebMediaStreamTrack&) OVERRIDE;
    virtual void didStopLocalMediaStream(const blink::WebMediaStream&) OVERRIDE;
    virtual bool didStopMediaStreamTrack(const blink::WebMediaStreamTrack&) OVERRIDE;
    virtual void didCreateMediaStream(blink::WebMediaStream&) OVERRIDE;

   // Task related methods
    WebTaskList* taskList() { return &m_taskList; }

private:
    WebTaskList m_taskList;
    TestInterfaces* m_interfaces;

    DISALLOW_COPY_AND_ASSIGN(MockWebMediaStreamCenter);
};

}

#endif  // CONTENT_SHELL_RENDERER_TEST_RUNNER_MOCKWEBMEDIASTREAMCENTER_H_
