// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_content_setting_bubble_model_delegate.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/common/url_constants.h"
#include "components/google/core/browser/google_util.h"

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
    content::WebContents* web_contents) {
  chrome::ShowCollectedCookiesDialog(web_contents);
}

void BrowserContentSettingBubbleModelDelegate::ShowContentSettingsPage(
    ContentSettingsType type) {
  switch (type) {
    case CONTENT_SETTINGS_TYPE_MIXEDSCRIPT:
      // We don't (yet?) implement user-settable exceptions for mixed script
      // blocking, so bounce to an explanatory page for now.
      chrome::AddSelectedTabWithURL(browser_,
                                    GURL(kInsecureScriptHelpUrl),
                                    ui::PAGE_TRANSITION_LINK);
      return;
    case CONTENT_SETTINGS_TYPE_PROTOCOL_HANDLERS:
      chrome::ShowSettingsSubPage(browser_, chrome::kHandlerSettingsSubPage);
      return;
    default:
      chrome::ShowContentSettings(browser_, type);
      return;
  }
}

void BrowserContentSettingBubbleModelDelegate::ShowLearnMorePage(
    ContentSettingsType type) {
  if (type != CONTENT_SETTINGS_TYPE_PLUGINS)
    return;
  chrome::AddSelectedTabWithURL(browser_,
                                GURL(chrome::kBlockedPluginLearnMoreURL),
                                ui::PAGE_TRANSITION_LINK);
}
