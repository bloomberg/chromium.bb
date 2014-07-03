// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/guest_view/app_view/app_view_guest.h"

// static
const char AppViewGuest::Type[] = "appview";

AppViewGuest::AppViewGuest(content::BrowserContext* browser_context,
                           int guest_instance_id)
    : GuestView<AppViewGuest>(browser_context, guest_instance_id) {
}

AppViewGuest::~AppViewGuest() {
}

void AppViewGuest::CreateWebContents(
    const std::string& embedder_extension_id,
    int embedder_render_process_id,
    const base::DictionaryValue& create_params,
    const WebContentsCreatedCallback& callback) {
  // TODO(fsamuel): Create a WebContents with the appropriate SiteInstance here.
  // After the WebContents has been created, call the |callback|.
  // callback.Run(new_web_contents);
}

void AppViewGuest::DidAttachToEmbedder() {
  // This is called after the guest process has been attached to a host
  // element. This means that the host element knows how to route input
  // events to the guest, and the guest knows how to get frames to the
  // embedder.
  // TODO(fsamuel): Perform the initial navigation here.
}
