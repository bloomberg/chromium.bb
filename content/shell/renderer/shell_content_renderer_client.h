// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_SHELL_CONTENT_RENDERER_CLIENT_H_
#define CONTENT_SHELL_SHELL_CONTENT_RENDERER_CLIENT_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/renderer/content_renderer_client.h"

namespace web_cache {
class WebCacheRenderProcessObserver;
}

namespace content {

class ShellRenderProcessObserver;
class WebTestProxyBase;

class ShellContentRendererClient : public ContentRendererClient {
 public:
  ShellContentRendererClient();
  virtual ~ShellContentRendererClient();

  // ContentRendererClient implementation.
  virtual void RenderThreadStarted() override;
  virtual void RenderViewCreated(RenderView* render_view) override;

  // TODO(mkwst): These toggle based on the kEnablePepperTesting flag. Do we
  // need that outside of layout tests?
  virtual bool IsPluginAllowedToUseCompositorAPI(const GURL& url) override;
  virtual bool IsPluginAllowedToUseVideoDecodeAPI(const GURL& url) override;
  virtual bool IsPluginAllowedToUseDevChannelAPIs() override;

 private:
  scoped_ptr<web_cache::WebCacheRenderProcessObserver> web_cache_observer_;
  scoped_ptr<ShellRenderProcessObserver> shell_observer_;
};

}  // namespace content

#endif  // CONTENT_SHELL_SHELL_CONTENT_RENDERER_CLIENT_H_
