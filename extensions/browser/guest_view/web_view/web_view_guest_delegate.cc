// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/guest_view/web_view/web_view_guest_delegate.h"

namespace extensions {

WebViewGuestDelegate::WebViewGuestDelegate(WebViewGuest* web_view_guest)
    : web_view_guest_(web_view_guest) {
}

WebViewGuestDelegate::~WebViewGuestDelegate() {
}

}  // namespace extensions
