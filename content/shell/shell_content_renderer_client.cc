// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/shell_content_renderer_client.h"

#include "v8/include/v8.h"

namespace content {

ShellContentRendererClient::~ShellContentRendererClient() {
}

void ShellContentRendererClient::RenderThreadStarted() {
}

void ShellContentRendererClient::RenderViewCreated(RenderView* render_view) {
}

void ShellContentRendererClient::SetNumberOfViews(int number_of_views) {
}

SkBitmap* ShellContentRendererClient::GetSadPluginBitmap() {
  return NULL;
}

std::string ShellContentRendererClient::GetDefaultEncoding() {
  return std::string();
}

bool ShellContentRendererClient::OverrideCreatePlugin(
    RenderView* render_view,
    WebKit::WebFrame* frame,
    const WebKit::WebPluginParams& params,
    WebKit::WebPlugin** plugin) {
  return false;
}

bool ShellContentRendererClient::HasErrorPage(int http_status_code,
                                              std::string* error_domain) {
  return false;
}

void ShellContentRendererClient::GetNavigationErrorStrings(
    const WebKit::WebURLRequest& failed_request,
    const WebKit::WebURLError& error,
    std::string* error_html,
    string16* error_description) {
}

bool ShellContentRendererClient::RunIdleHandlerWhenWidgetsHidden() {
  return true;
}

bool ShellContentRendererClient::AllowPopup(const GURL& creator) {
  return false;
}

bool ShellContentRendererClient::ShouldFork(WebKit::WebFrame* frame,
                                            const GURL& url,
                                            bool is_content_initiated,
                                            bool is_initial_navigation,
                                            bool* send_referrer) {
  return false;
}

bool ShellContentRendererClient::WillSendRequest(WebKit::WebFrame* frame,
                                                 const GURL& url,
                                                 GURL* new_url) {
  return false;
}

bool ShellContentRendererClient::ShouldPumpEventsDuringCookieMessage() {
  return false;
}

void ShellContentRendererClient::DidCreateScriptContext(
    WebKit::WebFrame* frame, v8::Handle<v8::Context> context, int world_id) {
}

void ShellContentRendererClient::WillReleaseScriptContext(
    WebKit::WebFrame* frame, v8::Handle<v8::Context> context, int world_id) {
}

unsigned long long ShellContentRendererClient::VisitedLinkHash(
    const char* canonical_url, size_t length) {
  return 0LL;
}

bool ShellContentRendererClient::IsLinkVisited(unsigned long long link_hash) {
  return false;
}

void ShellContentRendererClient::PrefetchHostName(
    const char* hostname, size_t length) {
}

bool ShellContentRendererClient::ShouldOverridePageVisibilityState(
    const RenderView* render_view,
    WebKit::WebPageVisibilityState* override_state) const {
  return false;
}

bool ShellContentRendererClient::HandleGetCookieRequest(
    RenderView* sender,
    const GURL& url,
    const GURL& first_party_for_cookies,
    std::string* cookies) {
  return false;
}

bool ShellContentRendererClient::HandleSetCookieRequest(
    RenderView* sender,
    const GURL& url,
    const GURL& first_party_for_cookies,
    const std::string& value) {
  return false;
}

void ShellContentRendererClient::RegisterPPAPIInterfaceFactories(
    webkit::ppapi::PpapiInterfaceFactoryManager* factory_manager) {
}

}  // namespace content
