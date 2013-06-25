// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/instant_tab.h"
#include "content/public/browser/web_contents.h"

InstantTab::InstantTab(InstantPage::Delegate* delegate,
                       bool is_incognito)
    : InstantPage(delegate, "", is_incognito) {
}

InstantTab::~InstantTab() {
}

void InstantTab::Init(content::WebContents* contents) {
  SetContents(contents);
}

bool InstantTab::ShouldProcessAboutToNavigateMainFrame() {
  return true;
}

bool InstantTab::ShouldProcessFocusOmnibox() {
  return true;
}

bool InstantTab::ShouldProcessNavigateToURL() {
  return true;
}

bool InstantTab::ShouldProcessDeleteMostVisitedItem() {
  return true;
}

bool InstantTab::ShouldProcessUndoMostVisitedDeletion() {
  return true;
}

bool InstantTab::ShouldProcessUndoAllMostVisitedDeletions() {
  return true;
}
