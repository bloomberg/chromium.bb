// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_FAKES_TEST_NAVIGATION_MANAGER_H_
#define IOS_WEB_PUBLIC_TEST_FAKES_TEST_NAVIGATION_MANAGER_H_

#import "ios/web/public/navigation_manager.h"

namespace web {

// A minimal implementation of web::NavigationManager that raises NOTREACHED()
// on most calls.
class TestNavigationManager : public web::NavigationManager {
 public:
  TestNavigationManager();
  ~TestNavigationManager() override;
  BrowserState* GetBrowserState() const override;
  WebState* GetWebState() const override;
  NavigationItem* GetVisibleItem() const override;
  NavigationItem* GetLastCommittedItem() const override;
  NavigationItem* GetPendingItem() const override;
  NavigationItem* GetTransientItem() const override;
  void DiscardNonCommittedItems() override;
  void LoadIfNecessary() override;
  void LoadURLWithParams(const NavigationManager::WebLoadParams&) override;
  void AddTransientURLRewriter(
      BrowserURLRewriter::URLRewriter rewriter) override;
  int GetItemCount() const override;
  NavigationItem* GetItemAtIndex(size_t index) const override;
  int GetCurrentItemIndex() const override;
  int GetPendingItemIndex() const override;
  int GetLastCommittedItemIndex() const override;
  bool RemoveItemAtIndex(int index) override;
  bool CanGoBack() const override;
  bool CanGoForward() const override;
  bool CanGoToOffset(int offset) const override;
  void GoBack() override;
  void GoForward() override;
  void GoToIndex(int index) override;
  void Reload(bool check_for_reposts) override;
  void OverrideDesktopUserAgentForNextPendingItem() override;

  // Setters for test data.
  void SetLastCommittedItem(NavigationItem* item);
  void SetPendingItem(NavigationItem* item);
  void SetVisibleItem(NavigationItem* item);

 private:
  NavigationItem* pending_item_;
  NavigationItem* last_committed_item_;
  NavigationItem* visible_item_;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_FAKES_TEST_NAVIGATION_MANAGER_H_
