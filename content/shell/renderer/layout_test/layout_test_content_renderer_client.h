// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_LAYOUT_TEST_CONTENT_RENDERER_CLIENT_H_
#define CONTENT_SHELL_LAYOUT_TEST_CONTENT_RENDERER_CLIENT_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "content/shell/renderer/shell_content_renderer_client.h"

namespace blink {
class WebFrame;
class WebPlugin;
struct WebPluginParams;
}

namespace web_cache {
class WebCacheRenderProcessObserver;
}

namespace content {

class MockWebClipboardImpl;
class ShellRenderProcessObserver;
class WebTestProxyBase;

class LayoutTestContentRendererClient : public ShellContentRendererClient {
 public:
  LayoutTestContentRendererClient();
  virtual ~LayoutTestContentRendererClient();

  // ShellContentRendererClient implementation.
  virtual void RenderViewCreated(RenderView* render_view) override;
  virtual blink::WebMediaStreamCenter* OverrideCreateWebMediaStreamCenter(
      blink::WebMediaStreamCenterClient* client) override;
  virtual blink::WebRTCPeerConnectionHandler*
  OverrideCreateWebRTCPeerConnectionHandler(
      blink::WebRTCPeerConnectionHandlerClient* client) override;
  virtual blink::WebMIDIAccessor* OverrideCreateMIDIAccessor(
      blink::WebMIDIAccessorClient* client) override;
  virtual blink::WebAudioDevice* OverrideCreateAudioDevice(
      double sample_rate) override;
  virtual blink::WebClipboard* OverrideWebClipboard() override;
  virtual blink::WebThemeEngine* OverrideThemeEngine() override;

 private:
  void WebTestProxyCreated(RenderView* render_view, WebTestProxyBase* proxy);

  scoped_ptr<web_cache::WebCacheRenderProcessObserver> web_cache_observer_;
  scoped_ptr<ShellRenderProcessObserver> shell_observer_;
  scoped_ptr<MockWebClipboardImpl> clipboard_;
};

}  // namespace content

#endif  // CONTENT_SHELL_LAYOUT_TEST_CONTENT_RENDERER_CLIENT_H_
