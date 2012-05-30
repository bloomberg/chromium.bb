// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/renderer/content_renderer_client.h"

namespace content {

SkBitmap* ContentRendererClient::GetSadPluginBitmap() {
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
    const FilePath& plugin_path) {
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
        media::FilterCollection* collection,
        WebKit::WebAudioSourceProvider* audio_source_provider,
        media::MessageLoopFactory* message_loop_factory,
        webkit_media::MediaStreamClient* media_stream_client,
        media::MediaLog* media_log) {
  return NULL;
}

bool ContentRendererClient::RunIdleHandlerWhenWidgetsHidden() {
  return true;
}

bool ContentRendererClient::AllowPopup(const GURL& creator) {
  return false;
}

bool ContentRendererClient::ShouldFork(WebKit::WebFrame* frame,
                                       const GURL& url,
                                       bool is_initial_navigation,
                                       bool* send_referrer) {
  return false;
}

bool ContentRendererClient::WillSendRequest(WebKit::WebFrame* frame,
                                            const GURL& url,
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

}  // namespace content
