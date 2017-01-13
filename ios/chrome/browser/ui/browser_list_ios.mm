// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/browser_list_ios.h"

#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/tabs/tab_model.h"

BrowserListIOS::BrowserVector* BrowserListIOS::browsers_;

void BrowserListIOS::AddBrowser(id<BrowserIOS> browser) {
  EnsureBrowsersIsValid();
  DCHECK(browser);
  browsers_->push_back(browser);
}

void BrowserListIOS::RemoveBrowser(id<BrowserIOS> browser) {
  if (!browsers_)
    return;
  DCHECK(browser);
  const iterator remove_browser =
      std::find(browsers_->begin(), browsers_->end(), browser);
  if (remove_browser != browsers_->end())
    browsers_->erase(remove_browser);
}

id<BrowserIOS> BrowserListIOS::GetLastActiveWithBrowserState(
    ios::ChromeBrowserState* browser_state) {
  DCHECK(browser_state);
  for (const_iterator i = BrowserListIOS::begin(); i != BrowserListIOS::end();
       ++i) {
    if ([*i browserState] == browser_state)
      return *i;
  }
  return NULL;
}

BrowserListIOS::const_iterator BrowserListIOS::begin() {
  EnsureBrowsersIsValid();
  return browsers_->begin();
}

BrowserListIOS::const_iterator BrowserListIOS::end() {
  EnsureBrowsersIsValid();
  return browsers_->end();
}

// static
bool BrowserListIOS::IsOffTheRecordSessionActive() {
  for (const_iterator i = BrowserListIOS::begin(); i != BrowserListIOS::end();
       ++i) {
    // Unlike desktop, an Incognito browser can exist but be empty, so filter
    // that case out.
    if ([*i browserState] && [*i browserState]->IsOffTheRecord() &&
        ![[*i tabModel] isEmpty])
      return true;
  }
  return false;
}

void BrowserListIOS::EnsureBrowsersIsValid() {
  if (browsers_)
    return;
  browsers_ = new BrowserVector;
}
