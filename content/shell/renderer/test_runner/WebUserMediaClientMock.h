// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebUserMediaClientMock_h
#define WebUserMediaClientMock_h

#include "content/shell/renderer/test_runner/TestCommon.h"
#include "content/shell/renderer/test_runner/WebTask.h"
#include "third_party/WebKit/public/platform/WebCommon.h"
#include "third_party/WebKit/public/platform/WebNonCopyable.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/web/WebUserMediaClient.h"

namespace WebTestRunner {

class WebTestDelegate;

class WebUserMediaClientMock : public blink::WebUserMediaClient, public blink::WebNonCopyable {
public:
    explicit WebUserMediaClientMock(WebTestDelegate*);
    ~WebUserMediaClientMock() { }

    virtual void requestUserMedia(const blink::WebUserMediaRequest&) OVERRIDE;
    virtual void cancelUserMediaRequest(const blink::WebUserMediaRequest&) OVERRIDE;

    // Task related methods
    WebTaskList* taskList() { return &m_taskList; }

private:
    WebTaskList m_taskList;
    WebTestDelegate* m_delegate;
};

}

#endif // WebUserMediaClientMock_h
