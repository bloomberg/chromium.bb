// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/navigation_manager_impl.h"

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

namespace {

// Checks whether or not two URL are an in-page navigation (differing only
// in the fragment).
bool AreURLsInPageNavigation(const GURL& existing_url, const GURL& new_url) {
  if (existing_url == new_url || !new_url.has_ref())
    return false;

  return existing_url.EqualsIgnoringRef(new_url);
}

}  // anonymous namespace

namespace web {

NavigationManager::WebLoadParams::WebLoadParams(const GURL& url)
    : url(url),
      transition_type(ui::PAGE_TRANSITION_LINK),
      user_agent_override_option(UserAgentOverrideOption::INHERIT),
      is_renderer_initiated(false),
      post_data(nil) {}

NavigationManager::WebLoadParams::~WebLoadParams() {}

NavigationManager::WebLoadParams::WebLoadParams(const WebLoadParams& other)
    : url(other.url),
      referrer(other.referrer),
      transition_type(other.transition_type),
      user_agent_override_option(other.user_agent_override_option),
      is_renderer_initiated(other.is_renderer_initiated),
      extra_headers([other.extra_headers copy]),
      post_data([other.post_data copy]) {}

NavigationManager::WebLoadParams& NavigationManager::WebLoadParams::operator=(
    const WebLoadParams& other) {
  url = other.url;
  referrer = other.referrer;
  is_renderer_initiated = other.is_renderer_initiated;
  transition_type = other.transition_type;
  user_agent_override_option = other.user_agent_override_option;
  extra_headers.reset([other.extra_headers copy]);
  post_data.reset([other.post_data copy]);

  return *this;
}

NavigationManagerImpl::NavigationManagerImpl()
    : delegate_(nullptr), browser_state_(nullptr) {}

NavigationManagerImpl::~NavigationManagerImpl() {
  [session_controller_ setNavigationManager:nullptr];
}

void NavigationManagerImpl::SetDelegate(NavigationManagerDelegate* delegate) {
  delegate_ = delegate;
}

void NavigationManagerImpl::SetBrowserState(BrowserState* browser_state) {
  browser_state_ = browser_state;
  [session_controller_ setBrowserState:browser_state];
}

void NavigationManagerImpl::SetSessionController(
    CRWSessionController* session_controller) {
  session_controller_.reset(session_controller);
  [session_controller_ setNavigationManager:this];
}

void NavigationManagerImpl::InitializeSession() {
  SetSessionController(
      [[CRWSessionController alloc] initWithBrowserState:browser_state_]);
}

void NavigationManagerImpl::ReplaceSessionHistory(
    std::vector<std::unique_ptr<web::NavigationItem>> items,
    int lastCommittedItemIndex) {
  SetSessionController([[CRWSessionController alloc]
        initWithBrowserState:browser_state_
             navigationItems:std::move(items)
      lastCommittedItemIndex:lastCommittedItemIndex]);
}

void NavigationManagerImpl::OnNavigationItemsPruned(size_t pruned_item_count) {
  delegate_->OnNavigationItemsPruned(pruned_item_count);
}

void NavigationManagerImpl::OnNavigationItemChanged() {
  delegate_->OnNavigationItemChanged();
}

void NavigationManagerImpl::OnNavigationItemCommitted() {
  LoadCommittedDetails details;
  details.item = GetLastCommittedItem();
  DCHECK(details.item);
  details.previous_item_index = [session_controller_ previousItemIndex];
  if (details.previous_item_index >= 0) {
    DCHECK([session_controller_ previousItem]);
    details.previous_url = [session_controller_ previousItem]->GetURL();
    details.is_in_page =
        AreURLsInPageNavigation(details.previous_url, details.item->GetURL());
  } else {
    details.previous_url = GURL();
    details.is_in_page = NO;
  }

  delegate_->OnNavigationItemCommitted(details);
}

CRWSessionController* NavigationManagerImpl::GetSessionController() {
  return session_controller_;
}

void NavigationManagerImpl::AddTransientItem(const GURL& url) {
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

void NavigationManagerImpl::AddPendingItem(
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

  // Set the user agent type for web URLs.
  NavigationItem* pending_item = GetPendingItem();
  if (!pending_item)
    return;

  // |user_agent_override_option| must be INHERIT if |pending_item|'s
  // UserAgentType is NONE, as requesting a desktop or mobile user agent should
  // be disabled for app-specific URLs.
  DCHECK(pending_item->GetUserAgentType() != UserAgentType::NONE ||
         user_agent_override_option == UserAgentOverrideOption::INHERIT);

  // Newly created pending items are created with UserAgentType::NONE for native
  // pages or UserAgentType::MOBILE for non-native pages.  If the pending item's
  // URL is non-native, check which user agent type it should be created with
  // based on |user_agent_override_option|.
  DCHECK_NE(UserAgentType::DESKTOP, pending_item->GetUserAgentType());
  if (pending_item->GetUserAgentType() == UserAgentType::NONE)
    return;

  switch (user_agent_override_option) {
    case UserAgentOverrideOption::DESKTOP:
      pending_item->SetUserAgentType(UserAgentType::DESKTOP);
      break;
    case UserAgentOverrideOption::MOBILE:
      pending_item->SetUserAgentType(UserAgentType::MOBILE);
      break;
    case UserAgentOverrideOption::INHERIT: {
      // Propagate the last committed non-native item's UserAgentType if there
      // is one, otherwise keep the default value, which is mobile.
      NavigationItem* last_non_native_item =
          GetLastCommittedNonAppSpecificItem();
      DCHECK(!last_non_native_item ||
             last_non_native_item->GetUserAgentType() != UserAgentType::NONE);
      if (last_non_native_item) {
        pending_item->SetUserAgentType(
            last_non_native_item->GetUserAgentType());
      }
      break;
    }
  }
}

void NavigationManagerImpl::CommitPendingItem() {
  [session_controller_ commitPendingItem];
}

BrowserState* NavigationManagerImpl::GetBrowserState() const {
  return browser_state_;
}

WebState* NavigationManagerImpl::GetWebState() const {
  return delegate_->GetWebState();
}

NavigationItem* NavigationManagerImpl::GetVisibleItem() const {
  return [session_controller_ visibleItem];
}

NavigationItem* NavigationManagerImpl::GetLastCommittedItem() const {
  return [session_controller_ lastCommittedItem];
}

NavigationItem* NavigationManagerImpl::GetPendingItem() const {
  return [session_controller_ pendingItem];
}

NavigationItem* NavigationManagerImpl::GetTransientItem() const {
  return [session_controller_ transientItem];
}

void NavigationManagerImpl::DiscardNonCommittedItems() {
  [session_controller_ discardNonCommittedItems];
}

void NavigationManagerImpl::LoadURLWithParams(
    const NavigationManager::WebLoadParams& params) {
  delegate_->LoadURLWithParams(params);
}

void NavigationManagerImpl::AddTransientURLRewriter(
    BrowserURLRewriter::URLRewriter rewriter) {
  DCHECK(rewriter);
  if (!transient_url_rewriters_) {
    transient_url_rewriters_.reset(
        new std::vector<BrowserURLRewriter::URLRewriter>());
  }
  transient_url_rewriters_->push_back(rewriter);
}

int NavigationManagerImpl::GetItemCount() const {
  return [session_controller_ items].size();
}

NavigationItem* NavigationManagerImpl::GetItemAtIndex(size_t index) const {
  return [session_controller_ itemAtIndex:index];
}

int NavigationManagerImpl::GetIndexOfItem(
    const web::NavigationItem* item) const {
  return [session_controller_ indexOfItem:item];
}

int NavigationManagerImpl::GetPendingItemIndex() const {
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

int NavigationManagerImpl::GetLastCommittedItemIndex() const {
  if (GetItemCount() == 0)
    return -1;
  return [session_controller_ lastCommittedItemIndex];
}

bool NavigationManagerImpl::RemoveItemAtIndex(int index) {
  if (index == GetLastCommittedItemIndex() || index == GetPendingItemIndex())
    return false;

  if (index < 0 || index >= GetItemCount())
    return false;

  [session_controller_ removeItemAtIndex:index];
  return true;
}

bool NavigationManagerImpl::CanGoBack() const {
  return CanGoToOffset(-1);
}

bool NavigationManagerImpl::CanGoForward() const {
  return CanGoToOffset(1);
}

bool NavigationManagerImpl::CanGoToOffset(int offset) const {
  int index = GetIndexForOffset(offset);
  return 0 <= index && index < GetItemCount();
}

void NavigationManagerImpl::GoBack() {
  delegate_->GoToIndex(GetIndexForOffset(-1));
}

void NavigationManagerImpl::GoForward() {
  delegate_->GoToIndex(GetIndexForOffset(1));
}

void NavigationManagerImpl::GoToIndex(int index) {
  delegate_->GoToIndex(index);
}

NavigationItemList NavigationManagerImpl::GetBackwardItems() const {
  return [session_controller_ backwardItems];
}

NavigationItemList NavigationManagerImpl::GetForwardItems() const {
  return [session_controller_ forwardItems];
}

void NavigationManagerImpl::Reload(ReloadType reload_type,
                                   bool check_for_reposts) {
  if (!GetTransientItem() && !GetPendingItem() && !GetLastCommittedItem())
    return;

  // Reload with ORIGINAL_REQUEST_URL type should reload with the original
  // request url of the transient item, or pending item if transient doesn't
  // exist, or last committed item if both of them don't exist. The reason is
  // that a server side redirect may change the item's url.
  // For example, the user visits www.chromium.org and is then redirected
  // to m.chromium.org, when the user wants to refresh the page with a different
  // configuration (e.g. user agent), the user would be expecting to visit
  // www.chromium.org instead of m.chromium.org.
  if (reload_type == web::ReloadType::ORIGINAL_REQUEST_URL) {
    NavigationItem* reload_item = nullptr;
    if (GetTransientItem())
      reload_item = GetTransientItem();
    else if (GetPendingItem())
      reload_item = GetPendingItem();
    else
      reload_item = GetLastCommittedItem();
    DCHECK(reload_item);

    reload_item->SetURL(reload_item->GetOriginalRequestURL());
  }

  delegate_->Reload();
}

void NavigationManagerImpl::CopyStateFromAndPrune(
    const NavigationManager* manager) {
  DCHECK(manager);
  CRWSessionController* other_session =
      static_cast<const NavigationManagerImpl*>(manager)->session_controller_;
  [session_controller_ copyStateFromSessionControllerAndPrune:other_session];
}

bool NavigationManagerImpl::CanPruneAllButLastCommittedItem() const {
  return [session_controller_ canPruneAllButLastCommittedItem];
}

std::unique_ptr<std::vector<BrowserURLRewriter::URLRewriter>>
NavigationManagerImpl::GetTransientURLRewriters() {
  return std::move(transient_url_rewriters_);
}

void NavigationManagerImpl::RemoveTransientURLRewriters() {
  transient_url_rewriters_.reset();
}

int NavigationManagerImpl::GetIndexForOffset(int offset) const {
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

bool NavigationManagerImpl::IsRedirectItemAtIndex(int index) const {
  DCHECK_GE(index, 0);
  DCHECK_LT(index, GetItemCount());
  ui::PageTransition transition = GetItemAtIndex(index)->GetTransitionType();
  return transition & ui::PAGE_TRANSITION_IS_REDIRECT_MASK;
}

NavigationItem* NavigationManagerImpl::GetLastCommittedNonAppSpecificItem()
    const {
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

}  // namespace web
