// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SYNC_BUBBLE_SYNC_PROMO_DELEGATE_H_
#define CHROME_BROWSER_UI_SYNC_BUBBLE_SYNC_PROMO_DELEGATE_H_

#include "components/signin/core/browser/account_info.h"

class BubbleSyncPromoDelegate {
 public:
  virtual ~BubbleSyncPromoDelegate() {}

  // Shows the chrome sign-in page.
  virtual void ShowBrowserSignin() = 0;

  // Enables sync for |account|.
  virtual void EnableSync(const AccountInfo& account) = 0;
};

#endif  // CHROME_BROWSER_UI_SYNC_BUBBLE_SYNC_PROMO_DELEGATE_H_
