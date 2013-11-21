// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_language_state_observer.h"

#include "chrome/browser/tab_contents/language_state.h"
#include "chrome/browser/translate/translate_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"

BrowserLanguageStateObserver::BrowserLanguageStateObserver(Browser* browser)
    : browser_(browser) {
}

BrowserLanguageStateObserver::~BrowserLanguageStateObserver() {
}

void BrowserLanguageStateObserver::OnIsPageTranslatedChanged(
    content::WebContents* source) {
  if (source == browser_->tab_strip_model()->GetActiveWebContents()) {
    TranslateTabHelper* translate_tab_helper =
        TranslateTabHelper::FromWebContents(source);
    LanguageState& language_state = translate_tab_helper->language_state();
    browser_->window()->SetTranslateIconToggled(
        language_state.IsPageTranslated());
  }
}

void BrowserLanguageStateObserver::OnTranslateEnabledChanged(
    content::WebContents* source) {
  if (source == browser_->tab_strip_model()->GetActiveWebContents())
    browser_->window()->UpdateToolbar(source);
}
