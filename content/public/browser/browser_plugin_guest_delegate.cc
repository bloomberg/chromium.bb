// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/browser_plugin_guest_delegate.h"

namespace content {

WebContents* BrowserPluginGuestDelegate::CreateNewGuestWindow(
    const WebContents::CreateParams& create_params) {
  return NULL;
}

bool BrowserPluginGuestDelegate::Find(int request_id,
                                      const base::string16& search_text,
                                      const blink::WebFindOptions& options,
                                      bool is_full_page_plugin) {
  return false;
}

}  // namespace content
