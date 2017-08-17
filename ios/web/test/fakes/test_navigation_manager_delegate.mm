// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/test/fakes/test_navigation_manager_delegate.h"
#import "ios/web/web_state/ui/crw_web_view_navigation_proxy.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

void TestNavigationManagerDelegate::ClearTransientContent() {}
void TestNavigationManagerDelegate::RecordPageStateInNavigationItem() {}
void TestNavigationManagerDelegate::UpdateHtml5HistoryState() {}
void TestNavigationManagerDelegate::WillLoadCurrentItemWithParams(
    const NavigationManager::WebLoadParams&,
    bool is_initial_navigation) {}
void TestNavigationManagerDelegate::WillChangeUserAgentType() {}
void TestNavigationManagerDelegate::LoadCurrentItem() {}
void TestNavigationManagerDelegate::Reload() {}
void TestNavigationManagerDelegate::OnNavigationItemsPruned(
    size_t pruned_item_count) {}
void TestNavigationManagerDelegate::OnNavigationItemChanged() {}
void TestNavigationManagerDelegate::OnNavigationItemCommitted(
    const LoadCommittedDetails& load_details) {}
WebState* TestNavigationManagerDelegate::GetWebState() {
  return nullptr;
}
id<CRWWebViewNavigationProxy>
TestNavigationManagerDelegate::GetWebViewNavigationProxy() const {
  return test_web_view_;
}

void TestNavigationManagerDelegate::SetWebViewNavigationProxy(id web_view) {
  test_web_view_ = web_view;
}

}  // namespace web
