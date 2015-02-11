// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_GUEST_VIEW_EXTENSION_VIEW_EXTENSION_VIEW_GUEST_DELEGATE_H_
#define EXTENSIONS_BROWSER_GUEST_VIEW_EXTENSION_VIEW_EXTENSION_VIEW_GUEST_DELEGATE_H_

#include "base/macros.h"

namespace extensions {

class ExtensionViewGuest;

// Interface to handle communication between ExtensionViewGuest and
// the browser.
class ExtensionViewGuestDelegate {
 public:
  explicit ExtensionViewGuestDelegate(ExtensionViewGuest* guest);
  virtual ~ExtensionViewGuestDelegate();

  // Called from ExtensionViewGuest::DidInitialize().
  virtual void DidInitialize() = 0;

  ExtensionViewGuest* extension_view_guest() const { return guest_; }

 private:
  ExtensionViewGuest* const guest_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionViewGuestDelegate);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_GUEST_VIEW_EXTENSION_VIEW_EXTENSION_VIEW_GUEST_DELEGATE_H_
