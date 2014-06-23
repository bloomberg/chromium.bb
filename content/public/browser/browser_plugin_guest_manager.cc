// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/browser_plugin_guest_manager.h"

#include "base/values.h"

namespace content {

bool BrowserPluginGuestManager::ForEachGuest(
    WebContents* embedder_web_contents,
    const GuestCallback& callback) {
  return false;
}

}  // content

