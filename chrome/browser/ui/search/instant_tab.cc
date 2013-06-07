// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/instant_tab.h"
#include "content/public/browser/web_contents.h"

InstantTab::InstantTab(InstantPage::Delegate* delegate)
    : InstantPage(delegate, "") {
}

InstantTab::~InstantTab() {
}

void InstantTab::Init(content::WebContents* contents) {
  SetContents(contents);
  if (!contents->IsWaitingForResponse())
    DetermineIfPageSupportsInstant();
}

bool InstantTab::ShouldProcessAboutToNavigateMainFrame() {
  return true;
}

bool InstantTab::ShouldProcessSetSuggestions() {
  return true;
}

bool InstantTab::ShouldProcessFocusOmnibox() {
  return true;
}

bool InstantTab::ShouldProcessNavigateToURL() {
  return true;
}
