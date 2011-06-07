// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/content_renderer_client.h"

#include "base/file_path.h"
#include "content/renderer/render_view.h"

using WebKit::WebFrame;

namespace content {
void ContentRendererClient::RenderThreadStarted() {
}

void ContentRendererClient::RenderViewCreated(RenderView* render_view) {
}

void ContentRendererClient::SetNumberOfViews(int number_of_views) {
}

SkBitmap* ContentRendererClient::GetSadPluginBitmap() {
  return NULL;
}

std::string ContentRendererClient::GetDefaultEncoding() {
  return std::string();
}

WebKit::WebPlugin* ContentRendererClient::CreatePlugin(RenderView* render_view,
                                                       WebFrame* frame,
    const WebKit::WebPluginParams& params) {
  return render_view->CreatePluginNoCheck(frame, params);
}

void ContentRendererClient::ShowErrorPage(RenderView* render_view,
                                          WebKit::WebFrame* frame,
                                          int http_status_code) {
}

std::string ContentRendererClient::GetNavigationErrorHtml(
    const WebKit::WebURLRequest& failed_request,
    const WebKit::WebURLError& error) {
  return std::string();
}

bool ContentRendererClient::RunIdleHandlerWhenWidgetsHidden() {
  return true;
}

bool ContentRendererClient::AllowPopup(const GURL& creator) {
  return false;
}

bool ContentRendererClient::ShouldFork(WebFrame* frame,
                                       const GURL& url,
                                       bool is_content_initiated,
                                       bool* send_referrer) {
  return false;
}

bool ContentRendererClient::WillSendRequest(WebFrame* frame,
                                            const GURL& url,
                                            GURL* new_url) {
  return false;
}

FilePath ContentRendererClient::GetMediaLibraryPath() {
  return FilePath();
}

bool ContentRendererClient::ShouldPumpEventsDuringCookieMessage() {
  return false;
}

void ContentRendererClient::DidCreateScriptContext(WebFrame* frame) {
}

void ContentRendererClient::DidDestroyScriptContext(WebFrame* frame) {
}

void ContentRendererClient::DidCreateIsolatedScriptContext(WebFrame* frame) {
}

unsigned long long ContentRendererClient::VisitedLinkHash(
    const char* canonical_url, size_t length) {
  return 0;
}

bool ContentRendererClient::IsLinkVisited(unsigned long long link_hash) {
  return false;
}

void ContentRendererClient::PrefetchHostName(const char* hostname,
                                             size_t length) {
}

bool ContentRendererClient::ShouldOverridePageVisibilityState(
    const RenderView* render_view,
    WebKit::WebPageVisibilityState* override_state) const {
  return false;
}

}  // namespace content
