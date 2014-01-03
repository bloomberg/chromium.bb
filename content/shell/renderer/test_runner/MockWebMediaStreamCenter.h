// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_TEST_RUNNER_MOCKWEBMEDIASTREAMCENTER_H_
#define CONTENT_SHELL_RENDERER_TEST_RUNNER_MOCKWEBMEDIASTREAMCENTER_H_

#include "base/basictypes.h"
#include "content/shell/renderer/test_runner/TestCommon.h"
#include "third_party/WebKit/public/platform/WebMediaStreamCenter.h"

namespace blink {
class WebMediaStreamCenterClient;
};

namespace WebTestRunner {

class MockWebMediaStreamCenter : public blink::WebMediaStreamCenter {
public:
    explicit MockWebMediaStreamCenter(blink::WebMediaStreamCenterClient*);

    virtual bool getMediaStreamTrackSources(const blink::WebMediaStreamTrackSourcesRequest&) OVERRIDE;
    virtual void didEnableMediaStreamTrack(const blink::WebMediaStream&, const blink::WebMediaStreamTrack&) OVERRIDE;
    virtual void didDisableMediaStreamTrack(const blink::WebMediaStream&, const blink::WebMediaStreamTrack&) OVERRIDE;
    virtual bool didAddMediaStreamTrack(const blink::WebMediaStream&, const blink::WebMediaStreamTrack&) OVERRIDE;
    virtual bool didRemoveMediaStreamTrack(const blink::WebMediaStream&, const blink::WebMediaStreamTrack&) OVERRIDE;
    virtual void didStopLocalMediaStream(const blink::WebMediaStream&) OVERRIDE;
    virtual bool didStopMediaStreamTrack(const blink::WebMediaStreamTrack&) OVERRIDE;
    virtual void didCreateMediaStream(blink::WebMediaStream&) OVERRIDE;

private:
    MockWebMediaStreamCenter() { }

    DISALLOW_COPY_AND_ASSIGN(MockWebMediaStreamCenter);
};

}

#endif  // CONTENT_SHELL_RENDERER_TEST_RUNNER_MOCKWEBMEDIASTREAMCENTER_H_
