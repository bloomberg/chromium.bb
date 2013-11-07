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
    blink::WebFrame* frame,
    const blink::WebPluginParams& params,
    blink::WebPlugin** plugin) {
  return false;
}

blink::WebPlugin* ContentRendererClient::CreatePluginReplacement(
    RenderView* render_view,
    const base::FilePath& plugin_path) {
  return NULL;
}

bool ContentRendererClient::HasErrorPage(int http_status_code,
                                         std::string* error_domain) {
  return false;
}

bool ContentRendererClient::ShouldSuppressErrorPage(const GURL& url) {
  return false;
}

void ContentRendererClient::DeferMediaLoad(RenderView* render_view,
                                           const base::Closure& closure) {
  closure.Run();
}

blink::WebMediaStreamCenter*
ContentRendererClient::OverrideCreateWebMediaStreamCenter(
    blink::WebMediaStreamCenterClient* client) {
  return NULL;
}

blink::WebRTCPeerConnectionHandler*
ContentRendererClient::OverrideCreateWebRTCPeerConnectionHandler(
    blink::WebRTCPeerConnectionHandlerClient* client) {
  return NULL;
}

blink::WebMIDIAccessor*
ContentRendererClient::OverrideCreateMIDIAccessor(
    blink::WebMIDIAccessorClient* client) {
  return NULL;
}

blink::WebAudioDevice*
ContentRendererClient::OverrideCreateAudioDevice(
    double sample_rate) {
  return NULL;
}

blink::WebClipboard* ContentRendererClient::OverrideWebClipboard() {
  return NULL;
}

blink::WebThemeEngine* ContentRendererClient::OverrideThemeEngine() {
  return NULL;
}

blink::WebSpeechSynthesizer* ContentRendererClient::OverrideSpeechSynthesizer(
    blink::WebSpeechSynthesizerClient* client) {
  return NULL;
}

bool ContentRendererClient::RunIdleHandlerWhenWidgetsHidden() {
  return true;
}

bool ContentRendererClient::AllowPopup() {
  return false;
}

bool ContentRendererClient::HandleNavigation(
    blink::WebFrame* frame,
    const blink::WebURLRequest& request,
    blink::WebNavigationType type,
    blink::WebNavigationPolicy default_policy,
    bool is_redirect) {
  return false;
}

bool ContentRendererClient::ShouldFork(blink::WebFrame* frame,
                                       const GURL& url,
                                       const std::string& http_method,
                                       bool is_initial_navigation,
                                       bool is_server_redirect,
                                       bool* send_referrer) {
  return false;
}

bool ContentRendererClient::WillSendRequest(
    blink::WebFrame* frame,
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

blink::WebPrescientNetworking*
ContentRendererClient::GetPrescientNetworking() {
  return NULL;
}

bool ContentRendererClient::ShouldOverridePageVisibilityState(
    const RenderView* render_view,
    blink::WebPageVisibilityState* override_state) {
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

const void* ContentRendererClient::CreatePPAPIInterface(
    const std::string& interface_name) {
  return NULL;
}

bool ContentRendererClient::IsExternalPepperPlugin(
    const std::string& module_name) {
  return false;
}

bool ContentRendererClient::IsPluginAllowedToCallRequestOSFileHandle(
    blink::WebPluginContainer* container) {
  return false;
}

bool ContentRendererClient::AllowBrowserPlugin(
    blink::WebPluginContainer* container) {
  return false;
}

bool ContentRendererClient::AllowPepperMediaStreamAPI(const GURL& url) {
  return false;
}

void ContentRendererClient::AddKeySystems(
    std::vector<KeySystemInfo>* key_systems) {
}

bool ContentRendererClient::ShouldReportDetailedMessageForSource(
    const base::string16& source) const {
  return false;
}

bool ContentRendererClient::ShouldEnableSiteIsolationPolicy() const {
  return true;
}

blink::WebWorkerPermissionClientProxy*
ContentRendererClient::CreateWorkerPermissionClientProxy(
    RenderView* render_view, blink::WebFrame* frame) {
  return NULL;
}

}  // namespace content
