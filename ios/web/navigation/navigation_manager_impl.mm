// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/navigation/navigation_manager_impl.h"

#include <stddef.h>

#include <utility>

#include "base/logging.h"
#import "ios/web/navigation/crw_session_controller+private_constructors.h"
#import "ios/web/navigation/crw_session_controller.h"
#import "ios/web/navigation/crw_session_entry.h"
#include "ios/web/navigation/navigation_item_impl.h"
#include "ios/web/navigation/navigation_manager_delegate.h"
#import "ios/web/navigation/navigation_manager_facade_delegate.h"
#include "ios/web/public/load_committed_details.h"
#include "ios/web/public/navigation_item.h"
#include "ios/web/public/web_state/web_state.h"
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

  url::Replacements<char> replacements;
  replacements.ClearRef();
  return existing_url.ReplaceComponents(replacements) ==
         new_url.ReplaceComponents(replacements);
}

}  // anonymous namespace

namespace web {

NavigationManager::WebLoadParams::WebLoadParams(const GURL& url)
    : url(url),
      transition_type(ui::PAGE_TRANSITION_LINK),
      is_renderer_initiated(false),
      post_data(nil) {}

NavigationManager::WebLoadParams::~WebLoadParams() {}

NavigationManager::WebLoadParams::WebLoadParams(const WebLoadParams& other)
    : url(other.url),
      referrer(other.referrer),
      transition_type(other.transition_type),
      is_renderer_initiated(other.is_renderer_initiated),
      extra_headers([other.extra_headers copy]),
      post_data([other.post_data copy]) {}

NavigationManager::WebLoadParams& NavigationManager::WebLoadParams::operator=(
    const WebLoadParams& other) {
  url = other.url;
  referrer = other.referrer;
  is_renderer_initiated = other.is_renderer_initiated;
  transition_type = other.transition_type;
  extra_headers.reset([other.extra_headers copy]);
  post_data.reset([other.post_data copy]);

  return *this;
}

NavigationManagerImpl::NavigationManagerImpl(
    NavigationManagerDelegate* delegate,
    BrowserState* browser_state)
    : delegate_(delegate),
      browser_state_(browser_state),
      facade_delegate_(nullptr) {
  DCHECK(browser_state_);
}

NavigationManagerImpl::~NavigationManagerImpl() {
  // The facade layer should be deleted before this object.
  DCHECK(!facade_delegate_);

  [session_controller_ setNavigationManager:nullptr];
}

void NavigationManagerImpl::SetSessionController(
    CRWSessionController* session_controller) {
  session_controller_.reset(session_controller);
  [session_controller_ setNavigationManager:this];
}

void NavigationManagerImpl::InitializeSession(NSString* window_name,
                                              NSString* opener_id,
                                              BOOL opened_by_dom,
                                              int opener_navigation_index) {
  SetSessionController([[CRWSessionController alloc]
         initWithWindowName:window_name
                   openerId:opener_id
                openedByDOM:opened_by_dom
      openerNavigationIndex:opener_navigation_index
               browserState:browser_state_]);
}

void NavigationManagerImpl::ReplaceSessionHistory(
    ScopedVector<web::NavigationItem> items,
    int current_index) {
  SetSessionController([[CRWSessionController alloc]
      initWithNavigationItems:std::move(items)
                 currentIndex:current_index
                 browserState:browser_state_]);
}

void NavigationManagerImpl::SetFacadeDelegate(
    NavigationManagerFacadeDelegate* facade_delegate) {
  facade_delegate_ = facade_delegate;
}

NavigationManagerFacadeDelegate* NavigationManagerImpl::GetFacadeDelegate()
    const {
  return facade_delegate_;
}

void NavigationManagerImpl::OnNavigationItemsPruned(size_t pruned_item_count) {
  delegate_->OnNavigationItemsPruned(pruned_item_count);

  if (facade_delegate_)
    facade_delegate_->OnNavigationItemsPruned(pruned_item_count);
}

void NavigationManagerImpl::OnNavigationItemChanged() {
  delegate_->OnNavigationItemChanged();

  if (facade_delegate_)
    facade_delegate_->OnNavigationItemChanged();
}

void NavigationManagerImpl::OnNavigationItemCommitted() {
  LoadCommittedDetails details;
  details.item = GetLastCommittedItem();
  DCHECK(details.item);
  details.previous_item_index = [session_controller_ previousNavigationIndex];
  if (details.previous_item_index >= 0) {
    DCHECK([session_controller_ previousEntry]);
    details.previous_url =
        [session_controller_ previousEntry].navigationItem->GetURL();
    details.is_in_page =
        AreURLsInPageNavigation(details.previous_url, details.item->GetURL());
  } else {
    details.previous_url = GURL();
    details.is_in_page = NO;
  }

  delegate_->OnNavigationItemCommitted(details);

  if (facade_delegate_) {
    facade_delegate_->OnNavigationItemCommitted(details.previous_item_index,
                                                details.is_in_page);
  }
}

CRWSessionController* NavigationManagerImpl::GetSessionController() {
  return session_controller_;
}

void NavigationManagerImpl::LoadURL(const GURL& url,
                                    const web::Referrer& referrer,
                                    ui::PageTransition type) {
  WebState::OpenURLParams params(url, referrer,
                                 WindowOpenDisposition::CURRENT_TAB, type, NO);
  delegate_->GetWebState()->OpenURL(params);
}

NavigationItem* NavigationManagerImpl::GetLastUserItem() const {
  CRWSessionEntry* entry = [session_controller_ lastUserEntry];
  return [entry navigationItem];
}

NavigationItem* NavigationManagerImpl::GetPreviousItem() const {
  CRWSessionEntry* entry = [session_controller_ previousEntry];
  return [entry navigationItem];
}

std::vector<NavigationItem*> NavigationManagerImpl::GetItems() {
  std::vector<NavigationItem*> items;
  size_t i = 0;
  items.resize([session_controller_ entries].count);
  for (CRWSessionEntry* entry in [session_controller_ entries]) {
    items[i++] = entry.navigationItem;
  }
  return items;
}

BrowserState* NavigationManagerImpl::GetBrowserState() const {
  return browser_state_;
}

WebState* NavigationManagerImpl::GetWebState() const {
  return delegate_->GetWebState();
}

NavigationItem* NavigationManagerImpl::GetVisibleItem() const {
  CRWSessionEntry* entry = [session_controller_ visibleEntry];
  return [entry navigationItem];
}

NavigationItem* NavigationManagerImpl::GetLastCommittedItem() const {
  CRWSessionEntry* entry = [session_controller_ lastCommittedEntry];
  return [entry navigationItem];
}

NavigationItem* NavigationManagerImpl::GetPendingItem() const {
  return [[session_controller_ pendingEntry] navigationItem];
}

NavigationItem* NavigationManagerImpl::GetTransientItem() const {
  return [[session_controller_ transientEntry] navigationItem];
}

void NavigationManagerImpl::DiscardNonCommittedItems() {
  [session_controller_ discardNonCommittedEntries];
}

void NavigationManagerImpl::LoadIfNecessary() {
  // Nothing to do; iOS loads lazily.
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
  return [[session_controller_ entries] count];
}

NavigationItem* NavigationManagerImpl::GetItemAtIndex(size_t index) const {
  NSArray* entries = [session_controller_ entries];
  return index < entries.count ? [entries[index] navigationItem] : nullptr;
}

int NavigationManagerImpl::GetCurrentItemIndex() const {
  return [session_controller_ currentNavigationIndex];
}

int NavigationManagerImpl::GetPendingItemIndex() const {
  if ([session_controller_ hasPendingEntry]) {
    if ([session_controller_ pendingEntryIndex] != -1) {
      return [session_controller_ pendingEntryIndex];
    }
    // TODO(crbug.com/665189): understand why current item index is
    // returned here.
    return GetCurrentItemIndex();
  }
  return -1;
}

int NavigationManagerImpl::GetLastCommittedItemIndex() const {
  if (![[session_controller_ entries] count])
    return -1;
  return [session_controller_ currentNavigationIndex];
}

bool NavigationManagerImpl::RemoveItemAtIndex(int index) {
  if (index == GetLastCommittedItemIndex() || index == GetPendingItemIndex())
    return false;

  NSUInteger idx = static_cast<NSUInteger>(index);
  NSArray* entries = [session_controller_ entries];
  if (idx >= entries.count)
    return false;

  [session_controller_ removeEntryAtIndex:index];
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

void NavigationManagerImpl::Reload(bool check_for_reposts) {
  // Navigation manager may be empty if the only pending entry failed to load
  // with SSL error and the user has decided not to proceed.
  NavigationItem* item = GetVisibleItem();
  GURL url = item ? item->GetURL() : GURL(url::kAboutBlankURL);
  web::Referrer referrer = item ? item->GetReferrer() : web::Referrer();

  WebState::OpenURLParams params(url, referrer,
                                 WindowOpenDisposition::CURRENT_TAB,
                                 ui::PAGE_TRANSITION_RELOAD, NO);
  delegate_->GetWebState()->OpenURL(params);
}

std::unique_ptr<std::vector<BrowserURLRewriter::URLRewriter>>
NavigationManagerImpl::GetTransientURLRewriters() {
  return std::move(transient_url_rewriters_);
}

void NavigationManagerImpl::RemoveTransientURLRewriters() {
  transient_url_rewriters_.reset();
}

void NavigationManagerImpl::CopyState(
    NavigationManagerImpl* navigation_manager) {
  SetSessionController([navigation_manager->GetSessionController() copy]);
}

int NavigationManagerImpl::GetIndexForOffset(int offset) const {
  int result = [session_controller_ pendingEntryIndex] == -1
                   ? GetCurrentItemIndex()
                   : static_cast<int>([session_controller_ pendingEntryIndex]);

  if (offset < 0) {
    if (GetTransientItem()) {
      // Going back from transient entry is a matter of discarding it and there
      // is no need to move navigation index back.
      offset++;
    }

    while (offset < 0 && result > 0) {
      // To stop the user getting 'stuck' on redirecting pages they weren't
      // even aware existed, it is necessary to pass over pages that would
      // immediately result in a redirect (the entry *before* the redirected
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
    if (GetPendingItem() && [session_controller_ pendingEntryIndex] == -1) {
      // Chrome for iOS does not allow forward navigation if there is another
      // pending navigation in progress. Returning invalid index indicates that
      // forward navigation will not be allowed (and |INT_MAX| works for that).
      // This is different from other platforms which allow forward navigation
      // if pending entry exist.
      // TODO(crbug.com/661858): Remove this once back-forward navigation uses
      // pending index.
      return INT_MAX;
    }
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
  DCHECK_GT(index, 0);
  DCHECK_LT(index, GetItemCount());
  ui::PageTransition transition = GetItemAtIndex(index)->GetTransitionType();
  return transition & ui::PAGE_TRANSITION_IS_REDIRECT_MASK;
}

}  // namespace web
