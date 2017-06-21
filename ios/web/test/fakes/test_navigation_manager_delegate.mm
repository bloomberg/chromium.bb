// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/test/fakes/test_navigation_manager_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

void TestNavigationManagerDelegate::GoToIndex(int index) {}
void TestNavigationManagerDelegate::LoadURLWithParams(
    const NavigationManager::WebLoadParams&) {}
void TestNavigationManagerDelegate::Reload() {}
void TestNavigationManagerDelegate::OnNavigationItemsPruned(
    size_t pruned_item_count) {}
void TestNavigationManagerDelegate::OnNavigationItemChanged() {}
void TestNavigationManagerDelegate::OnNavigationItemCommitted(
    const LoadCommittedDetails& load_details) {}
WebState* TestNavigationManagerDelegate::GetWebState() {
  return nullptr;
}

}  // namespace web
