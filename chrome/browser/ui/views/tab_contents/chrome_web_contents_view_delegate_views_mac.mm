// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/views/tab_contents/chrome_web_contents_view_delegate_views_mac.h"

#include "chrome/browser/ui/sad_tab_helper.h"
#include "chrome/browser/ui/views/sad_tab_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/ui_features.h"
#include "ui/views/widget/widget.h"

ChromeWebContentsViewDelegateViewsMac::ChromeWebContentsViewDelegateViewsMac(
    content::WebContents* web_contents)
    : ChromeWebContentsViewDelegateMac(web_contents) {}

void ChromeWebContentsViewDelegateViewsMac::SizeChanged(const gfx::Size& size) {
  SadTabHelper* sad_tab_helper = SadTabHelper::FromWebContents(web_contents());
  if (!sad_tab_helper)
    return;
  SadTabView* sad_tab = static_cast<SadTabView*>(sad_tab_helper->sad_tab());
  if (sad_tab)
    sad_tab->GetWidget()->SetBounds(gfx::Rect(size));
}

#if BUILDFLAG(MAC_VIEWS_BROWSER)
namespace chrome {

content::WebContentsViewDelegate* CreateWebContentsViewDelegate(
    content::WebContents* web_contents) {
  return new ChromeWebContentsViewDelegateViewsMac(web_contents);
}

}  // namespace chrome
#endif  // MAC_VIEWS_BROWSER
