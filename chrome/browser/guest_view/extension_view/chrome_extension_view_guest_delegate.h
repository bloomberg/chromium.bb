// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GUEST_VIEW_EXTENSION_VIEW_CHROME_EXTENSION_VIEW_GUEST_DELEGATE_H_
#define CHROME_BROWSER_GUEST_VIEW_EXTENSION_VIEW_CHROME_EXTENSION_VIEW_GUEST_DELEGATE_H_

#include "extensions/browser/guest_view/extension_view/extension_view_guest_delegate.h"

namespace extensions {

class ExtensionViewGuest;

class ChromeExtensionViewGuestDelegate : public ExtensionViewGuestDelegate {
 public:
  explicit ChromeExtensionViewGuestDelegate(ExtensionViewGuest* guest);
  ~ChromeExtensionViewGuestDelegate() override;

  void DidInitialize() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeExtensionViewGuestDelegate);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_GUEST_VIEW_EXTENSION_VIEW_CHROME_EXTENSION_VIEW_GUEST_DELEGATE_H_
