// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_render_view_host_observer.h"

#include "chrome/browser/prerender/prerender_contents.h"
#include "chrome/common/icon_messages.h"
#include "chrome/common/render_messages.h"

namespace prerender {

PrerenderRenderViewHostObserver::PrerenderRenderViewHostObserver(
    PrerenderContents* prerender_contents,
    RenderViewHost* render_view_host)
    : RenderViewHostObserver(render_view_host),
      prerender_contents_(prerender_contents) {
}

void PrerenderRenderViewHostObserver::RenderViewHostDestroyed(
    RenderViewHost* rvh) {
  // The base class deletes |this| on RenderViewHost destruction but we want the
  // lifetime to be tied to the PrerenderContents instead, so we'll do nothing
  // here.
}

bool PrerenderRenderViewHostObserver::OnMessageReceived(
    const IPC::Message& message) {
  if (!prerender_contents_)
    return RenderViewHostObserver::OnMessageReceived(message);

  bool handled = true;
  // The following messages we do want to consume.
  IPC_BEGIN_MESSAGE_MAP(PrerenderRenderViewHostObserver, message)
    IPC_MESSAGE_HANDLER(IconHostMsg_UpdateFaviconURL, OnUpdateFaviconURL)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_MaybeCancelPrerenderForHTML5Media,
                        OnMaybeCancelPrerenderForHTML5Media)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_CancelPrerenderForPrinting,
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
