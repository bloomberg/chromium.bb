// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SYNC_ONE_CLICK_SIGNIN_BUBBLE_DELEGATE_H_
#define CHROME_BROWSER_UI_SYNC_ONE_CLICK_SIGNIN_BUBBLE_DELEGATE_H_

class OneClickSigninBubbleDelegate {
 public:
  virtual ~OneClickSigninBubbleDelegate() {}
  virtual void OnLearnMoreLinkClicked(bool is_dialog) = 0;
  virtual void OnAdvancedLinkClicked() = 0;
};

#endif  // CHROME_BROWSER_UI_SYNC_ONE_CLICK_SIGNIN_BUBBLE_DELEGATE_H_
