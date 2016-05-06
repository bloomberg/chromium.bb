// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/shell_content_renderer_client.h"

#include "base/command_line.h"
#include "components/web_cache/renderer/web_cache_impl.h"
#include "content/shell/renderer/shell_render_view_observer.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "v8/include/v8.h"

#if defined(ENABLE_PLUGINS)
#include "ppapi/shared_impl/ppapi_switches.h"
#endif

namespace content {

ShellContentRendererClient::ShellContentRendererClient() {}

ShellContentRendererClient::~ShellContentRendererClient() {
}

void ShellContentRendererClient::RenderThreadStarted() {
  web_cache_impl_.reset(new web_cache::WebCacheImpl());
}

void ShellContentRendererClient::RenderViewCreated(RenderView* render_view) {
  new ShellRenderViewObserver(render_view);
}

bool ShellContentRendererClient::IsPluginAllowedToUseCompositorAPI(
    const GURL& url) {
#if defined(ENABLE_PLUGINS)
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnablePepperTesting);
#else
  return false;
#endif
}

bool ShellContentRendererClient::IsPluginAllowedToUseDevChannelAPIs() {
#if defined(ENABLE_PLUGINS)
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnablePepperTesting);
#else
  return false;
#endif
}

}  // namespace content
