// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_TEST_FAKES_TEST_NAVIGATION_MANAGER_DELEGATE_H_
#define IOS_WEB_TEST_FAKES_TEST_NAVIGATION_MANAGER_DELEGATE_H_

#import "ios/web/navigation/navigation_manager_delegate.h"

namespace web {

class TestNavigationManagerDelegate : public NavigationManagerDelegate {
 public:
  void GoToIndex(int index) override;
  void LoadURLWithParams(const NavigationManager::WebLoadParams&) override;
  void Reload() override;
  void OnNavigationItemsPruned(size_t pruned_item_count) override;
  void OnNavigationItemChanged() override;
  void OnNavigationItemCommitted(
      const LoadCommittedDetails& load_details) override;
  WebState* GetWebState() override;
};

}  // namespace web

#endif  // IOS_WEB_TEST_FAKES_TEST_NAVIGATION_MANAGER_DELEGATE_H_
