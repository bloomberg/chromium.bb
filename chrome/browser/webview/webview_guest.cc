// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webview/webview_guest.h"

#include "base/lazy_instance.h"
#include "chrome/browser/extensions/api/web_request/web_request_api.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"

using content::WebContents;

namespace chrome {

namespace {

typedef std::map<int, WebViewGuest*> WebViewGuestMap;
typedef std::map<void*, WebViewGuestMap> WebViewProfileMap;
static base::LazyInstance<WebViewProfileMap> webview_profile_map =
    LAZY_INSTANCE_INITIALIZER;

void RemoveWebViewEventListenersOnIOThread(
    void* profile,
    const std::string& extension_id,
    int embedder_process_id,
    int web_view_instance_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  ExtensionWebRequestEventRouter::GetInstance()->RemoveWebViewEventListeners(
      profile, extension_id, embedder_process_id, web_view_instance_id);
}

}  // namespace

WebViewGuest::WebViewGuest(WebContents* guest_web_contents,
                           WebContents* embedder_web_contents,
                           const std::string& extension_id)
    : WebContentsObserver(guest_web_contents),
      embedder_web_contents_(embedder_web_contents),
      extension_id_(extension_id),
      embedder_render_process_id_(
          embedder_web_contents->GetRenderProcessHost()->GetID()),
      profile_(guest_web_contents->GetBrowserContext()),
      instance_id_(guest_web_contents->GetEmbeddedInstanceID()) {
  webview_profile_map.Get()[profile_].insert(
      std::make_pair(instance_id_, this));
}

// static
WebViewGuest* WebViewGuest::From(void* profile, int instance_id) {
  WebViewProfileMap* profile_map = webview_profile_map.Pointer();
  WebViewProfileMap::iterator it = profile_map->find(profile);
  if (it == profile_map->end())
    return NULL;
  const WebViewGuestMap& guest_map = it->second;
  WebViewGuestMap::const_iterator guest_it = guest_map.find(instance_id);
  return guest_it == guest_map.end() ? NULL : guest_it->second;
}

WebViewGuest::~WebViewGuest() {
  webview_profile_map.Get()[profile_].erase(instance_id_);
  if (webview_profile_map.Get()[profile_].empty())
    webview_profile_map.Get().erase(profile_);
}

void WebViewGuest::WebContentsDestroyed(WebContents* web_contents) {
  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(
          &RemoveWebViewEventListenersOnIOThread,
          profile_, extension_id_,
          embedder_render_process_id_,
          instance_id_));
  delete this;
}

}  // namespace chrome
