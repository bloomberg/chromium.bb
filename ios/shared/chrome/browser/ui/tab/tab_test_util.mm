// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/shared/chrome/browser/ui/tab/tab_test_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

int TabNavigationManager::GetItemCount() const {
  return item_count_;
}

bool TabNavigationManager::CanGoBack() const {
  return false;
}

bool TabNavigationManager::CanGoForward() const {
  return false;
}

void TabNavigationManager::LoadURLWithParams(
    const NavigationManager::WebLoadParams&) {
  has_loaded_url_ = true;
}

void TabNavigationManager::SetItemCount(int count) {
  item_count_ = count;
}

bool TabNavigationManager::GetHasLoadedUrl() {
  return has_loaded_url_;
}
