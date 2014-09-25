// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_TEST_RUNNER_WEB_TEST_INTERFACES_H_
#define CONTENT_SHELL_RENDERER_TEST_RUNNER_WEB_TEST_INTERFACES_H_

#include "base/memory/scoped_ptr.h"

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

namespace content {

class TestInterfaces;
class WebTestDelegate;
class WebTestProxyBase;
class WebTestRunner;

class WebTestInterfaces {
 public:
  WebTestInterfaces();
  ~WebTestInterfaces();

  void SetWebView(blink::WebView* web_view, WebTestProxyBase* proxy);
  void SetDelegate(WebTestDelegate* delegate);
  void BindTo(blink::WebFrame* frame);
  void ResetAll();
  void SetTestIsRunning(bool running);
  void ConfigureForTestWithURL(const blink::WebURL& test_url,
                               bool generate_pixels);

  WebTestRunner* TestRunner();
  blink::WebThemeEngine* ThemeEngine();

  blink::WebMediaStreamCenter* CreateMediaStreamCenter(
      blink::WebMediaStreamCenterClient* client);
  blink::WebRTCPeerConnectionHandler* CreateWebRTCPeerConnectionHandler(
      blink::WebRTCPeerConnectionHandlerClient* client);

  blink::WebMIDIAccessor* CreateMIDIAccessor(
      blink::WebMIDIAccessorClient* client);

  blink::WebAudioDevice* CreateAudioDevice(double sample_rate);

  TestInterfaces* GetTestInterfaces();

 private:
  scoped_ptr<TestInterfaces> interfaces_;

  DISALLOW_COPY_AND_ASSIGN(WebTestInterfaces);
};

}  // namespace content

#endif  // CONTENT_SHELL_RENDERER_TEST_RUNNER_WEB_TEST_INTERFACES_H_
