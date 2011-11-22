// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/mock_content_renderer_client.h"

#include <string>
#include "v8/include/v8.h"

namespace content {

MockContentRendererClient::~MockContentRendererClient() {
}

void MockContentRendererClient::RenderThreadStarted() {
}

void MockContentRendererClient::RenderViewCreated(RenderView* render_view) {
}

void MockContentRendererClient::SetNumberOfViews(int number_of_views) {
}

SkBitmap* MockContentRendererClient::GetSadPluginBitmap() {
  return NULL;
}

std::string MockContentRendererClient::GetDefaultEncoding() {
  return std::string();
}

bool MockContentRendererClient::OverrideCreatePlugin(
    RenderView* render_view,
    WebKit::WebFrame* frame,
    const WebKit::WebPluginParams& params,
    WebKit::WebPlugin** plugin) {
  return false;
}

bool MockContentRendererClient::HasErrorPage(int http_status_code,
                                             std::string* error_domain) {
  return false;
}

void MockContentRendererClient::GetNavigationErrorStrings(
    const WebKit::WebURLRequest& failed_request,
    const WebKit::WebURLError& error,
    std::string* error_html,
    string16* error_description) {
}

bool MockContentRendererClient::RunIdleHandlerWhenWidgetsHidden() {
  return true;
}

bool MockContentRendererClient::AllowPopup(const GURL& creator) {
  return false;
}

bool MockContentRendererClient::ShouldFork(WebKit::WebFrame* frame,
                                           const GURL& url,
                                           bool is_content_initiated,
                                           bool is_initial_navigation,
                                           bool* send_referrer) {
  return false;
}

bool MockContentRendererClient::WillSendRequest(WebKit::WebFrame* frame,
                                                const GURL& url,
                                                GURL* new_url) {
  return false;
}

bool MockContentRendererClient::ShouldPumpEventsDuringCookieMessage() {
  return false;
}

void MockContentRendererClient::DidCreateScriptContext(
    WebKit::WebFrame* frame, v8::Handle<v8::Context> context, int world_id) {
}

void MockContentRendererClient::WillReleaseScriptContext(
    WebKit::WebFrame* frame, v8::Handle<v8::Context> context, int world_id) {
}

unsigned long long MockContentRendererClient::VisitedLinkHash(
    const char* canonical_url, size_t length) {
  return 0LL;
}

bool MockContentRendererClient::IsLinkVisited(unsigned long long link_hash) {
  return false;
}

void MockContentRendererClient::PrefetchHostName(
    const char* hostname, size_t length) {
}

bool MockContentRendererClient::ShouldOverridePageVisibilityState(
    const RenderView* render_view,
    WebKit::WebPageVisibilityState* override_state) const {
  return false;
}

bool MockContentRendererClient::HandleGetCookieRequest(
    RenderView* sender,
    const GURL& url,
    const GURL& first_party_for_cookies,
    std::string* cookies) {
  return false;
}

bool MockContentRendererClient::HandleSetCookieRequest(
    RenderView* sender,
    const GURL& url,
    const GURL& first_party_for_cookies,
    const std::string& value) {
  return false;
}

void MockContentRendererClient::RegisterPPAPIInterfaceFactories(
    webkit::ppapi::PpapiInterfaceFactoryManager* factory_manager) {
}

}  // namespace content
