// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webview/webview_guest.h"

#include "base/lazy_instance.h"
#include "chrome/browser/extensions/api/web_request/web_request_api.h"
#include "chrome/browser/extensions/extension_renderer_state.h"
#include "chrome/browser/extensions/script_executor.h"
#include "chrome/browser/webview/webview_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"

using content::WebContents;

namespace keys = webview_constants;

namespace chrome {

namespace {

typedef std::map<std::pair<int, int>, WebViewGuest*> WebViewGuestMap;
static base::LazyInstance<WebViewGuestMap> webview_guest_map =
    LAZY_INSTANCE_INITIALIZER;

void RemoveWebViewEventListenersOnIOThread(
    void* profile,
    const std::string& extension_id,
    int embedder_process_id,
    int guest_instance_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  ExtensionWebRequestEventRouter::GetInstance()->RemoveWebViewEventListeners(
      profile, extension_id, embedder_process_id, guest_instance_id);
}

}  // namespace

WebViewGuest::WebViewGuest(WebContents* guest_web_contents,
                           WebContents* embedder_web_contents,
                           const std::string& extension_id,
                           int webview_instance_id,
                           const base::DictionaryValue& args)
    : WebContentsObserver(guest_web_contents),
      embedder_web_contents_(embedder_web_contents),
      extension_id_(extension_id),
      embedder_render_process_id_(
          embedder_web_contents->GetRenderProcessHost()->GetID()),
      profile_(guest_web_contents->GetBrowserContext()),
      guest_instance_id_(guest_web_contents->GetEmbeddedInstanceID()),
      webview_instance_id_(webview_instance_id),
      script_executor_(new extensions::ScriptExecutor(guest_web_contents,
                                                      &script_observers_)) {
  std::string api_name;
  DCHECK(args.GetString(keys::kAttributeApi, &api_name));
  DCHECK_EQ("webview", api_name);
  std::pair<int, int> key(embedder_render_process_id_, guest_instance_id_);
  webview_guest_map.Get().insert(std::make_pair(key, this));

  AddWebViewToExtensionRendererState();
}

// static
WebViewGuest* WebViewGuest::From(int embedder_process_id,
                                 int guest_instance_id) {
  WebViewGuestMap* guest_map = webview_guest_map.Pointer();
  WebViewGuestMap::iterator it = guest_map->find(
      std::make_pair(embedder_process_id, guest_instance_id));
  return it == guest_map->end() ? NULL : it->second;
}

WebViewGuest::~WebViewGuest() {
  std::pair<int, int> key(embedder_render_process_id_, guest_instance_id_);
  webview_guest_map.Get().erase(key);
}

void WebViewGuest::WebContentsDestroyed(WebContents* web_contents) {
  RemoveWebViewFromExtensionRendererState(web_contents);
  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(
          &RemoveWebViewEventListenersOnIOThread,
          profile_, extension_id_,
          embedder_render_process_id_,
          webview_instance_id_));
  delete this;
}

void WebViewGuest::AddWebViewToExtensionRendererState() {
  ExtensionRendererState::WebViewInfo webview_info;
  webview_info.embedder_process_id =
      embedder_web_contents()->GetRenderProcessHost()->GetID();
  webview_info.embedder_routing_id = embedder_web_contents()->GetRoutingID();
  webview_info.guest_instance_id = guest_instance_id_;
  webview_info.instance_id = webview_instance_id_;

  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(
          &ExtensionRendererState::AddWebView,
          base::Unretained(ExtensionRendererState::GetInstance()),
          web_contents()->GetRenderProcessHost()->GetID(),
          web_contents()->GetRoutingID(),
          webview_info));
}

// static
void WebViewGuest::RemoveWebViewFromExtensionRendererState(
    WebContents* web_contents) {
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(
          &ExtensionRendererState::RemoveWebView,
          base::Unretained(ExtensionRendererState::GetInstance()),
          web_contents->GetRenderProcessHost()->GetID(),
          web_contents->GetRoutingID()));
}

}  // namespace chrome
