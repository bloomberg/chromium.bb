// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/test_runner/MockColorChooser.h"

#include "content/shell/renderer/test_runner/WebTestDelegate.h"
#include "content/shell/renderer/test_runner/web_test_proxy.h"

using namespace blink;

namespace content {

namespace {
class HostMethodTask : public WebMethodTask<MockColorChooser> {
public:
    typedef void (MockColorChooser::*CallbackMethodType)();
    HostMethodTask(MockColorChooser* object, CallbackMethodType callback)
        : WebMethodTask<MockColorChooser>(object)
        , m_callback(callback)
    { }

    virtual void runIfValid() OVERRIDE { (m_object->*m_callback)(); }

private:
    CallbackMethodType m_callback;
};
}

MockColorChooser::MockColorChooser(blink::WebColorChooserClient* client, WebTestDelegate* delegate, WebTestProxyBase* proxy)
    : m_client(client)
    , m_delegate(delegate)
    , m_proxy(proxy)
{
    m_proxy->DidOpenChooser();
}

MockColorChooser::~MockColorChooser()
{
    m_proxy->DidCloseChooser();
}

void MockColorChooser::setSelectedColor(const blink::WebColor)
{
}

void MockColorChooser::endChooser()
{
    m_delegate->postDelayedTask(new HostMethodTask(this, &MockColorChooser::invokeDidEndChooser), 0);
}

void MockColorChooser::invokeDidEndChooser()
{
    m_client->didEndChooser();
}

}  // namespace content
