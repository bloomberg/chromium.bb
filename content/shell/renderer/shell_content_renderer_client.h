// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_SHELL_CONTENT_RENDERER_CLIENT_H_
#define CONTENT_SHELL_SHELL_CONTENT_RENDERER_CLIENT_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/renderer/content_renderer_client.h"

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

class ShellContentRendererClient : public ContentRendererClient {
 public:
  ShellContentRendererClient();
  virtual ~ShellContentRendererClient();

  // ContentRendererClient implementation.
  virtual void RenderThreadStarted() OVERRIDE;
  virtual void RenderFrameCreated(RenderFrame* render_frame) OVERRIDE;
  virtual void RenderViewCreated(RenderView* render_view) OVERRIDE;
  virtual bool OverrideCreatePlugin(
      RenderFrame* render_frame,
      blink::WebLocalFrame* frame,
      const blink::WebPluginParams& params,
      blink::WebPlugin** plugin) OVERRIDE;
  virtual blink::WebMediaStreamCenter* OverrideCreateWebMediaStreamCenter(
      blink::WebMediaStreamCenterClient* client) OVERRIDE;
  virtual blink::WebRTCPeerConnectionHandler*
  OverrideCreateWebRTCPeerConnectionHandler(
      blink::WebRTCPeerConnectionHandlerClient* client) OVERRIDE;
  virtual blink::WebMIDIAccessor* OverrideCreateMIDIAccessor(
      blink::WebMIDIAccessorClient* client) OVERRIDE;
  virtual blink::WebAudioDevice* OverrideCreateAudioDevice(
      double sample_rate) OVERRIDE;
  virtual blink::WebClipboard* OverrideWebClipboard() OVERRIDE;
  virtual blink::WebThemeEngine* OverrideThemeEngine() OVERRIDE;
  virtual bool IsPluginAllowedToUseCompositorAPI(const GURL& url) OVERRIDE;
  virtual bool IsPluginAllowedToUseVideoDecodeAPI(const GURL& url) OVERRIDE;
  virtual bool IsPluginAllowedToUseDevChannelAPIs() OVERRIDE;

 private:
  void WebTestProxyCreated(RenderView* render_view, WebTestProxyBase* proxy);

  scoped_ptr<web_cache::WebCacheRenderProcessObserver> web_cache_observer_;
  scoped_ptr<ShellRenderProcessObserver> shell_observer_;
  scoped_ptr<MockWebClipboardImpl> clipboard_;
};

}  // namespace content

#endif  // CONTENT_SHELL_SHELL_CONTENT_RENDERER_CLIENT_H_
