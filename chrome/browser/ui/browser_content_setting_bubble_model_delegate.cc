// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_content_setting_bubble_model_delegate.h"

#include "chrome/browser/google/google_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/chrome_pages.h"

// The URL for when the user clicks "learn more" on the mixed scripting page
// icon bubble.
const char kInsecureScriptHelpUrl[] =
    "https://support.google.com/chrome/bin/answer.py?answer=1342714";

BrowserContentSettingBubbleModelDelegate::
BrowserContentSettingBubbleModelDelegate(Browser* browser) : browser_(browser) {
}

BrowserContentSettingBubbleModelDelegate::
~BrowserContentSettingBubbleModelDelegate() {
}

void BrowserContentSettingBubbleModelDelegate::ShowCollectedCookiesDialog(
    TabContents* contents) {
  browser::ShowCollectedCookiesDialog(contents);
}

void BrowserContentSettingBubbleModelDelegate::ShowContentSettingsPage(
    ContentSettingsType type) {
  if (type == CONTENT_SETTINGS_TYPE_MIXEDSCRIPT) {
    // We don't (yet?) implement user-settable exceptions for mixed script
    // blocking, so bounce to an explanatory page for now.
    GURL url(google_util::AppendGoogleLocaleParam(
        GURL(kInsecureScriptHelpUrl)));
    chrome::AddSelectedTabWithURL(browser_, url, content::PAGE_TRANSITION_LINK);
  } else {
    chrome::ShowContentSettings(browser_, type);
  }
}
