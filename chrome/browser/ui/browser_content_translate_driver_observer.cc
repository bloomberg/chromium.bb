// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_content_translate_driver_observer.h"

#include "chrome/browser/translate/translate_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/translate/core/browser/language_state.h"

BrowserContentTranslateDriverObserver::BrowserContentTranslateDriverObserver(
    Browser* browser) : browser_(browser) {
}

BrowserContentTranslateDriverObserver::
    ~BrowserContentTranslateDriverObserver() {
}

void BrowserContentTranslateDriverObserver::OnIsPageTranslatedChanged(
    content::WebContents* source) {
  if (source == browser_->tab_strip_model()->GetActiveWebContents()) {
    TranslateTabHelper* translate_tab_helper =
        TranslateTabHelper::FromWebContents(source);
    LanguageState& language_state = translate_tab_helper->GetLanguageState();
    browser_->window()->SetTranslateIconToggled(
        language_state.IsPageTranslated());
  }
}

void BrowserContentTranslateDriverObserver::OnTranslateEnabledChanged(
    content::WebContents* source) {
  if (source == browser_->tab_strip_model()->GetActiveWebContents())
    browser_->window()->UpdateToolbar(source);
}
