// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SYNC_ONE_CLICK_SIGNIN_BUBBLE_LINKS_DELEGATE_H_
#define CHROME_BROWSER_UI_SYNC_ONE_CLICK_SIGNIN_BUBBLE_LINKS_DELEGATE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/ui/sync/one_click_signin_bubble_delegate.h"

class Browser;

class OneClickSigninBubbleLinksDelegate : public OneClickSigninBubbleDelegate {
 public:
  // |browser| must outlive the delegate.
  explicit OneClickSigninBubbleLinksDelegate(Browser* browser);
  virtual ~OneClickSigninBubbleLinksDelegate();

 private:
  // OneClickSigninBubbleDelegate:
  virtual void OnLearnMoreLinkClicked(bool is_dialog) OVERRIDE;
  virtual void OnAdvancedLinkClicked() OVERRIDE;

  // Browser in which the links should be opened.
  Browser* browser_;

  DISALLOW_COPY_AND_ASSIGN(OneClickSigninBubbleLinksDelegate);
};

#endif  // CHROME_BROWSER_UI_SYNC_ONE_CLICK_SIGNIN_BUBBLE_LINKS_DELEGATE_H_
