// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/wk_based_navigation_manager_impl.h"

#include <memory>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#import "ios/web/navigation/navigation_item_impl.h"
#import "ios/web/navigation/navigation_manager_delegate.h"
#include "ios/web/public/load_committed_details.h"
#import "ios/web/public/navigation_item.h"
#import "ios/web/public/web_client.h"
#import "ios/web/web_state/ui/crw_web_view_navigation_proxy.h"
#import "net/base/mac/url_conversions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@class CRWSessionController;

namespace web {

WKBasedNavigationManagerImpl::WKBasedNavigationManagerImpl() = default;

WKBasedNavigationManagerImpl::~WKBasedNavigationManagerImpl() = default;

void WKBasedNavigationManagerImpl::SetSessionController(
    CRWSessionController* session_controller) {}

void WKBasedNavigationManagerImpl::InitializeSession() {}

void WKBasedNavigationManagerImpl::ReplaceSessionHistory(
    std::vector<std::unique_ptr<NavigationItem>> items,
    int current_index) {
  DLOG(WARNING) << "Not yet implemented.";
}

void WKBasedNavigationManagerImpl::OnNavigationItemsPruned(
    size_t pruned_item_count) {
  delegate_->OnNavigationItemsPruned(pruned_item_count);
}

void WKBasedNavigationManagerImpl::OnNavigationItemChanged() {
  delegate_->OnNavigationItemChanged();
}

void WKBasedNavigationManagerImpl::OnNavigationItemCommitted() {
  LoadCommittedDetails details;
  details.item = GetLastCommittedItem();
  DCHECK(details.item);
  details.previous_item_index = GetPreviousItemIndex();
  if (details.previous_item_index >= 0) {
    NavigationItem* previous_item = GetItemAtIndex(details.previous_item_index);
    DCHECK(previous_item);
    details.previous_url = previous_item->GetURL();
    details.is_in_page = AreUrlsFragmentChangeNavigation(
        details.previous_url, details.item->GetURL());
  } else {
    details.previous_url = GURL();
    details.is_in_page = NO;
  }

  delegate_->OnNavigationItemCommitted(details);
}

CRWSessionController* WKBasedNavigationManagerImpl::GetSessionController()
    const {
  return nil;
}

void WKBasedNavigationManagerImpl::AddTransientItem(const GURL& url) {
  DLOG(WARNING) << "Not yet implemented.";
}

void WKBasedNavigationManagerImpl::AddPendingItem(
    const GURL& url,
    const web::Referrer& referrer,
    ui::PageTransition navigation_type,
    NavigationInitiationType initiation_type,
    UserAgentOverrideOption user_agent_override_option) {
  DLOG(WARNING) << "Not yet implemented.";
}

void WKBasedNavigationManagerImpl::CommitPendingItem() {
  DLOG(WARNING) << "Not yet implemented.";
}

int WKBasedNavigationManagerImpl::GetIndexForOffset(int offset) const {
  DLOG(WARNING) << "Not yet implemented.";
  return -1;
}

BrowserState* WKBasedNavigationManagerImpl::GetBrowserState() const {
  return browser_state_;
}

WebState* WKBasedNavigationManagerImpl::GetWebState() const {
  return delegate_->GetWebState();
}

NavigationItem* WKBasedNavigationManagerImpl::GetVisibleItem() const {
  DLOG(WARNING) << "Not yet implemented.";
  return nullptr;
}

NavigationItem* WKBasedNavigationManagerImpl::GetLastCommittedItem() const {
  DLOG(WARNING) << "Not yet implemented.";
  return nullptr;
}

NavigationItem* WKBasedNavigationManagerImpl::GetPendingItem() const {
  DLOG(WARNING) << "Not yet implemented.";
  return nullptr;
}

NavigationItem* WKBasedNavigationManagerImpl::GetTransientItem() const {
  DLOG(WARNING) << "Not yet implemented.";
  return nullptr;
}

void WKBasedNavigationManagerImpl::DiscardNonCommittedItems() {
  DLOG(WARNING) << "Not yet implemented.";
}

void WKBasedNavigationManagerImpl::LoadURLWithParams(
    const NavigationManager::WebLoadParams&) {
  DLOG(WARNING) << "Not yet implemented.";
}


int WKBasedNavigationManagerImpl::GetItemCount() const {
  id<CRWWebViewNavigationProxy> proxy = delegate_->GetWebViewNavigationProxy();
  if (proxy) {
    int count_current_page = proxy.backForwardList.currentItem ? 1 : 0;
    return static_cast<int>(proxy.backForwardList.backList.count) +
           count_current_page +
           static_cast<int>(proxy.backForwardList.forwardList.count);
  }

  // If WebView has not been created, it's fair to say navigation has 0 item.
  return 0;
}

NavigationItem* WKBasedNavigationManagerImpl::GetItemAtIndex(
    size_t index) const {
  DLOG(WARNING) << "Not yet implemented.";
  return nullptr;
}

int WKBasedNavigationManagerImpl::GetIndexOfItem(
    const NavigationItem* item) const {
  DLOG(WARNING) << "Not yet implemented.";
  return -1;
}

int WKBasedNavigationManagerImpl::GetPendingItemIndex() const {
  DLOG(WARNING) << "Not yet implemented.";
  return -1;
}

int WKBasedNavigationManagerImpl::GetLastCommittedItemIndex() const {
  id<CRWWebViewNavigationProxy> proxy = delegate_->GetWebViewNavigationProxy();
  if (proxy.backForwardList.currentItem) {
    return static_cast<int>(proxy.backForwardList.backList.count);
  }
  return -1;
}

bool WKBasedNavigationManagerImpl::RemoveItemAtIndex(int index) {
  DLOG(WARNING) << "Not yet implemented.";
  return true;
}

bool WKBasedNavigationManagerImpl::CanGoBack() const {
  return [delegate_->GetWebViewNavigationProxy() canGoBack];
}

bool WKBasedNavigationManagerImpl::CanGoForward() const {
  return [delegate_->GetWebViewNavigationProxy() canGoForward];
}

bool WKBasedNavigationManagerImpl::CanGoToOffset(int offset) const {
  int index = GetIndexForOffset(offset);
  return index >= 0 && index < GetItemCount();
}

void WKBasedNavigationManagerImpl::GoBack() {
  [delegate_->GetWebViewNavigationProxy() goBack];
}

void WKBasedNavigationManagerImpl::GoForward() {
  [delegate_->GetWebViewNavigationProxy() goForward];
}

void WKBasedNavigationManagerImpl::GoToIndex(int index) {
  DLOG(WARNING) << "Not yet implemented.";
}

NavigationItemList WKBasedNavigationManagerImpl::GetBackwardItems() const {
  DLOG(WARNING) << "Not yet implemented.";
  return NavigationItemList();
}

NavigationItemList WKBasedNavigationManagerImpl::GetForwardItems() const {
  DLOG(WARNING) << "Not yet implemented.";
  return NavigationItemList();
}

void WKBasedNavigationManagerImpl::CopyStateFromAndPrune(
    const NavigationManager* source) {
  DLOG(WARNING) << "Not yet implemented.";
}

bool WKBasedNavigationManagerImpl::CanPruneAllButLastCommittedItem() const {
  DLOG(WARNING) << "Not yet implemented.";
  return true;
}


NavigationItemImpl* WKBasedNavigationManagerImpl::GetNavigationItemImplAtIndex(
    size_t index) const {
  DLOG(WARNING) << "Not yet implemented.";
  return nullptr;
}

int WKBasedNavigationManagerImpl::GetPreviousItemIndex() const {
  DLOG(WARNING) << "Not yet implemented.";
  return -1;
}

}  // namespace web
