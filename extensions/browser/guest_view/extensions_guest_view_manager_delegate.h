// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_GUEST_VIEW_EXTENSIONS_GUEST_VIEW_MANAGER_DELEGATE_H_
#define EXTENSIONS_BROWSER_GUEST_VIEW_EXTENSIONS_GUEST_VIEW_MANAGER_DELEGATE_H_

#include "extensions/browser/guest_view/guest_view_manager_delegate.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace extensions {
class GuestViewBase;
class GuestViewManager;
}  // namespace extensions

namespace extensions {

// ExtensionsGuestViewManagerDelegate implements GuestViewManager functionality
// specific to Chromium builds that include the extensions module.
class ExtensionsGuestViewManagerDelegate
    : public guestview::GuestViewManagerDelegate {
 public:
  explicit ExtensionsGuestViewManagerDelegate(content::BrowserContext* context);
  ~ExtensionsGuestViewManagerDelegate() override;

  // GuestViewManagerDelegate implementation.
  void DispatchEvent(const std::string& event_name,
                     scoped_ptr<base::DictionaryValue> args,
                     GuestViewBase* guest,
                     int instance_id) override;
  bool IsGuestAvailableToContext(extensions::GuestViewBase* guest) override;
  bool IsOwnedByExtension(extensions::GuestViewBase* guest) override;
  void RegisterAdditionalGuestViewTypes() override;

 private:
  content::BrowserContext* const context_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_GUEST_VIEW_EXTENSIONS_GUEST_VIEW_MANAGER_DELEGATE_H_
