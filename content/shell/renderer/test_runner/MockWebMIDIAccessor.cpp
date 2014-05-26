// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/test_runner/MockWebMIDIAccessor.h"

#include "content/shell/renderer/test_runner/TestInterfaces.h"
#include "content/shell/renderer/test_runner/WebTestDelegate.h"
#include "content/shell/renderer/test_runner/test_runner.h"
#include "content/shell/renderer/test_runner/web_test_runner.h"
#include "third_party/WebKit/public/platform/WebMIDIAccessorClient.h"

using namespace blink;

namespace content {

namespace {

class DidStartSessionTask : public WebMethodTask<MockWebMIDIAccessor> {
public:
    DidStartSessionTask(MockWebMIDIAccessor* object, blink::WebMIDIAccessorClient* client, bool result)
        : WebMethodTask<MockWebMIDIAccessor>(object)
        , m_client(client)
        , m_result(result)
    {
    }

    virtual void runIfValid() OVERRIDE
    {
        m_client->didStartSession(m_result, "InvalidStateError", "");
    }

private:
    blink::WebMIDIAccessorClient* m_client;
    bool m_result;
};

}  // namespace

MockWebMIDIAccessor::MockWebMIDIAccessor(blink::WebMIDIAccessorClient* client, TestInterfaces* interfaces)
    : m_client(client)
    , m_interfaces(interfaces)
{
}

MockWebMIDIAccessor::~MockWebMIDIAccessor()
{
}

void MockWebMIDIAccessor::startSession()
{
    // Add a mock input and output port.
    m_client->didAddInputPort("MockInputID", "MockInputManufacturer", "MockInputName", "MockInputVersion");
    m_client->didAddOutputPort("MockOutputID", "MockOutputManufacturer", "MockOutputName", "MockOutputVersion");
    m_interfaces->delegate()->postTask(new DidStartSessionTask(this, m_client, m_interfaces->testRunner()->midiAccessorResult()));
}

}  // namespace content
