// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_content_setting_bubble_model_delegate.h"

#include "chrome/browser/content_settings/chrome_content_settings_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/tab_dialogs.h"
#include "chrome/common/url_constants.h"
#include "components/google/core/browser/google_util.h"

// The URL for when the user clicks "learn more" on the mixed scripting page
// icon bubble.
const char kInsecureScriptHelpUrl[] =
    "https://support.google.com/chrome/?p=unauthenticated";

BrowserContentSettingBubbleModelDelegate::
BrowserContentSettingBubbleModelDelegate(Browser* browser) : browser_(browser) {
}

BrowserContentSettingBubbleModelDelegate::
~BrowserContentSettingBubbleModelDelegate() {
}

void BrowserContentSettingBubbleModelDelegate::ShowCollectedCookiesDialog(
    content::WebContents* web_contents) {
  TabDialogs::FromWebContents(web_contents)->ShowCollectedCookies();
}

void BrowserContentSettingBubbleModelDelegate::ShowMediaSettingsPage() {
  // Microphone and camera settings appear in the content settings menu right
  // next to each other, the microphone section is first.
  chrome::ShowContentSettings(
          browser_, CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC);
}

void BrowserContentSettingBubbleModelDelegate::ShowContentSettingsPage(
    ContentSettingsType type) {
  if (type == CONTENT_SETTINGS_TYPE_MIXEDSCRIPT) {
    // We don't (yet?) implement user-settable exceptions for mixed script
    // blocking, so bounce to an explanatory page for now.
    content_settings::RecordMixedScriptAction(
        content_settings::MIXED_SCRIPT_ACTION_CLICKED_LEARN_MORE);
    chrome::AddSelectedTabWithURL(browser_,
                                  GURL(kInsecureScriptHelpUrl),
                                  ui::PAGE_TRANSITION_LINK);
  } else if (type == CONTENT_SETTINGS_TYPE_PROTOCOL_HANDLERS) {
    chrome::ShowSettingsSubPage(browser_, chrome::kHandlerSettingsSubPage);
  } else {
    chrome::ShowContentSettingsExceptions(browser_, type);
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
