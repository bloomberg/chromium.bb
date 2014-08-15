// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/guest_view/web_view/web_view_permission_helper_delegate.h"

namespace extensions {

WebViewPermissionHelperDelegate::WebViewPermissionHelperDelegate(
    content::WebContents* contents)
    : content::WebContentsObserver(contents) {
}

WebViewPermissionHelperDelegate::~WebViewPermissionHelperDelegate() {
}

}  // namespace extensions
