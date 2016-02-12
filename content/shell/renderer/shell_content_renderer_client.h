// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_SHELL_CONTENT_RENDERER_CLIENT_H_
#define CONTENT_SHELL_RENDERER_SHELL_CONTENT_RENDERER_CLIENT_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/renderer/content_renderer_client.h"

namespace web_cache {
class WebCacheRenderProcessObserver;
}

namespace content {

class ShellContentRendererClient : public ContentRendererClient {
 public:
  ShellContentRendererClient();
  ~ShellContentRendererClient() override;

  // ContentRendererClient implementation.
  void RenderThreadStarted() override;
  void RenderViewCreated(RenderView* render_view) override;

  // TODO(mkwst): These toggle based on the kEnablePepperTesting flag. Do we
  // need that outside of layout tests?
  bool IsPluginAllowedToUseCompositorAPI(const GURL& url) override;
  bool IsPluginAllowedToUseDevChannelAPIs() override;
  base::StringPiece GetOriginTrialPublicKey() override;

 private:
  scoped_ptr<web_cache::WebCacheRenderProcessObserver> web_cache_observer_;
  base::StringPiece origin_trial_public_key_;
};

}  // namespace content

#endif  // CONTENT_SHELL_RENDERER_SHELL_CONTENT_RENDERER_CLIENT_H_
