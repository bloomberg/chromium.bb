// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_render_view_host_observer.h"

#include "chrome/browser/prerender/prerender_contents.h"
#include "chrome/common/icon_messages.h"
#include "chrome/common/render_messages.h"
#include "content/common/view_messages.h"
#include "ipc/ipc_message_macros.h"

namespace prerender {

PrerenderRenderViewHostObserver::PrerenderRenderViewHostObserver(
    PrerenderContents* prerender_contents,
    RenderViewHost* render_view_host)
    : RenderViewHostObserver(render_view_host),
      prerender_contents_(prerender_contents) {
}

void PrerenderRenderViewHostObserver::RenderViewHostDestroyed() {
  // The base class deletes |this| on RenderViewHost destruction but we want the
  // lifetime to be tied to the PrerenderContents instead, so we'll do nothing
  // here.
}

bool PrerenderRenderViewHostObserver::OnMessageReceived(
    const IPC::Message& message) {
  if (!prerender_contents_)
    return RenderViewHostObserver::OnMessageReceived(message);

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PrerenderRenderViewHostObserver, message)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidStartProvisionalLoadForFrame,
                        OnDidStartProvisionalLoadForFrame)
    IPC_MESSAGE_HANDLER(IconHostMsg_UpdateFaviconURL, OnUpdateFaviconURL)
    IPC_MESSAGE_HANDLER(ViewHostMsg_MaybeCancelPrerenderForHTML5Media,
                        OnMaybeCancelPrerenderForHTML5Media)
    IPC_MESSAGE_HANDLER(ViewHostMsg_JSOutOfMemory, OnJSOutOfMemory)
    IPC_MESSAGE_HANDLER(ViewHostMsg_RunJavaScriptMessage,
                        OnRunJavaScriptMessage)
    IPC_MESSAGE_HANDLER(ViewHostMsg_RenderViewGone, OnRenderViewGone)
    IPC_MESSAGE_HANDLER(ViewHostMsg_CancelPrerenderForPrinting,
                        OnCancelPrerenderForPrinting)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  // Pass the message through.
  if (!handled)
    handled = RenderViewHostObserver::OnMessageReceived(message);

  return handled;
}

// The base class declares this as protected so this is just here to make it
// public so it is visible to the caller.
bool PrerenderRenderViewHostObserver::Send(IPC::Message* message) {
  return RenderViewHostObserver::Send(message);
}

void PrerenderRenderViewHostObserver::OnJSOutOfMemory() {
  prerender_contents_->OnJSOutOfMemory();
}

void PrerenderRenderViewHostObserver::OnRunJavaScriptMessage(
    const std::wstring& message,
    const std::wstring& default_prompt,
    const GURL& frame_url,
    const int flags,
    bool* did_suppress_message,
    std::wstring* prompt_field) {
  prerender_contents_->OnRunJavaScriptMessage(message,
                                              default_prompt,
                                              frame_url,
                                              flags,
                                              did_suppress_message,
                                              prompt_field);
}

void PrerenderRenderViewHostObserver::OnRenderViewGone(int status,
                                                       int exit_code) {
  prerender_contents_->OnRenderViewGone(status, exit_code);
}

void PrerenderRenderViewHostObserver::OnDidStartProvisionalLoadForFrame(
    int64 frame_id,
    bool is_main_frame,
    const GURL& url) {
  prerender_contents_->OnDidStartProvisionalLoadForFrame(frame_id,
                                                         is_main_frame,
                                                         url);
}

void PrerenderRenderViewHostObserver::OnUpdateFaviconURL(
    int32 page_id,
    const std::vector<FaviconURL>& urls) {
  prerender_contents_->OnUpdateFaviconURL(page_id, urls);
}

void PrerenderRenderViewHostObserver::OnMaybeCancelPrerenderForHTML5Media() {
  prerender_contents_->Destroy(FINAL_STATUS_HTML5_MEDIA);
}

void PrerenderRenderViewHostObserver::OnCancelPrerenderForPrinting() {
  prerender_contents_->Destroy(FINAL_STATUS_WINDOW_PRINT);
}

}
