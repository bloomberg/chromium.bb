// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/test_navigation_manager.h"

namespace web {

TestNavigationManager::TestNavigationManager() {}

TestNavigationManager::~TestNavigationManager() {}

BrowserState* TestNavigationManager::GetBrowserState() const {
  NOTREACHED();
  return nullptr;
}

WebState* TestNavigationManager::GetWebState() const {
  NOTREACHED();
  return nullptr;
}

NavigationItem* TestNavigationManager::GetVisibleItem() const {
  return visible_item_;
}

void TestNavigationManager::SetVisibleItem(NavigationItem* item) {
  visible_item_ = item;
}

NavigationItem* TestNavigationManager::GetLastCommittedItem() const {
  return last_committed_item_;
}

void TestNavigationManager::SetLastCommittedItem(NavigationItem* item) {
  last_committed_item_ = item;
}

NavigationItem* TestNavigationManager::GetPendingItem() const {
  return pending_item_;
}

void TestNavigationManager::SetPendingItem(NavigationItem* item) {
  pending_item_ = item;
}

web::NavigationItem* TestNavigationManager::GetTransientItem() const {
  NOTREACHED();
  return nullptr;
}

void TestNavigationManager::DiscardNonCommittedItems() {
  NOTREACHED();
  return;
}

void TestNavigationManager::LoadIfNecessary() {
  NOTREACHED();
  return;
}

void TestNavigationManager::LoadURLWithParams(
    const NavigationManager::WebLoadParams& params) {
  NOTREACHED();
  return;
}

void TestNavigationManager::AddTransientURLRewriter(
    BrowserURLRewriter::URLRewriter rewriter) {
  NOTREACHED();
  return;
}

int TestNavigationManager::GetItemCount() const {
  NOTREACHED();
  return 0;
}

web::NavigationItem* TestNavigationManager::GetItemAtIndex(size_t index) const {
  NOTREACHED();
  return nullptr;
}

int TestNavigationManager::GetCurrentItemIndex() const {
  NOTREACHED();
  return 0;
}

int TestNavigationManager::GetLastCommittedItemIndex() const {
  NOTREACHED();
  return 0;
}

int TestNavigationManager::GetPendingItemIndex() const {
  NOTREACHED();
  return 0;
}

bool TestNavigationManager::RemoveItemAtIndex(int index) {
  NOTREACHED();
  return false;
}

bool TestNavigationManager::CanGoBack() const {
  NOTREACHED();
  return false;
}

bool TestNavigationManager::CanGoForward() const {
  NOTREACHED();
  return false;
}

bool TestNavigationManager::CanGoToOffset(int offset) const {
  NOTREACHED();
  return false;
}

void TestNavigationManager::GoBack() {
  NOTREACHED();
  return;
}

void TestNavigationManager::GoForward() {
  NOTREACHED();
  return;
}

void TestNavigationManager::GoToIndex(int index) {
  NOTREACHED();
  return;
}

void TestNavigationManager::Reload(bool check_for_repost) {
  NOTREACHED();
  return;
}

}  // namespace web
