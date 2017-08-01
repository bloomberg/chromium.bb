// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/legacy_navigation_manager_impl.h"

#include <stddef.h>

#include <utility>

#include "base/logging.h"
#import "ios/web/navigation/crw_session_controller+private_constructors.h"
#import "ios/web/navigation/crw_session_controller.h"
#import "ios/web/navigation/navigation_item_impl.h"
#import "ios/web/navigation/navigation_item_impl_list.h"
#import "ios/web/navigation/navigation_manager_delegate.h"
#include "ios/web/public/load_committed_details.h"
#import "ios/web/public/navigation_item.h"
#include "ios/web/public/reload_type.h"
#import "ios/web/public/web_client.h"
#import "ios/web/public/web_state/web_state.h"
#include "ui/base/page_transition_types.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

LegacyNavigationManagerImpl::LegacyNavigationManagerImpl() = default;

LegacyNavigationManagerImpl::~LegacyNavigationManagerImpl() {
  [session_controller_ setNavigationManager:nullptr];
}

void LegacyNavigationManagerImpl::SetBrowserState(BrowserState* browser_state) {
  NavigationManagerImpl::SetBrowserState(browser_state);
  [session_controller_ setBrowserState:browser_state];
}

void LegacyNavigationManagerImpl::SetSessionController(
    CRWSessionController* session_controller) {
  session_controller_.reset(session_controller);
  [session_controller_ setNavigationManager:this];
}

void LegacyNavigationManagerImpl::InitializeSession() {
  SetSessionController(
      [[CRWSessionController alloc] initWithBrowserState:browser_state_]);
}

void LegacyNavigationManagerImpl::ReplaceSessionHistory(
    std::vector<std::unique_ptr<web::NavigationItem>> items,
    int lastCommittedItemIndex) {
  SetSessionController([[CRWSessionController alloc]
        initWithBrowserState:browser_state_
             navigationItems:std::move(items)
      lastCommittedItemIndex:lastCommittedItemIndex]);
}

void LegacyNavigationManagerImpl::OnNavigationItemsPruned(
    size_t pruned_item_count) {
  delegate_->OnNavigationItemsPruned(pruned_item_count);
}

void LegacyNavigationManagerImpl::OnNavigationItemChanged() {
  delegate_->OnNavigationItemChanged();
}

void LegacyNavigationManagerImpl::OnNavigationItemCommitted() {
  LoadCommittedDetails details;
  details.item = GetLastCommittedItem();
  DCHECK(details.item);
  details.previous_item_index = [session_controller_ previousItemIndex];
  if (details.previous_item_index >= 0) {
    DCHECK([session_controller_ previousItem]);
    details.previous_url = [session_controller_ previousItem]->GetURL();
    details.is_in_page = IsFragmentChangeNavigationBetweenUrls(
        details.previous_url, details.item->GetURL());
  } else {
    details.previous_url = GURL();
    details.is_in_page = NO;
  }

  delegate_->OnNavigationItemCommitted(details);
}

CRWSessionController* LegacyNavigationManagerImpl::GetSessionController()
    const {
  return session_controller_;
}

void LegacyNavigationManagerImpl::AddTransientItem(const GURL& url) {
  [session_controller_ addTransientItemWithURL:url];

  // TODO(crbug.com/676129): Transient item is only supposed to be added for
  // pending non-app-specific loads, but pending item can be null because of the
  // bug. The workaround should be removed once the bug is fixed.
  NavigationItem* item = GetPendingItem();
  if (!item)
    item = GetLastCommittedNonAppSpecificItem();
  DCHECK(item->GetUserAgentType() != UserAgentType::NONE);
  GetTransientItem()->SetUserAgentType(item->GetUserAgentType());
}

void LegacyNavigationManagerImpl::AddPendingItem(
    const GURL& url,
    const web::Referrer& referrer,
    ui::PageTransition navigation_type,
    NavigationInitiationType initiation_type,
    UserAgentOverrideOption user_agent_override_option) {
  [session_controller_ addPendingItem:url
                             referrer:referrer
                           transition:navigation_type
                       initiationType:initiation_type
              userAgentOverrideOption:user_agent_override_option];

  if (!GetPendingItem()) {
    return;
  }

  UpdatePendingItemUserAgentType(user_agent_override_option,
                                 GetLastCommittedNonAppSpecificItem(),
                                 GetPendingItem());
}

void LegacyNavigationManagerImpl::CommitPendingItem() {
  [session_controller_ commitPendingItem];
}

BrowserState* LegacyNavigationManagerImpl::GetBrowserState() const {
  return browser_state_;
}

WebState* LegacyNavigationManagerImpl::GetWebState() const {
  return delegate_->GetWebState();
}

NavigationItem* LegacyNavigationManagerImpl::GetVisibleItem() const {
  return [session_controller_ visibleItem];
}

NavigationItem* LegacyNavigationManagerImpl::GetLastCommittedItem() const {
  return [session_controller_ lastCommittedItem];
}

NavigationItem* LegacyNavigationManagerImpl::GetPendingItem() const {
  return [session_controller_ pendingItem];
}

NavigationItem* LegacyNavigationManagerImpl::GetTransientItem() const {
  return [session_controller_ transientItem];
}

void LegacyNavigationManagerImpl::DiscardNonCommittedItems() {
  [session_controller_ discardNonCommittedItems];
}

void LegacyNavigationManagerImpl::LoadURLWithParams(
    const NavigationManager::WebLoadParams& params) {
  delegate_->LoadURLWithParams(params);
}

int LegacyNavigationManagerImpl::GetItemCount() const {
  return [session_controller_ items].size();
}

