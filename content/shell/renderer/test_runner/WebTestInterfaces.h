// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebTestInterfaces_h
#define WebTestInterfaces_h

#include "content/shell/renderer/test_runner/WebScopedPtr.h"

namespace blink {
class WebAudioDevice;
class WebFrame;
class WebMediaStreamCenter;
class WebMediaStreamCenterClient;
class WebMIDIAccessor;
class WebMIDIAccessorClient;
class WebRTCPeerConnectionHandler;
class WebRTCPeerConnectionHandlerClient;
class WebThemeEngine;
class WebURL;
class WebView;
}

namespace WebTestRunner {

class TestInterfaces;
class WebTestDelegate;
class WebTestProxyBase;
class WebTestRunner;

class WebTestInterfaces {
public:
    WebTestInterfaces();
    ~WebTestInterfaces();

    void setWebView(blink::WebView*, WebTestProxyBase*);
    void setDelegate(WebTestDelegate*);
    void bindTo(blink::WebFrame*);
    void resetAll();
    void setTestIsRunning(bool);
    void configureForTestWithURL(const blink::WebURL&, bool generatePixels);

    WebTestRunner* testRunner();
    blink::WebThemeEngine* themeEngine();

    blink::WebMediaStreamCenter* createMediaStreamCenter(blink::WebMediaStreamCenterClient*);
    blink::WebRTCPeerConnectionHandler* createWebRTCPeerConnectionHandler(blink::WebRTCPeerConnectionHandlerClient*);

    blink::WebMIDIAccessor* createMIDIAccessor(blink::WebMIDIAccessorClient*);

    blink::WebAudioDevice* createAudioDevice(double sampleRate);

    TestInterfaces* testInterfaces();

private:
    WebScopedPtr<TestInterfaces> m_interfaces;
};

}

#endif // WebTestInterfaces_h
