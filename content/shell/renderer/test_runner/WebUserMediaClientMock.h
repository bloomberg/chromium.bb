// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_TEST_RUNNER_WEBUSERMEDIACLIENTMOCK_H_
#define CONTENT_SHELL_RENDERER_TEST_RUNNER_WEBUSERMEDIACLIENTMOCK_H_

#include "base/basictypes.h"
#include "content/shell/renderer/test_runner/TestCommon.h"
#include "content/shell/renderer/test_runner/WebTask.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/web/WebUserMediaClient.h"

namespace WebTestRunner {

class WebTestDelegate;

class WebUserMediaClientMock : public blink::WebUserMediaClient {
public:
    explicit WebUserMediaClientMock(WebTestDelegate*);
    virtual ~WebUserMediaClientMock() { }

    virtual void requestUserMedia(const blink::WebUserMediaRequest&) OVERRIDE;
    virtual void cancelUserMediaRequest(const blink::WebUserMediaRequest&) OVERRIDE;
    virtual void requestMediaDevices(const blink::WebMediaDevicesRequest&) OVERRIDE;
    virtual void cancelMediaDevicesRequest(const blink::WebMediaDevicesRequest&) OVERRIDE;

    // Task related methods
    WebTaskList* taskList() { return &m_taskList; }

private:
    WebTaskList m_taskList;
    WebTestDelegate* m_delegate;

    DISALLOW_COPY_AND_ASSIGN(WebUserMediaClientMock);
};

}

#endif  // CONTENT_SHELL_RENDERER_TEST_RUNNER_WEBUSERMEDIACLIENTMOCK_H_
