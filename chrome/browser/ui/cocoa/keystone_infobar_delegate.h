// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_KEYSTONE_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_UI_COCOA_KEYSTONE_INFOBAR_DELEGATE_H_

class Profile;

class KeystoneInfoBar {
 public:
  // If the application is Keystone-enabled and not on a read-only filesystem
  // (capable of being auto-updated), and Keystone indicates that it needs
  // ticket promotion, PromotionInfoBar displays an info bar asking the user
  // to promote the ticket.  The user will need to authenticate in order to
  // gain authorization to perform the promotion.  The info bar is not shown
  // if its "don't ask" button was ever clicked, if the "don't check default
  // browser" command-line flag is present, on the very first launch, or if
  // another info bar is already showing in the active tab.
  static void PromotionInfoBar(Profile* profile);
};

#endif  // CHROME_BROWSER_UI_COCOA_KEYSTONE_INFOBAR_DELEGATE_H_
