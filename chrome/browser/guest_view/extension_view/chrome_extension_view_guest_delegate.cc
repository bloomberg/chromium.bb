// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/guest_view/extension_view/chrome_extension_view_guest_delegate.h"

#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "extensions/browser/guest_view/extension_view/extension_view_guest.h"

namespace extensions {

ChromeExtensionViewGuestDelegate::ChromeExtensionViewGuestDelegate(
    ExtensionViewGuest* guest)
    : ExtensionViewGuestDelegate(guest) {
}

ChromeExtensionViewGuestDelegate::~ChromeExtensionViewGuestDelegate() {
}

void ChromeExtensionViewGuestDelegate::DidInitialize() {
  ChromeExtensionWebContentsObserver::CreateForWebContents(
      extension_view_guest()->web_contents());
}

}  // namespace extensions
