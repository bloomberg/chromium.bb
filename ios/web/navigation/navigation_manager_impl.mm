// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/navigation/navigation_manager_impl.h"

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

CRWSessionController* NavigationManagerImpl::GetSessionController() {
  return session_controller_;
}

void NavigationManagerImpl::SetSessionController(
    CRWSessionController* session_controller) {
  session_controller_.reset([session_controller retain]);
  [session_controller_ setNavigationManager:this];
}

void NavigationManagerImpl::InitializeSession(NSString* window_name,
                                              NSString* opener_id,
                                              BOOL opened_by_dom,
                                              int opener_navigation_index) {
  SetSessionController([[[CRWSessionController alloc]
         initWithWindowName:window_name
                   openerId:opener_id
                openedByDOM:opened_by_dom
      openerNavigationIndex:opener_navigation_index
               browserState:browser_state_] autorelease]);
}

void NavigationManagerImpl::ReplaceSessionHistory(
    ScopedVector<web::NavigationItem> items,
    int current_index) {
  SetSessionController([[[CRWSessionController alloc]
      initWithNavigationItems:items.Pass()
                 currentIndex:current_index
                 browserState:browser_state_] autorelease]);
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

NavigationItem* NavigationManagerImpl::GetVisibleItem() const {
  CRWSessionEntry* entry = [session_controller_ visibleEntry];
  return [entry navigationItem];
}

NavigationItem* NavigationManagerImpl::GetPendingItem() const {
  return [[session_controller_ pendingEntry] navigationItem];
}

void NavigationManagerImpl::DiscardNonCommittedItems() {
  [session_controller_ discardNonCommittedEntries];
}

void NavigationManagerImpl::LoadIfNecessary() {
  // Nothing to do; iOS loads lazily.
}

NavigationItem* NavigationManagerImpl::GetTransientItem() const {
  return [[session_controller_ transientEntry] navigationItem];
}

NavigationItem* NavigationManagerImpl::GetLastCommittedItem() const {
  CRWSessionEntry* entry = [session_controller_ lastCommittedEntry];
  return [entry navigationItem];
}

int NavigationManagerImpl::GetLastCommittedEntryIndex() const {
  if (![[session_controller_ entries] count])
    return -1;
  return [session_controller_ currentNavigationIndex];
}

bool NavigationManagerImpl::RemoveEntryAtIndex(int index) {
  if (index == GetLastCommittedEntryIndex() || index == GetPendingItemIndex())
    return false;

  NSUInteger idx = static_cast<NSUInteger>(index);
  NSArray* entries = [session_controller_ entries];
  if (idx >= entries.count)
    return false;

  [session_controller_ removeEntryAtIndex:index];
  return true;
}

NavigationItem* NavigationManagerImpl::GetLastUserItem() const {
  CRWSessionEntry* entry = [session_controller_ lastUserEntry];
  return [entry navigationItem];
}

NavigationItem* NavigationManagerImpl::GetPreviousItem() const {
  CRWSessionEntry* entry = [session_controller_ previousEntry];
  return [entry navigationItem];
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

int NavigationManagerImpl::GetEntryCount() const {
  return [[session_controller_ entries] count];
}

NavigationItem* NavigationManagerImpl::GetItemAtIndex(size_t index) const {
  NSArray* entries = [session_controller_ entries];
  return index < entries.count ? [entries[index] navigationItem] : nullptr;
}

int NavigationManagerImpl::GetCurrentEntryIndex() const {
  return [session_controller_ currentNavigationIndex];
}

int NavigationManagerImpl::GetPendingItemIndex() const {
  if ([session_controller_ hasPendingEntry])
    return GetCurrentEntryIndex();
  return -1;
}

scoped_ptr<std::vector<BrowserURLRewriter::URLRewriter>>
NavigationManagerImpl::GetTransientURLRewriters() {
  return transient_url_rewriters_.Pass();
}

void NavigationManagerImpl::RemoveTransientURLRewriters() {
  transient_url_rewriters_.reset();
}

void NavigationManagerImpl::LoadURL(const GURL& url,
                                    const web::Referrer& referrer,
                                    ui::PageTransition type) {
  WebState::OpenURLParams params(url, referrer, CURRENT_TAB, type, NO);
  delegate_->GetWebState()->OpenURL(params);
}

bool NavigationManagerImpl::CanGoBack() const {
  return [session_controller_ canGoBack];
}

bool NavigationManagerImpl::CanGoForward() const {
  return [session_controller_ canGoForward];
}

void NavigationManagerImpl::GoBack() {
  if (CanGoBack()) {
    [session_controller_ goBack];
    // Signal the delegate to load the old page.
    delegate_->NavigateToPendingEntry();
  }
}

void NavigationManagerImpl::GoForward() {
  if (CanGoForward()) {
    [session_controller_ goForward];
    // Signal the delegate to load the new page.
    delegate_->NavigateToPendingEntry();
  }
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

void NavigationManagerImpl::CopyState(
    NavigationManagerImpl* navigation_manager) {
  SetSessionController(
      [[navigation_manager->GetSessionController() copy] autorelease]);
}

}  // namespace web

