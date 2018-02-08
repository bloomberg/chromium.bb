// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/views/tab_contents/chrome_web_contents_view_delegate_views_mac.h"

#include "chrome/browser/ui/sad_tab_helper.h"
#include "chrome/browser/ui/views/sad_tab_view.h"
#include "chrome/browser/ui/views/tab_contents/chrome_web_contents_view_focus_helper.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/ui_features.h"
#include "ui/views/widget/widget.h"

ChromeWebContentsViewDelegateViewsMac::ChromeWebContentsViewDelegateViewsMac(
    content::WebContents* web_contents)
    : ChromeWebContentsViewDelegateMac(web_contents),
      focus_helper_(
          std::make_unique<ChromeWebContentsViewFocusHelper>(web_contents)) {}

ChromeWebContentsViewDelegateViewsMac::
    ~ChromeWebContentsViewDelegateViewsMac() {
}

void ChromeWebContentsViewDelegateViewsMac::StoreFocus() {
  focus_helper_->StoreFocus();
}

bool ChromeWebContentsViewDelegateViewsMac::RestoreFocus() {
  return focus_helper_->RestoreFocus();
}

void ChromeWebContentsViewDelegateViewsMac::ResetStoredFocus() {
  focus_helper_->ResetStoredFocus();
}

bool ChromeWebContentsViewDelegateViewsMac::Focus() {
  return focus_helper_->Focus();
}

bool ChromeWebContentsViewDelegateViewsMac::TakeFocus(bool reverse) {
  return focus_helper_->TakeFocus(reverse);
}

#if BUILDFLAG(MAC_VIEWS_BROWSER)

content::WebContentsViewDelegate* CreateWebContentsViewDelegate(
    content::WebContents* web_contents) {
  return new ChromeWebContentsViewDelegateViewsMac(web_contents);
}

#endif  // MAC_VIEWS_BROWSER
