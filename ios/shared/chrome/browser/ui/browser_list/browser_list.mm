// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/shared/chrome/browser/ui/browser_list/browser_list.h"

#include <stdint.h>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const char kBrowserListKey = 0;
}

BrowserList::BrowserList(ios::ChromeBrowserState* browser_state)
    : browser_state_(browser_state) {
  DCHECK(browser_state_);
}

BrowserList::~BrowserList() = default;

// static
BrowserList* BrowserList::FromBrowserState(
    ios::ChromeBrowserState* browser_state) {
  base::SupportsUserData::Data* data =
      browser_state->GetUserData(&kBrowserListKey);
  if (!data) {
    browser_state->SetUserData(&kBrowserListKey,
                               base::MakeUnique<BrowserList>(browser_state));
    data = browser_state->GetUserData(&kBrowserListKey);
  }
  DCHECK(data);
  return static_cast<BrowserList*>(data);
}

int BrowserList::GetBrowserCount() const {
  DCHECK_LE(browsers_.size(), static_cast<size_t>(INT_MAX));
  return static_cast<int>(browsers_.size());
}

int BrowserList::ContainsIndex(int index) const {
  return 0 <= index && index < GetBrowserCount();
}

Browser* BrowserList::GetBrowserAtIndex(int index) const {
  DCHECK(ContainsIndex(index));
  return browsers_[index].get();
}

Browser* BrowserList::CreateNewBrowser() {
  browsers_.push_back(base::MakeUnique<Browser>(browser_state_));
  return browsers_.back().get();
}

void BrowserList::CloseBrowserAtIndex(int index) {
  DCHECK(ContainsIndex(index));
  browsers_.erase(browsers_.begin() + index);
}
