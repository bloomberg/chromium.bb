// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/guestview/adview/adview_guest.h"

#include "chrome/browser/guestview/adview/adview_constants.h"
#include "chrome/browser/guestview/guestview_constants.h"
#include "content/public/browser/web_contents.h"

using content::WebContents;

AdViewGuest::AdViewGuest(WebContents* guest_web_contents,
                         WebContents* embedder_web_contents,
                         const std::string& extension_id,
                         int adview_instance_id,
                         const base::DictionaryValue& args)
    : GuestView(guest_web_contents,
                embedder_web_contents,
                extension_id,
                adview_instance_id,
                args),
      WebContentsObserver(guest_web_contents) {
}

// static
AdViewGuest* AdViewGuest::From(int embedder_process_id,
                               int guest_instance_id) {
  GuestView* guest = GuestView::From(embedder_process_id, guest_instance_id);
  if (!guest)
    return NULL;
  return guest->AsAdView();
}

WebContents* AdViewGuest::GetWebContents() const {
  return WebContentsObserver::web_contents();
}

WebViewGuest* AdViewGuest::AsWebView() {
  return NULL;
}

AdViewGuest* AdViewGuest::AsAdView() {
  return this;
}

AdViewGuest::~AdViewGuest() {
}

void AdViewGuest::DidCommitProvisionalLoadForFrame(
    int64 frame_id,
    bool is_main_frame,
    const GURL& url,
    content::PageTransition transition_type,
    content::RenderViewHost* render_view_host) {
  scoped_ptr<DictionaryValue> event(new DictionaryValue());
  event->SetString(guestview::kUrl, url.spec());
  event->SetBoolean(guestview::kIsTopLevel, is_main_frame);
  DispatchEvent(adview::kEventLoadCommit, event.Pass());
}

void AdViewGuest::WebContentsDestroyed(WebContents* web_contents) {
  delete this;
}
