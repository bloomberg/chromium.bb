// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/content_renderer_client.h"

#include "content/renderer/render_view.h"

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
  return render_view->CreatePluginNoCheck(frame, params);
}

std::string ContentRendererClient::GetNavigationErrorHtml(
    const WebKit::WebURLRequest& failed_request,
    const WebKit::WebURLError& error) {
  return std::string();
}

std::string ContentRendererClient::DetermineTextLanguage(const string16& text) {
  return std::string();
}

bool ContentRendererClient::RunIdleHandlerWhenWidgetsHidden() {
  return true;
}

bool ContentRendererClient::AllowPopup(const GURL& creator) {
  return false;
}

}  // namespace content
