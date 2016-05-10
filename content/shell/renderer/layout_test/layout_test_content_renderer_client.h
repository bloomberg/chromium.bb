// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_LAYOUT_TEST_LAYOUT_TEST_CONTENT_RENDERER_CLIENT_H_
#define CONTENT_SHELL_RENDERER_LAYOUT_TEST_LAYOUT_TEST_CONTENT_RENDERER_CLIENT_H_

#include <memory>

#include "content/shell/renderer/shell_content_renderer_client.h"

namespace content {

class LayoutTestRenderThreadObserver;
class MockWebClipboardImpl;

class LayoutTestContentRendererClient : public ShellContentRendererClient {
 public:
  LayoutTestContentRendererClient();
  ~LayoutTestContentRendererClient() override;

  // ShellContentRendererClient implementation.
  void RenderThreadStarted() override;
  void RenderFrameCreated(RenderFrame* render_frame) override;
  void RenderViewCreated(RenderView* render_view) override;
  blink::WebMediaStreamCenter* OverrideCreateWebMediaStreamCenter(
      blink::WebMediaStreamCenterClient* client) override;
  blink::WebRTCPeerConnectionHandler* OverrideCreateWebRTCPeerConnectionHandler(
      blink::WebRTCPeerConnectionHandlerClient* client) override;
  blink::WebMIDIAccessor* OverrideCreateMIDIAccessor(
      blink::WebMIDIAccessorClient* client) override;
  blink::WebAudioDevice* OverrideCreateAudioDevice(double sample_rate) override;
  blink::WebClipboard* OverrideWebClipboard() override;
  blink::WebThemeEngine* OverrideThemeEngine() override;
  std::unique_ptr<blink::WebAppBannerClient> CreateAppBannerClient(
      RenderFrame* render_frame) override;
  std::unique_ptr<MediaStreamRendererFactory> CreateMediaStreamRendererFactory()
      override;
  void DidInitializeWorkerContextOnWorkerThread(
      v8::Local<v8::Context> context) override;

 private:
  std::unique_ptr<LayoutTestRenderThreadObserver> shell_observer_;
  std::unique_ptr<MockWebClipboardImpl> clipboard_;
};

}  // namespace content

#endif  // CONTENT_SHELL_RENDERER_LAYOUT_TEST_LAYOUT_TEST_CONTENT_RENDERER_CLIENT_H_
