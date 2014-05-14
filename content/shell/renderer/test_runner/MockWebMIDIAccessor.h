// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_TEST_RUNNER_MOCKWEBMIDIACCESSOR_H_
#define CONTENT_SHELL_RENDERER_TEST_RUNNER_MOCKWEBMIDIACCESSOR_H_

#include "base/basictypes.h"
#include "content/shell/renderer/test_runner/TestCommon.h"
#include "content/shell/renderer/test_runner/WebTask.h"
#include "third_party/WebKit/public/platform/WebMIDIAccessor.h"

namespace blink {
class WebMIDIAccessorClient;
}

namespace content {

class TestInterfaces;

class MockWebMIDIAccessor : public blink::WebMIDIAccessor {
public:
    explicit MockWebMIDIAccessor(blink::WebMIDIAccessorClient*, TestInterfaces*);
    virtual ~MockWebMIDIAccessor();

    // blink::WebMIDIAccessor implementation.
    virtual void startSession() OVERRIDE;
    virtual void sendMIDIData(unsigned portIndex,
                              const unsigned char* data,
                              size_t length,
                              double timestamp) OVERRIDE {}

    // WebTask related methods
    WebTaskList* mutable_task_list() { return &m_taskList; }

private:
    blink::WebMIDIAccessorClient* m_client;
    WebTaskList m_taskList;
    TestInterfaces* m_interfaces;

    DISALLOW_COPY_AND_ASSIGN(MockWebMIDIAccessor);
};

}  // namespace content

#endif  // CONTENT_SHELL_RENDERER_TEST_RUNNER_MOCKWEBMIDIACCESSOR_H_
