// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_language_state_observer.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"

BrowserLanguageStateObserver::BrowserLanguageStateObserver(Browser* browser)
    : browser_(browser) {
}

BrowserLanguageStateObserver::~BrowserLanguageStateObserver() {
}

void BrowserLanguageStateObserver::OnTranslateEnabledChanged(
    content::WebContents* source) {
  if (source == browser_->tab_strip_model()->GetActiveWebContents())
    browser_->window()->UpdateToolbar(source);
}
