// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MockWebMIDIAccessor_h
#define MockWebMIDIAccessor_h

#include "content/shell/renderer/test_runner/TestCommon.h"
#include "content/shell/renderer/test_runner/WebTask.h"
#include "third_party/WebKit/public/platform/WebMIDIAccessor.h"
#include "third_party/WebKit/public/platform/WebNonCopyable.h"

namespace blink {
class WebMIDIAccessorClient;
}

namespace WebTestRunner {

class TestInterfaces;

class MockWebMIDIAccessor : public blink::WebMIDIAccessor, public blink::WebNonCopyable {
public:
    explicit MockWebMIDIAccessor(blink::WebMIDIAccessorClient*, TestInterfaces*);
    virtual ~MockWebMIDIAccessor();

    // blink::WebMIDIAccessor implementation.
    virtual void startSession() OVERRIDE;
    virtual void sendMIDIData(
        unsigned portIndex,
        const unsigned char* data,
        size_t length,
        double timestamp) OVERRIDE { }

    // WebTask related methods
    WebTaskList* taskList() { return &m_taskList; }

private:
    blink::WebMIDIAccessorClient* m_client;
    WebTaskList m_taskList;
    TestInterfaces* m_interfaces;
};

} // namespace WebTestRunner

#endif // MockWebMIDIAccessor_h
