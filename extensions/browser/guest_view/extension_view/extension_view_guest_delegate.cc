// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/guest_view/extension_view/extension_view_guest_delegate.h"

namespace extensions {

ExtensionViewGuestDelegate::ExtensionViewGuestDelegate(
    ExtensionViewGuest* guest)
    : guest_(guest) {
}

ExtensionViewGuestDelegate::~ExtensionViewGuestDelegate() {
}

}  // namespace extensions
