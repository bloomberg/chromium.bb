// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_dialogs.h"

#include "chrome/browser/ui/views/website_settings/website_settings_popup_view.h"

// This file provides definitions of desktop browser dialog-creation methods for
// Mac where a Cocoa browser is using Views dialogs. I.e. it is included in the
// Cocoa build and definitions under chrome/browser/ui/cocoa may select at
// runtime whether to show a Cocoa dialog, or the toolkit-views dialog defined
// here (declared in browser_dialogs.h).

namespace chrome {

void ShowWebsiteSettingsBubbleViewsAtPoint(const gfx::Point& anchor_point,
                                           Profile* profile,
                                           content::WebContents* web_contents,
                                           const GURL& url,
                                           const content::SSLStatus& ssl) {
  WebsiteSettingsPopupView::ShowPopup(nullptr,
                                      gfx::Rect(anchor_point, gfx::Size()),
                                      profile, web_contents, url, ssl);
}

}  // namespace chrome
