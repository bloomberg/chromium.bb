// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_TEST_FAKES_TEST_NAVIGATION_MANAGER_DELEGATE_H_
#define IOS_WEB_TEST_FAKES_TEST_NAVIGATION_MANAGER_DELEGATE_H_

#import "ios/web/navigation/navigation_manager_delegate.h"

@protocol CRWWebViewNavigationProxy;

namespace web {

class TestNavigationManagerDelegate : public NavigationManagerDelegate {
 public:
  void ClearTransientContent() override;
  void RecordPageStateInNavigationItem() override;
  void UpdateHtml5HistoryState() override;
  void WillLoadCurrentItemWithParams(const NavigationManager::WebLoadParams&,
                                     bool is_initial_navigation) override;
  void WillChangeUserAgentType() override;
  void LoadCurrentItem() override;
  void LoadIfNecessary() override;
  void Reload() override;
  void OnNavigationItemsPruned(size_t pruned_item_count) override;
  void OnNavigationItemChanged() override;
  void OnNavigationItemCommitted(
      const LoadCommittedDetails& load_details) override;
  WebState* GetWebState() override;
  id<CRWWebViewNavigationProxy> GetWebViewNavigationProxy() const override;

  // Setters for tests to inject dependencies.
  void SetWebViewNavigationProxy(id test_web_view);

 private:
  id test_web_view_;
};

}  // namespace web

#endif  // IOS_WEB_TEST_FAKES_TEST_NAVIGATION_MANAGER_DELEGATE_H_
