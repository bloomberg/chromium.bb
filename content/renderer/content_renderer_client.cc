// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/content_renderer_client.h"

namespace content {

SkBitmap* ContentRendererClient::GetSadPluginBitmap() {
  return NULL;
}

std::string ContentRendererClient::GetDefaultEncoding() {
  return std::string();
}

WebKit::WebPlugin* ContentRendererClient::CreatePlugin(
    RenderView* render_view,
    WebKit::WebFrame* frame,
    const WebKit::WebPluginParams& params) {
  return NULL;
}

std::string ContentRendererClient::GetNavigationErrorHtml(
    const WebKit::WebURLRequest& failed_request,
    const WebKit::WebURLError& error) {
  return std::string();
}

}  // namespace content
