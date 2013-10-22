// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/guestview/adview/adview_guest.h"

#include "base/strings/string_util.h"
#include "chrome/browser/guestview/adview/adview_constants.h"
#include "chrome/browser/guestview/guestview_constants.h"
#include "content/public/browser/web_contents.h"
#include "net/base/net_errors.h"

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
    const string16& frame_unique_name,
    bool is_main_frame,
    const GURL& url,
    content::PageTransition transition_type,
    content::RenderViewHost* render_view_host) {
  scoped_ptr<DictionaryValue> args(new DictionaryValue());
  args->SetString(guestview::kUrl, url.spec());
  args->SetBoolean(guestview::kIsTopLevel, is_main_frame);
  DispatchEvent(new GuestView::Event(adview::kEventLoadCommit, args.Pass()));
}

void AdViewGuest::DidFailProvisionalLoad(
    int64 frame_id,
    const string16& frame_unique_name,
    bool is_main_frame,
    const GURL& validated_url,
    int error_code,
    const string16& error_description,
    content::RenderViewHost* render_view_host) {
  // Translate the |error_code| into an error string.
  std::string error_type;
  RemoveChars(net::ErrorToString(error_code), "net::", &error_type);

  scoped_ptr<DictionaryValue> args(new DictionaryValue());
  args->SetBoolean(guestview::kIsTopLevel, is_main_frame);
  args->SetString(guestview::kUrl, validated_url.spec());
  args->SetString(guestview::kReason, error_type);
  DispatchEvent(new GuestView::Event(adview::kEventLoadAbort, args.Pass()));
}
