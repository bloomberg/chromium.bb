// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/renderer/content_renderer_client.h"

namespace content {

SkBitmap* ContentRendererClient::GetSadPluginBitmap() {
  return NULL;
}

SkBitmap* ContentRendererClient::GetSadWebViewBitmap() {
  return NULL;
}

std::string ContentRendererClient::GetDefaultEncoding() {
  return std::string();
}

bool ContentRendererClient::OverrideCreatePlugin(
    RenderView* render_view,
    WebKit::WebFrame* frame,
    const WebKit::WebPluginParams& params,
    WebKit::WebPlugin** plugin) {
  return false;
}

WebKit::WebPlugin* ContentRendererClient::CreatePluginReplacement(
    RenderView* render_view,
    const base::FilePath& plugin_path) {
  return NULL;
}

bool ContentRendererClient::HasErrorPage(int http_status_code,
                                         std::string* error_domain) {
  return false;
}

webkit_media::WebMediaPlayerImpl*
ContentRendererClient::OverrideCreateWebMediaPlayer(
    RenderView* render_view,
    WebKit::WebFrame* frame,
    WebKit::WebMediaPlayerClient* client,
    base::WeakPtr<webkit_media::WebMediaPlayerDelegate> delegate,
    const webkit_media::WebMediaPlayerParams& params) {
  return NULL;
}

WebKit::WebMediaStreamCenter*
ContentRendererClient::OverrideCreateWebMediaStreamCenter(
    WebKit::WebMediaStreamCenterClient* client) {
  return NULL;
}

WebKit::WebRTCPeerConnectionHandler*
ContentRendererClient::OverrideCreateWebRTCPeerConnectionHandler(
    WebKit::WebRTCPeerConnectionHandlerClient* client) {
  return NULL;
}

WebKit::WebClipboard* ContentRendererClient::OverrideWebClipboard() {
  return NULL;
}

WebKit::WebMimeRegistry* ContentRendererClient::OverrideWebMimeRegistry() {
  return NULL;
}

WebKit::WebHyphenator* ContentRendererClient::OverrideWebHyphenator() {
  return NULL;
}

WebKit::WebThemeEngine* ContentRendererClient::OverrideThemeEngine() {
  return NULL;
}

bool ContentRendererClient::RunIdleHandlerWhenWidgetsHidden() {
  return true;
}

bool ContentRendererClient::AllowPopup() {
  return false;
}

bool ContentRendererClient::HandleNavigation(
    WebKit::WebFrame* frame,
    const WebKit::WebURLRequest& request,
    WebKit::WebNavigationType type,
    WebKit::WebNavigationPolicy default_policy,
    bool is_redirect) {
  return false;
}

bool ContentRendererClient::ShouldFork(WebKit::WebFrame* frame,
                                       const GURL& url,
                                       const std::string& http_method,
                                       bool is_initial_navigation,
                                       bool* send_referrer) {
  return false;
}

bool ContentRendererClient::WillSendRequest(
    WebKit::WebFrame* frame,
    PageTransition transition_type,
    const GURL& url,
    const GURL& first_party_for_cookies,
    GURL* new_url) {
  return false;
}

bool ContentRendererClient::ShouldPumpEventsDuringCookieMessage() {
  return false;
}

unsigned long long ContentRendererClient::VisitedLinkHash(
    const char* canonical_url, size_t length) {
  return 0LL;
}

bool ContentRendererClient::IsLinkVisited(unsigned long long link_hash) {
  return false;
}

bool ContentRendererClient::ShouldOverridePageVisibilityState(
    const RenderView* render_view,
    WebKit::WebPageVisibilityState* override_state) const {
  return false;
}

bool ContentRendererClient::HandleGetCookieRequest(
    RenderView* sender,
    const GURL& url,
    const GURL& first_party_for_cookies,
    std::string* cookies) {
  return false;
}

bool ContentRendererClient::HandleSetCookieRequest(
    RenderView* sender,
    const GURL& url,
    const GURL& first_party_for_cookies,
    const std::string& value) {
  return false;
}

bool ContentRendererClient::AllowBrowserPlugin(
    WebKit::WebPluginContainer* container) const {
  return false;
}

MessageLoop* ContentRendererClient::OverrideCompositorMessageLoop() const {
  return NULL;
}

bool ContentRendererClient::ShouldCreateCompositorInputHandler() const {
  return true;
}

bool ContentRendererClient::IsRequestOSFileHandleAllowedForURL(
    const GURL& url) const {
  return false;
}

}  // namespace content