NavigationItem* LegacyNavigationManagerImpl::GetItemAtIndex(
    size_t index) const {
  return GetNavigationItemImplAtIndex(index);
}

NavigationItemImpl* LegacyNavigationManagerImpl::GetNavigationItemImplAtIndex(
    size_t index) const {
  return [session_controller_ itemAtIndex:index];
}

int LegacyNavigationManagerImpl::GetIndexOfItem(
    const web::NavigationItem* item) const {
  return [session_controller_ indexOfItem:item];
}

int LegacyNavigationManagerImpl::GetPendingItemIndex() const {
  if (GetPendingItem()) {
    if ([session_controller_ pendingItemIndex] != -1) {
      return [session_controller_ pendingItemIndex];
    }
    // TODO(crbug.com/665189): understand why last committed item index is
    // returned here.
    return GetLastCommittedItemIndex();
  }
  return -1;
}

int LegacyNavigationManagerImpl::GetLastCommittedItemIndex() const {
  if (GetItemCount() == 0)
    return -1;
  return [session_controller_ lastCommittedItemIndex];
}

bool LegacyNavigationManagerImpl::RemoveItemAtIndex(int index) {
  if (index == GetLastCommittedItemIndex() || index == GetPendingItemIndex())
    return false;

  if (index < 0 || index >= GetItemCount())
    return false;

  [session_controller_ removeItemAtIndex:index];
  return true;
}

bool LegacyNavigationManagerImpl::CanGoBack() const {
  return CanGoToOffset(-1);
}

bool LegacyNavigationManagerImpl::CanGoForward() const {
  return CanGoToOffset(1);
}

bool LegacyNavigationManagerImpl::CanGoToOffset(int offset) const {
  int index = GetIndexForOffset(offset);
  return 0 <= index && index < GetItemCount();
}

void LegacyNavigationManagerImpl::GoBack() {
  delegate_->GoToIndex(GetIndexForOffset(-1));
}

void LegacyNavigationManagerImpl::GoForward() {
  delegate_->GoToIndex(GetIndexForOffset(1));
}

void LegacyNavigationManagerImpl::GoToIndex(int index) {
  delegate_->GoToIndex(index);
}

NavigationItemList LegacyNavigationManagerImpl::GetBackwardItems() const {
  return [session_controller_ backwardItems];
}

NavigationItemList LegacyNavigationManagerImpl::GetForwardItems() const {
  return [session_controller_ forwardItems];
}

void LegacyNavigationManagerImpl::CopyStateFromAndPrune(
    const NavigationManager* manager) {
  DCHECK(manager);
  CRWSessionController* other_session =
      static_cast<const NavigationManagerImpl*>(manager)
          ->GetSessionController();
  [session_controller_ copyStateFromSessionControllerAndPrune:other_session];
}

bool LegacyNavigationManagerImpl::CanPruneAllButLastCommittedItem() const {
  return [session_controller_ canPruneAllButLastCommittedItem];
}

int LegacyNavigationManagerImpl::GetIndexForOffset(int offset) const {
  int result = [session_controller_ pendingItemIndex] == -1
                   ? GetLastCommittedItemIndex()
                   : static_cast<int>([session_controller_ pendingItemIndex]);

  if (offset < 0) {
    if (GetTransientItem() && [session_controller_ pendingItemIndex] == -1) {
      // Going back from transient item that added to the end navigation stack
      // is a matter of discarding it as there is no need to move navigation
      // index back.
      offset++;
    }

    while (offset < 0 && result > 0) {
      // To stop the user getting 'stuck' on redirecting pages they weren't
      // even aware existed, it is necessary to pass over pages that would
      // immediately result in a redirect (the item *before* the redirected
      // page).
      while (result > 0 && IsRedirectItemAtIndex(result)) {
        --result;
      }
      --result;
      ++offset;
    }
    // Result may be out of bounds, so stop trying to skip redirect items and
    // simply add the remainder.
    result += offset;
    if (result > GetItemCount() /* overflow */)
      result = INT_MIN;
  } else if (offset > 0) {
    while (offset > 0 && result < GetItemCount()) {
      ++result;
      --offset;
      // As with going back, skip over redirects.
      while (result + 1 < GetItemCount() && IsRedirectItemAtIndex(result + 1)) {
        ++result;
      }
    }
    // Result may be out of bounds, so stop trying to skip redirect items and
    // simply add the remainder.
    result += offset;
    if (result < 0 /* overflow */)
      result = INT_MAX;
  }

  return result;
}

bool LegacyNavigationManagerImpl::IsRedirectItemAtIndex(int index) const {
  DCHECK_GE(index, 0);
  DCHECK_LT(index, GetItemCount());
  ui::PageTransition transition = GetItemAtIndex(index)->GetTransitionType();
  return transition & ui::PAGE_TRANSITION_IS_REDIRECT_MASK;
}

NavigationItem*
LegacyNavigationManagerImpl::GetLastCommittedNonAppSpecificItem() const {
  int index = GetLastCommittedItemIndex();
  if (index == -1)
    return nullptr;
  WebClient* client = GetWebClient();
  const ScopedNavigationItemImplList& items = [session_controller_ items];
  while (index >= 0) {
    NavigationItem* item = items[index--].get();
    if (!client->IsAppSpecificURL(item->GetVirtualURL()))
      return item;
  }
  return nullptr;
}

int LegacyNavigationManagerImpl::GetPreviousItemIndex() const {
  return base::checked_cast<int>([session_controller_ previousItemIndex]);
}

}  // namespace web
