// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/test_runner/web_test_interfaces.h"

#include <utility>

#include "components/test_runner/app_banner_client.h"
#include "components/test_runner/event_sender.h"
#include "components/test_runner/mock_web_audio_device.h"
#include "components/test_runner/mock_web_media_stream_center.h"
#include "components/test_runner/mock_web_midi_accessor.h"
#include "components/test_runner/mock_webrtc_peer_connection_handler.h"
#include "components/test_runner/test_interfaces.h"
#include "components/test_runner/test_runner.h"
#include "components/test_runner/web_frame_test_client.h"
#include "components/test_runner/web_view_test_client.h"

using namespace blink;

namespace test_runner {

WebTestInterfaces::WebTestInterfaces() : interfaces_(new TestInterfaces()) {
}

WebTestInterfaces::~WebTestInterfaces() {
}

void WebTestInterfaces::SetWebView(WebView* web_view, WebTestProxyBase* proxy) {
  interfaces_->SetWebView(web_view, proxy);
}

void WebTestInterfaces::SetDelegate(WebTestDelegate* delegate) {
  interfaces_->SetDelegate(delegate);
}

void WebTestInterfaces::BindTo(WebFrame* frame) {
  interfaces_->BindTo(frame);
}

void WebTestInterfaces::ResetAll() {
  interfaces_->ResetAll();
}

void WebTestInterfaces::SetTestIsRunning(bool running) {
  interfaces_->SetTestIsRunning(running);
}

void WebTestInterfaces::ConfigureForTestWithURL(const WebURL& test_url,
                                                bool generate_pixels) {
  interfaces_->ConfigureForTestWithURL(test_url, generate_pixels);
}

void WebTestInterfaces::SetSendWheelGestures(bool send_gestures) {
  interfaces_->GetEventSender()->set_send_wheel_gestures(send_gestures);
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

WebMediaStreamCenter* WebTestInterfaces::CreateMediaStreamCenter(
    WebMediaStreamCenterClient* client) {
  return new MockWebMediaStreamCenter();
}

WebRTCPeerConnectionHandler*
WebTestInterfaces::CreateWebRTCPeerConnectionHandler(
    WebRTCPeerConnectionHandlerClient* client) {
  return new MockWebRTCPeerConnectionHandler(client, interfaces_.get());
}

WebMIDIAccessor* WebTestInterfaces::CreateMIDIAccessor(
    WebMIDIAccessorClient* client) {
  return new MockWebMIDIAccessor(client, interfaces_.get());
}

WebAudioDevice* WebTestInterfaces::CreateAudioDevice(double sample_rate) {
  return new MockWebAudioDevice(sample_rate);
}

scoped_ptr<blink::WebAppBannerClient>
WebTestInterfaces::CreateAppBannerClient() {
  scoped_ptr<AppBannerClient> client(new AppBannerClient);
  interfaces_->SetAppBannerClient(client.get());
  return std::move(client);
}

scoped_ptr<WebFrameTestClient> WebTestInterfaces::CreateWebFrameTestClient() {
  return make_scoped_ptr(new WebFrameTestClient(
        interfaces_->GetTestRunner(),
        interfaces_->GetDelegate(),
        interfaces_->GetAccessibilityController(),
        interfaces_->GetEventSender()));
}

scoped_ptr<WebViewTestClient> WebTestInterfaces::CreateWebViewTestClient(
    WebTestProxyBase* web_test_proxy_base) {
  return make_scoped_ptr(new WebViewTestClient(interfaces_->GetTestRunner(),
                                               interfaces_->GetEventSender(),
                                               web_test_proxy_base));
}

}  // namespace test_runner
