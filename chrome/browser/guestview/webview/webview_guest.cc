// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/guestview/webview/webview_guest.h"

#include "base/debug/stack_trace.h"
#include "chrome/browser/extensions/api/web_request/web_request_api.h"
#include "chrome/browser/extensions/extension_renderer_state.h"
#include "chrome/browser/extensions/script_executor.h"
#include "chrome/browser/guestview/guestview_constants.h"
#include "chrome/browser/guestview/webview/webview_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"

using content::WebContents;

namespace {

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

WebViewGuest::WebViewGuest(WebContents* guest_web_contents)
    : GuestView(guest_web_contents),
      WebContentsObserver(guest_web_contents),
      script_executor_(new extensions::ScriptExecutor(guest_web_contents,
                                                      &script_observers_)) {
}

// static
WebViewGuest* WebViewGuest::From(int embedder_process_id,
                                 int guest_instance_id) {
  GuestView* guest = GuestView::From(embedder_process_id, guest_instance_id);
  if (!guest)
    return NULL;
  return guest->AsWebView();
}

void WebViewGuest::Attach(WebContents* embedder_web_contents,
                          const std::string& extension_id,
                          int view_instance_id,
                          const base::DictionaryValue& args) {
  GuestView::Attach(
      embedder_web_contents, extension_id, view_instance_id, args);

  AddWebViewToExtensionRendererState();
}

GuestView::Type WebViewGuest::GetViewType() const {
  return GuestView::WEBVIEW;
}

WebViewGuest* WebViewGuest::AsWebView() {
  return this;
}

AdViewGuest* WebViewGuest::AsAdView() {
  return NULL;
}

void WebViewGuest::Go(int relative_index) {
  guest_web_contents()->GetController().GoToOffset(relative_index);
}

WebViewGuest::~WebViewGuest() {
}

void WebViewGuest::DidCommitProvisionalLoadForFrame(
    int64 frame_id,
    bool is_main_frame,
    const GURL& url,
    content::PageTransition transition_type,
    content::RenderViewHost* render_view_host) {
  scoped_ptr<DictionaryValue> args(new DictionaryValue());
  args->SetString(guestview::kUrl, url.spec());
  args->SetBoolean(guestview::kIsTopLevel, is_main_frame);
  args->SetInteger(guestview::kInternalCurrentEntryIndex,
      web_contents()->GetController().GetCurrentEntryIndex());
  args->SetInteger(guestview::kInternalEntryCount,
      web_contents()->GetController().GetEntryCount());
  DispatchEvent(new GuestView::Event(webview::kEventLoadCommit, args.Pass()));
}

void WebViewGuest::DidStopLoading(content::RenderViewHost* render_view_host) {
  scoped_ptr<DictionaryValue> args(new DictionaryValue());
  DispatchEvent(new GuestView::Event(webview::kEventLoadStop, args.Pass()));
}

void WebViewGuest::WebContentsDestroyed(WebContents* web_contents) {
  RemoveWebViewFromExtensionRendererState(web_contents);
  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(
          &RemoveWebViewEventListenersOnIOThread,
          browser_context(), extension_id(),
          embedder_render_process_id(),
          view_instance_id()));
  delete this;
}

void WebViewGuest::AddWebViewToExtensionRendererState() {
  ExtensionRendererState::WebViewInfo webview_info;
  webview_info.embedder_process_id = embedder_render_process_id();
  webview_info.embedder_routing_id = embedder_web_contents()->GetRoutingID();
  webview_info.guest_instance_id = guest_instance_id();
  webview_info.instance_id = view_instance_id();

  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(
          &ExtensionRendererState::AddWebView,
          base::Unretained(ExtensionRendererState::GetInstance()),
          guest_web_contents()->GetRenderProcessHost()->GetID(),
          guest_web_contents()->GetRoutingID(),
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
