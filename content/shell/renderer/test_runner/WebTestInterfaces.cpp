// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/test_runner/WebTestInterfaces.h"

#include "content/shell/renderer/test_runner/MockWebMIDIAccessor.h"
#include "content/shell/renderer/test_runner/MockWebMediaStreamCenter.h"
#include "content/shell/renderer/test_runner/TestInterfaces.h"
#include "content/shell/renderer/test_runner/mock_web_audio_device.h"
#include "content/shell/renderer/test_runner/mock_webrtc_peer_connection_handler.h"
#include "content/shell/renderer/test_runner/test_runner.h"

using namespace blink;

namespace content {

WebTestInterfaces::WebTestInterfaces()
    : m_interfaces(new TestInterfaces())
{
}

WebTestInterfaces::~WebTestInterfaces()
{
}

void WebTestInterfaces::setWebView(WebView* webView, WebTestProxyBase* proxy)
{
    m_interfaces->setWebView(webView, proxy);
}

void WebTestInterfaces::setDelegate(WebTestDelegate* delegate)
{
    m_interfaces->setDelegate(delegate);
}

void WebTestInterfaces::bindTo(WebFrame* frame)
{
    m_interfaces->bindTo(frame);
}

void WebTestInterfaces::resetAll()
{
    m_interfaces->resetAll();
}

void WebTestInterfaces::setTestIsRunning(bool running)
{
    m_interfaces->setTestIsRunning(running);
}

void WebTestInterfaces::configureForTestWithURL(const WebURL& testURL, bool generatePixels)
{
    m_interfaces->configureForTestWithURL(testURL, generatePixels);
}

WebTestRunner* WebTestInterfaces::testRunner()
{
    return m_interfaces->testRunner();
}

WebThemeEngine* WebTestInterfaces::themeEngine()
{
    return m_interfaces->themeEngine();
}

TestInterfaces* WebTestInterfaces::testInterfaces()
{
    return m_interfaces.get();
}

WebMediaStreamCenter* WebTestInterfaces::createMediaStreamCenter(WebMediaStreamCenterClient* client)
{
    return new MockWebMediaStreamCenter(client, m_interfaces.get());
}

WebRTCPeerConnectionHandler* WebTestInterfaces::createWebRTCPeerConnectionHandler(WebRTCPeerConnectionHandlerClient* client)
{
    return new MockWebRTCPeerConnectionHandler(client, m_interfaces.get());
}

WebMIDIAccessor* WebTestInterfaces::createMIDIAccessor(WebMIDIAccessorClient* client)
{
    return new MockWebMIDIAccessor(client, m_interfaces.get());
}

WebAudioDevice* WebTestInterfaces::createAudioDevice(double sampleRate)
{
    return new MockWebAudioDevice(sampleRate);
}

}  // namespace content
