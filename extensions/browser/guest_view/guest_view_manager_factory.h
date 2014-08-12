// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_GUEST_VIEW_GUEST_VIEW_MANAGER_FACTORY_H_
#define EXTENSIONS_BROWSER_GUEST_VIEW_GUEST_VIEW_MANAGER_FACTORY_H_

namespace extensions {

class GuestViewManagerFactory {
 public:
  virtual GuestViewManager* CreateGuestViewManager(
      content::BrowserContext* context) = 0;

 protected:
  virtual ~GuestViewManagerFactory() {}
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_GUEST_VIEW_GUEST_VIEW_MANAGER_FACTORY_H_

