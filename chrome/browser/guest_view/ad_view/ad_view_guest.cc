// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/guest_view/ad_view/ad_view_guest.h"

#include "base/strings/string_util.h"
#include "chrome/browser/guest_view/ad_view/ad_view_constants.h"
#include "chrome/browser/guest_view/guest_view_constants.h"
#include "content/public/browser/web_contents.h"
#include "net/base/net_errors.h"

using content::WebContents;

AdViewGuest::AdViewGuest(int guest_instance_id,
                         WebContents* guest_web_contents,
                         const std::string& extension_id)
    : GuestView<AdViewGuest>(guest_instance_id,
                             guest_web_contents,
                             extension_id),
      WebContentsObserver(guest_web_contents) {
}

// static
const char AdViewGuest::Type[] = "adview";

AdViewGuest::~AdViewGuest() {
}

void AdViewGuest::DidCommitProvisionalLoadForFrame(
    int64 frame_id,
    const base::string16& frame_unique_name,
    bool is_main_frame,
    const GURL& url,
    content::PageTransition transition_type,
    content::RenderViewHost* render_view_host) {
  scoped_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  args->SetString(guestview::kUrl, url.spec());
  args->SetBoolean(guestview::kIsTopLevel, is_main_frame);
  DispatchEvent(
      new GuestViewBase::Event(adview::kEventLoadCommit, args.Pass()));
}

void AdViewGuest::DidFailProvisionalLoad(
    int64 frame_id,
    const base::string16& frame_unique_name,
    bool is_main_frame,
    const GURL& validated_url,
    int error_code,
    const base::string16& error_description,
    content::RenderViewHost* render_view_host) {
  // Translate the |error_code| into an error string.
  std::string error_type;
  base::RemoveChars(net::ErrorToString(error_code), "net::", &error_type);

  scoped_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  args->SetBoolean(guestview::kIsTopLevel, is_main_frame);
  args->SetString(guestview::kUrl, validated_url.spec());
  args->SetString(guestview::kReason, error_type);
  DispatchEvent(new GuestViewBase::Event(adview::kEventLoadAbort, args.Pass()));
}
