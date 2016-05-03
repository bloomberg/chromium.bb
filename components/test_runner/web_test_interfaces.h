// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TEST_RUNNER_WEB_TEST_INTERFACES_H_
#define COMPONENTS_TEST_RUNNER_WEB_TEST_INTERFACES_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "components/test_runner/test_runner_export.h"

namespace blink {
class WebAppBannerClient;
class WebAudioDevice;
class WebFrame;
class WebFrameClient;
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

namespace test_runner {

class AppBannerClient;
class TestInterfaces;
class WebFrameTestClient;
class WebTestDelegate;
class WebTestProxyBase;
class WebTestRunner;
class WebViewTestClient;
class WebWidgetTestClient;

class TEST_RUNNER_EXPORT WebTestInterfaces {
 public:
  WebTestInterfaces();
  ~WebTestInterfaces();

  void SetMainView(blink::WebView* web_view);
  void SetDelegate(WebTestDelegate* delegate);
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

  std::unique_ptr<blink::WebAppBannerClient> CreateAppBannerClient();

  TestInterfaces* GetTestInterfaces();

  // Creates a WebFrameClient implementation providing test behavior (i.e.
  // forwarding javascript console output to the test harness).  The caller
  // should guarantee that the returned object won't be used beyond the lifetime
  // of WebTestInterfaces and/or the lifetime of |web_test_proxy_base|.
  std::unique_ptr<WebFrameTestClient> CreateWebFrameTestClient(
      WebTestProxyBase* web_test_proxy_base);

  // Creates a WebViewClient implementation providing test behavior (i.e.
  // providing a mocked speech recognizer).  The caller should guarantee that
  // the returned pointer won't be used beyond the lifetime of WebTestInterfaces
  // and/or the lifetime of |web_test_proxy_base|.
  std::unique_ptr<WebViewTestClient> CreateWebViewTestClient(
      WebTestProxyBase* web_test_proxy_base);

  // Creates a WebWidgetClient implementation providing test behavior (i.e.
  // providing a mocked screen orientation).  The caller should guarantee that
  // the returned pointer won't be used beyond the lifetime of WebTestInterfaces
  // and/or the lifetime of |web_test_proxy_base|.
  std::unique_ptr<WebWidgetTestClient> CreateWebWidgetTestClient(
      WebTestProxyBase* web_test_proxy_base);

  // Gets a list of currently opened windows created by the current test.
  std::vector<blink::WebView*> GetWindowList();

 private:
  std::unique_ptr<TestInterfaces> interfaces_;

  DISALLOW_COPY_AND_ASSIGN(WebTestInterfaces);
};

}  // namespace test_runner

#endif  // COMPONENTS_TEST_RUNNER_WEB_TEST_INTERFACES_H_
