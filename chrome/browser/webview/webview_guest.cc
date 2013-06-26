// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webview/webview_guest.h"

#include "base/lazy_instance.h"
#include "chrome/browser/extensions/api/web_request/web_request_api.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_renderer_state.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/script_executor.h"
#include "chrome/browser/webview/webview_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"

using content::WebContents;

namespace {

typedef std::map<std::pair<int, int>, WebViewGuest*> WebViewGuestMap;
base::LazyInstance<WebViewGuestMap> webview_guest_map =
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

void WebViewGuest::Go(int relative_index) {
  web_contents()->GetController().GoToOffset(relative_index);
}

WebViewGuest::~WebViewGuest() {
  std::pair<int, int> key(embedder_render_process_id_, guest_instance_id_);
  webview_guest_map.Get().erase(key);
}

void WebViewGuest::DispatchEvent(const std::string& event_name,
                                 scoped_ptr<DictionaryValue> event) {
  Profile* profile = Profile::FromBrowserContext(
      web_contents()->GetBrowserContext());

  extensions::EventFilteringInfo info;
  info.SetURL(GURL());
  info.SetInstanceID(guest_instance_id_);
  scoped_ptr<ListValue> args(new ListValue());
  args->Append(event.release());

  extensions::EventRouter::DispatchEvent(
      embedder_web_contents_, profile, extension_id_,
      event_name, args.Pass(),
      extensions::EventRouter::USER_GESTURE_UNKNOWN, info);
}

void WebViewGuest::DidCommitProvisionalLoadForFrame(
    int64 frame_id,
    bool is_main_frame,
    const GURL& url,
    content::PageTransition transition_type,
    content::RenderViewHost* render_view_host) {
  scoped_ptr<DictionaryValue> event(new DictionaryValue());
  event->SetString(webview::kUrl, url.spec());
  event->SetBoolean(webview::kIsTopLevel, is_main_frame);
  event->SetInteger(webview::kInternalCurrentEntryIndex,
      web_contents()->GetController().GetCurrentEntryIndex());
  event->SetInteger(webview::kInternalEntryCount,
      web_contents()->GetController().GetEntryCount());
  DispatchEvent(webview::kEventLoadCommit, event.Pass());
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
