// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/guestview/adview/adview_guest.h"

#include "chrome/browser/guestview/adview/adview_constants.h"
#include "chrome/browser/guestview/guestview_constants.h"
#include "content/public/browser/web_contents.h"

using content::WebContents;

AdViewGuest::AdViewGuest(WebContents* guest_web_contents)
    : GuestView(guest_web_contents),
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

GuestView::Type AdViewGuest::GetViewType() const {
  return GuestView::ADVIEW;
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
  scoped_ptr<DictionaryValue> args(new DictionaryValue());
  args->SetString(guestview::kUrl, url.spec());
  args->SetBoolean(guestview::kIsTopLevel, is_main_frame);
  DispatchEvent(new GuestView::Event(adview::kEventLoadCommit, args.Pass()));
}
