// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/browser_plugin_guest_manager_delegate.h"

#include "base/values.h"

namespace content {

content::WebContents* BrowserPluginGuestManagerDelegate::CreateGuest(
    SiteInstance* embedder_site_instance,
    int instance_id,
    const std::string& storage_partition_id,
    bool persist_storage,
    scoped_ptr<base::DictionaryValue> extra_params) {
  return NULL;
}

int BrowserPluginGuestManagerDelegate::GetNextInstanceID() {
  return 0;
}

bool BrowserPluginGuestManagerDelegate::ForEachGuest(
    WebContents* embedder_web_contents,
    const GuestCallback& callback) {
  return false;
}

}  // content

