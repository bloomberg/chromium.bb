// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/test_runner/web_test_interfaces.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "content/shell/test_runner/mock_web_audio_device.h"
#include "content/shell/test_runner/mock_web_media_stream_center.h"
#include "content/shell/test_runner/mock_web_midi_accessor.h"
#include "content/shell/test_runner/mock_webrtc_peer_connection_handler.h"
#include "content/shell/test_runner/test_interfaces.h"
#include "content/shell/test_runner/test_runner.h"
#include "content/shell/test_runner/web_frame_test_client.h"
#include "content/shell/test_runner/web_view_test_client.h"
#include "content/shell/test_runner/web_view_test_proxy.h"
#include "content/shell/test_runner/web_widget_test_client.h"
#include "content/shell/test_runner/web_widget_test_proxy.h"
#include "third_party/WebKit/public/platform/modules/webmidi/WebMIDIAccessor.h"

using namespace blink;

namespace test_runner {

WebTestInterfaces::WebTestInterfaces() : interfaces_(new TestInterfaces()) {}

WebTestInterfaces::~WebTestInterfaces() {}

void WebTestInterfaces::SetMainView(WebView* web_view) {
  interfaces_->SetMainView(web_view);
}

void WebTestInterfaces::SetDelegate(WebTestDelegate* delegate) {
  interfaces_->SetDelegate(delegate);
}

void WebTestInterfaces::ResetAll() {
  interfaces_->ResetAll();
}

bool WebTestInterfaces::TestIsRunning() {
  return interfaces_->TestIsRunning();
}

void WebTestInterfaces::SetTestIsRunning(bool running) {
  interfaces_->SetTestIsRunning(running);
}

void WebTestInterfaces::ConfigureForTestWithURL(const WebURL& test_url,
                                                bool generate_pixels,
                                                bool initial_configuration) {
  interfaces_->ConfigureForTestWithURL(test_url, generate_pixels,
                                       initial_configuration);
}

WebTestRunner* WebTestInterfaces::TestRunner() {
  return interfaces_->GetTestRunner();
}

WebThemeEngine* WebTestInterfaces::ThemeEngine() {
  return interfaces_->GetThemeEngine();
}

TestInterfaces* WebTestInterfaces::GetTestInterfaces() {
  return interfaces_.get();
}

std::unique_ptr<WebMediaStreamCenter>
WebTestInterfaces::CreateMediaStreamCenter(WebMediaStreamCenterClient* client) {
  return base::MakeUnique<MockWebMediaStreamCenter>();
}

std::unique_ptr<WebRTCPeerConnectionHandler>
WebTestInterfaces::CreateWebRTCPeerConnectionHandler(
    WebRTCPeerConnectionHandlerClient* client) {
  return base::MakeUnique<MockWebRTCPeerConnectionHandler>(client,
                                                           interfaces_.get());
}

std::unique_ptr<WebMIDIAccessor> WebTestInterfaces::CreateMIDIAccessor(
    WebMIDIAccessorClient* client) {
  return base::MakeUnique<MockWebMIDIAccessor>(client, interfaces_.get());
}

std::unique_ptr<WebAudioDevice> WebTestInterfaces::CreateAudioDevice(
    double sample_rate,
    int frames_per_buffer) {
  return base::MakeUnique<MockWebAudioDevice>(sample_rate, frames_per_buffer);
}

std::unique_ptr<WebFrameTestClient> WebTestInterfaces::CreateWebFrameTestClient(
    WebViewTestProxyBase* web_view_test_proxy_base,
    WebFrameTestProxyBase* web_frame_test_proxy_base) {
  // TODO(lukasza): Do not pass the WebTestDelegate below - it's lifetime can
  // differ from the lifetime of WebFrameTestClient - https://crbug.com/606594.
  return base::MakeUnique<WebFrameTestClient>(interfaces_->GetDelegate(),
                                              web_view_test_proxy_base,
                                              web_frame_test_proxy_base);
}

std::unique_ptr<WebViewTestClient> WebTestInterfaces::CreateWebViewTestClient(
    WebViewTestProxyBase* web_view_test_proxy_base) {
  return base::MakeUnique<WebViewTestClient>(web_view_test_proxy_base);
}

std::unique_ptr<WebWidgetTestClient>
WebTestInterfaces::CreateWebWidgetTestClient(
    WebWidgetTestProxyBase* web_widget_test_proxy_base) {
  return base::MakeUnique<WebWidgetTestClient>(web_widget_test_proxy_base);
}

std::vector<blink::WebView*> WebTestInterfaces::GetWindowList() {
  std::vector<blink::WebView*> result;
  for (WebViewTestProxyBase* proxy : interfaces_->GetWindowList())
    result.push_back(proxy->web_view());
  return result;
}

}  // namespace test_runner
