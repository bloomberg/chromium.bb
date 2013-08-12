// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/browser_plugin_guest_delegate.h"

namespace content {

bool BrowserPluginGuestDelegate::HandleKeyboardEvent(
    const NativeWebKeyboardEvent& event) {
  return false;
}

bool BrowserPluginGuestDelegate::RequestPermission(
    BrowserPluginPermissionType permission_type,
    const base::DictionaryValue& request_info,
    const PermissionResponseCallback& callback) {
  return false;
}

}  // namespace content
