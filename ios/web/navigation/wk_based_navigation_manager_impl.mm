// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/wk_based_navigation_manager_impl.h"

#import <Foundation/Foundation.h>
#include <memory>

#include "base/logging.h"
#include "base/mac/bundle_locations.h"
#include "base/memory/ptr_util.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/web/navigation/crw_navigation_item_holder.h"
#import "ios/web/navigation/navigation_item_impl.h"
#include "ios/web/navigation/navigation_item_impl_list.h"
#import "ios/web/navigation/navigation_manager_delegate.h"
#include "ios/web/navigation/placeholder_navigation_util.h"
#include "ios/web/navigation/wk_based_restore_session_util.h"
#include "ios/web/public/load_committed_details.h"
#import "ios/web/public/navigation_item.h"
#import "ios/web/public/web_client.h"
#import "ios/web/web_state/ui/crw_web_view_navigation_proxy.h"
#import "net/base/mac/url_conversions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@class CRWSessionController;

namespace {

void SetNavigationItemInWKItem(WKBackForwardListItem* wk_item,
                               std::unique_ptr<web::NavigationItemImpl> item) {
  DCHECK(wk_item);
  [[CRWNavigationItemHolder holderForBackForwardListItem:wk_item]
      setNavigationItem:std::move(item)];
}

web::NavigationItemImpl* GetNavigationItemFromWKItem(
    WKBackForwardListItem* wk_item) {
  return wk_item ? [[CRWNavigationItemHolder
                       holderForBackForwardListItem:wk_item] navigationItem]
                 : nullptr;
}

}  // namespace

namespace web {

WKBasedNavigationManagerImpl::WKBasedNavigationManagerImpl()
    : pending_item_index_(-1),
      previous_item_index_(-1),
      last_committed_item_index_(-1) {}

WKBasedNavigationManagerImpl::~WKBasedNavigationManagerImpl() = default;

void WKBasedNavigationManagerImpl::SetSessionController(
    CRWSessionController* session_controller) {}

void WKBasedNavigationManagerImpl::InitializeSession() {}

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
    details.is_in_page = IsFragmentChangeNavigationBetweenUrls(
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
  NavigationItem* last_committed_item = GetLastCommittedItem();
  transient_item_ = CreateNavigationItemWithRewriters(
      url, Referrer(), ui::PAGE_TRANSITION_CLIENT_REDIRECT,
      NavigationInitiationType::USER_INITIATED,
      last_committed_item ? last_committed_item->GetURL() : GURL::EmptyGURL(),
      nullptr /* use default rewriters only */);
  transient_item_->SetTimestamp(
      time_smoother_.GetSmoothedTime(base::Time::Now()));

  // Transient item is only supposed to be added for pending non-app-specific
  // navigations.
  NavigationItem* pending_item = GetPendingItem();
  DCHECK(pending_item->GetUserAgentType() != UserAgentType::NONE);
  transient_item_->SetUserAgentType(pending_item->GetUserAgentType());
}

void WKBasedNavigationManagerImpl::AddPendingItem(
    const GURL& url,
    const web::Referrer& referrer,
    ui::PageTransition navigation_type,
    NavigationInitiationType initiation_type,
    UserAgentOverrideOption user_agent_override_option) {
  DiscardNonCommittedItems();

  NavigationItem* last_committed_item = GetLastCommittedItem();
  pending_item_ = CreateNavigationItemWithRewriters(
      url, referrer, navigation_type, initiation_type,
      last_committed_item ? last_committed_item->GetURL() : GURL::EmptyGURL(),
      &transient_url_rewriters_);
  RemoveTransientURLRewriters();
  // Ignore URL rewrite if this is a placeholder URL
  if (placeholder_navigation_util::IsPlaceholderUrl(url)) {
    pending_item_->SetURL(url);
  }
  UpdatePendingItemUserAgentType(user_agent_override_option,
                                 GetLastCommittedNonAppSpecificItem(),
                                 pending_item_.get());

  // AddPendingItem is called no later than |didCommitNavigation|. The only time
  // when all three of WKWebView's URL, the pending URL and WKBackForwardList's
  // current item URL are identical before |didCommitNavigation| is when the
  // in-progress navigation is a back-forward navigation. In this case, current
  // item has already been updated to point to the new location in back-forward
  // history, so pending item index should be set to the current item index.
  id<CRWWebViewNavigationProxy> proxy = delegate_->GetWebViewNavigationProxy();
  WKBackForwardListItem* current_wk_item = proxy.backForwardList.currentItem;
  GURL current_item_url = net::GURLWithNSURL(current_wk_item.URL);
  if (proxy.backForwardList.currentItem &&
      current_item_url == net::GURLWithNSURL(proxy.URL) &&
      current_item_url == pending_item_->GetURL()) {
    pending_item_index_ = GetWKCurrentItemIndex();

    // If |currentItem| is not already associated with a NavigationItemImpl,
    // associate the newly created item with it. Otherwise, discard the new item
    // since it will be a duplicate.
    NavigationItemImpl* current_item =
        GetNavigationItemFromWKItem(current_wk_item);
    if (!current_item) {
      SetNavigationItemInWKItem(current_wk_item, std::move(pending_item_));
    }
    pending_item_.reset();
  } else {
    pending_item_index_ = -1;
  }
}

void WKBasedNavigationManagerImpl::CommitPendingItem() {
  bool last_committed_item_was_empty_window_open_item =
      empty_window_open_item_ != nullptr;

  if (pending_item_index_ == -1) {
    pending_item_->ResetForCommit();
    pending_item_->SetTimestamp(
        time_smoother_.GetSmoothedTime(base::Time::Now()));

    id<CRWWebViewNavigationProxy> proxy =
        delegate_->GetWebViewNavigationProxy();

    // If WKBackForwardList exists but |currentItem| is nil at this point, it is
    // because the current navigation is an empty window open navigation.
    // If |currentItem| is not nil, it is the last committed item in the
    // WKWebView.
    if (proxy.backForwardList && !proxy.backForwardList.currentItem) {
      // WKWebView's URL should be about:blank for empty window open item.
      DCHECK_EQ(url::kAboutBlankURL, net::GURLWithNSURL(proxy.URL).spec());
      // There should be no back-forward history for empty window open item.
      DCHECK_EQ(0UL, proxy.backForwardList.backList.count);
      DCHECK_EQ(0UL, proxy.backForwardList.forwardList.count);

      empty_window_open_item_ = std::move(pending_item_);
    } else {
      empty_window_open_item_.reset();
      SetNavigationItemInWKItem(proxy.backForwardList.currentItem,
                                std::move(pending_item_));
    }
  }

  pending_item_index_ = -1;
  // If the last committed item is the empty window open item, then don't update
  // previous item because the new commit replaces the last committed item.
  if (!last_committed_item_was_empty_window_open_item) {
    previous_item_index_ = last_committed_item_index_;
  }
  // If the newly committed item is the empty window open item, fake an index of
  // 0 because WKBackForwardList is empty at this point.
  last_committed_item_index_ =
      empty_window_open_item_ ? 0 : GetWKCurrentItemIndex();
  OnNavigationItemCommitted();
}

int WKBasedNavigationManagerImpl::GetIndexForOffset(int offset) const {
  int current_item_index = pending_item_index_;
  if (pending_item_index_ == -1) {
    current_item_index = empty_window_open_item_ ? 0 : GetWKCurrentItemIndex();
  }

  if (offset < 0 && GetTransientItem() && pending_item_index_ == -1) {
    // Going back from transient item that added to the end navigation stack
    // is a matter of discarding it as there is no need to move navigation
    // index back.
    offset++;
  }
  return current_item_index + offset;
}

int WKBasedNavigationManagerImpl::GetPreviousItemIndex() const {
  return previous_item_index_;
}

void WKBasedNavigationManagerImpl::SetPreviousItemIndex(
    int previous_item_index) {
  previous_item_index_ = previous_item_index;
}

void WKBasedNavigationManagerImpl::AddPushStateItemIfNecessary(
    const GURL& url,
    NSString* state_object,
    ui::PageTransition transition) {
  // WKBasedNavigationManager doesn't directly manage session history. Nothing
  // to do here.
}

BrowserState* WKBasedNavigationManagerImpl::GetBrowserState() const {
  return browser_state_;
}

WebState* WKBasedNavigationManagerImpl::GetWebState() const {
  return delegate_->GetWebState();
}

NavigationItem* WKBasedNavigationManagerImpl::GetVisibleItem() const {
  NavigationItem* transient_item = GetTransientItem();
  if (transient_item) {
    return transient_item;
  }

  // Only return pending_item_ for new (non-history), user-initiated
  // navigations in order to prevent URL spoof attacks.
  NavigationItemImpl* pending_item = GetPendingItemImpl();
  if (pending_item) {
    bool is_user_initiated = pending_item->NavigationInitiationType() ==
                             NavigationInitiationType::USER_INITIATED;
    bool safe_to_show_pending = is_user_initiated && pending_item_index_ == -1;
    if (safe_to_show_pending) {
      return pending_item;
    }
  }
  return GetLastCommittedItem();
}

void WKBasedNavigationManagerImpl::DiscardNonCommittedItems() {
  pending_item_.reset();
  transient_item_.reset();
}

int WKBasedNavigationManagerImpl::GetItemCount() const {
  if (empty_window_open_item_) {
    return 1;
  }

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
  return GetNavigationItemImplAtIndex(index);
}

int WKBasedNavigationManagerImpl::GetIndexOfItem(
    const NavigationItem* item) const {
  if (item == empty_window_open_item_.get()) {
    return 0;
  }

  for (int index = 0; index < GetItemCount(); index++) {
    if (GetNavigationItemFromWKItem(GetWKItemAtIndex(index)) == item) {
      return index;
    }
  }
  return -1;
}

int WKBasedNavigationManagerImpl::GetPendingItemIndex() const {
  if (GetPendingItem()) {
    if (pending_item_index_ != -1) {
      return pending_item_index_;
    }
    // TODO(crbug.com/665189): understand why last committed item index is
    // returned here.
    return GetLastCommittedItemIndex();
  }
  return -1;
}

int WKBasedNavigationManagerImpl::GetLastCommittedItemIndex() const {
  // WKBackForwardList's |currentItem| is usually the last committed item,
  // except two cases:
  // 1) when the pending navigation is a back-forward navigation, in which
  //    case it is actually the pending item. As a workaround, fall back to
  //    last_committed_item_index_. This is not 100% correct (since
  //    last_committed_item_index_ is only updated for main frame navigations),
  //    but is the best possible answer.
  // 2) when the last committed item is an empty window open item.
  if (pending_item_index_ >= 0 || empty_window_open_item_) {
    return last_committed_item_index_;
  }
  return GetWKCurrentItemIndex();
}

bool WKBasedNavigationManagerImpl::RemoveItemAtIndex(int index) {
  DLOG(WARNING) << "Not yet implemented.";
  return true;
}

bool WKBasedNavigationManagerImpl::CanGoBack() const {
  return CanGoToOffset(-1);
}

bool WKBasedNavigationManagerImpl::CanGoForward() const {
  return CanGoToOffset(1);
}

bool WKBasedNavigationManagerImpl::CanGoToOffset(int offset) const {
  // If the last committed item is the empty window.open item, no back-forward
  // navigation is allowed.
  if (empty_window_open_item_) {
    return offset == 0;
  }
  int index = GetIndexForOffset(offset);
  return index >= 0 && index < GetItemCount();
}

void WKBasedNavigationManagerImpl::GoBack() {
  GoToIndex(GetIndexForOffset(-1));
}

void WKBasedNavigationManagerImpl::GoForward() {
  GoToIndex(GetIndexForOffset(1));
}

NavigationItemList WKBasedNavigationManagerImpl::GetBackwardItems() const {
  NavigationItemList items;

  // If the current navigation item is a transient item (e.g. SSL
  // interstitial), the last committed item should also be considered part of
  // the backward history.
  int wk_current_item_index = GetWKCurrentItemIndex();
  if (GetTransientItem() && wk_current_item_index >= 0) {
    items.push_back(GetItemAtIndex(wk_current_item_index));
  }

  for (int index = wk_current_item_index - 1; index >= 0; index--) {
    items.push_back(GetItemAtIndex(index));
  }
  return items;
}

NavigationItemList WKBasedNavigationManagerImpl::GetForwardItems() const {
  NavigationItemList items;
  for (int index = GetWKCurrentItemIndex() + 1; index < GetItemCount();
       index++) {
    items.push_back(GetItemAtIndex(index));
  }
  return items;
}

void WKBasedNavigationManagerImpl::CopyStateFromAndPrune(
    const NavigationManager* source) {
  DLOG(WARNING) << "Not yet implemented.";
}

bool WKBasedNavigationManagerImpl::CanPruneAllButLastCommittedItem() const {
  DLOG(WARNING) << "Not yet implemented.";
  return true;
}

void WKBasedNavigationManagerImpl::Restore(
    int last_committed_item_index,
    std::vector<std::unique_ptr<NavigationItem>> items) {
  DCHECK_LT(last_committed_item_index, static_cast<int>(items.size()));
  DCHECK(items.empty() || last_committed_item_index >= 0);
  if (items.empty())
    return;

  DiscardNonCommittedItems();
  if (GetItemCount() > 0) {
    delegate_->RemoveWebView();
  }
  DCHECK_EQ(0, GetItemCount());
  pending_item_index_ = -1;
  previous_item_index_ = -1;
  last_committed_item_index_ = -1;

  // This function restores session history by loading a magic local file
  // (restore_session.html) into the web view. The session history is encoded
  // in the query parameter. When loaded, restore_session.html parses the
  // session history and replays them into the web view using History API.

  // TODO(crbug.com/771200): Retain these original NavigationItems restored from
  // storage and associate them with new WKBackForwardListItems created after
  // history restore so information such as scroll position is restored.
  GURL url = CreateRestoreSessionUrl(last_committed_item_index, items);

  WebLoadParams params(url);
  // It's not clear how this transition type will be used and what's the impact.
  // For now, use RELOAD because restoring history is kind of like a reload of
  // the current page.
  params.transition_type = ui::PAGE_TRANSITION_RELOAD;
  LoadURLWithParams(params);

  GetPendingItemImpl()->SetVirtualURL(
      items[last_committed_item_index]->GetVirtualURL());
}

NavigationItemImpl* WKBasedNavigationManagerImpl::GetNavigationItemImplAtIndex(
    size_t index) const {
  if (empty_window_open_item_) {
    // Return nullptr for index != 0 instead of letting the code fall through
    // (which in most cases will return null anyways because wk_item should be
    // nil) for the slim chance that WKBackForwardList has been updated for a
    // new navigation but WKWebView has not triggered the |didCommitNavigation:|
    // callback. NavigationItem for the new wk_item should not be returned until
    // after DidCommitPendingItem() is called.
    return index == 0 ? empty_window_open_item_.get() : nullptr;
  }

  WKBackForwardListItem* wk_item = GetWKItemAtIndex(index);
  NavigationItemImpl* item = GetNavigationItemFromWKItem(wk_item);

  if (!wk_item || item) {
    return item;
  }

  // TODO(crbug.com/734150): Add a stat counter to track rebuilding frequency.
  WKBackForwardListItem* prev_wk_item =
      index == 0 ? nil : GetWKItemAtIndex(index - 1);
  std::unique_ptr<web::NavigationItemImpl> new_item =
      CreateNavigationItemWithRewriters(
          net::GURLWithNSURL(wk_item.URL),
          (prev_wk_item ? web::Referrer(net::GURLWithNSURL(prev_wk_item.URL),
                                        web::ReferrerPolicyAlways)
                        : web::Referrer()),
          ui::PageTransition::PAGE_TRANSITION_LINK,
          NavigationInitiationType::RENDERER_INITIATED,
          // Not using GetLastCommittedItem()->GetURL() in case the last
          // committed item in the WKWebView hasn't been linked to a
          // NavigationItem and this method is called in that code path to avoid
          // an infinite cycle.
          net::GURLWithNSURL(prev_wk_item.URL),
          nullptr /* use default rewriters only */);
  new_item->SetTimestamp(time_smoother_.GetSmoothedTime(base::Time::Now()));
  const GURL& url = new_item->GetURL();
  // If this navigation item has a restore_session.html URL, then it was created
  // to restore session history and will redirect to the target URL encoded in
  // the query parameter automatically. Set virtual URL to the target URL so the
  // internal restore_session.html is not exposed in the UI and to URL-sensing
  // components outside of //ios/web layer.
  if (IsRestoreSessionUrl(url)) {
    GURL virtual_url;
    bool success = ExtractTargetURL(url, &virtual_url);
    DCHECK(success);
    if (success)
      new_item->SetVirtualURL(virtual_url);
  }

  SetNavigationItemInWKItem(wk_item, std::move(new_item));
  return GetNavigationItemFromWKItem(wk_item);
}

NavigationItemImpl* WKBasedNavigationManagerImpl::GetLastCommittedItemImpl()
    const {
  if (empty_window_open_item_) {
    return empty_window_open_item_.get();
  }

  int index = GetLastCommittedItemIndex();
  return index == -1 ? nullptr
                     : GetNavigationItemImplAtIndex(static_cast<size_t>(index));
}

NavigationItemImpl* WKBasedNavigationManagerImpl::GetPendingItemImpl() const {
  return (pending_item_index_ == -1)
             ? pending_item_.get()
             : GetNavigationItemImplAtIndex(pending_item_index_);
}

NavigationItemImpl* WKBasedNavigationManagerImpl::GetTransientItemImpl() const {
  return transient_item_.get();
}

void WKBasedNavigationManagerImpl::FinishGoToIndex(
    int index,
    NavigationInitiationType type) {
  DiscardNonCommittedItems();
  NavigationItem* item = GetItemAtIndex(index);
  item->SetTransitionType(ui::PageTransitionFromInt(
      item->GetTransitionType() | ui::PAGE_TRANSITION_FORWARD_BACK));
  WKBackForwardListItem* wk_item = GetWKItemAtIndex(index);
  if (wk_item) {
    [delegate_->GetWebViewNavigationProxy() goToBackForwardListItem:wk_item];
  } else {
    DCHECK(index == 0 && empty_window_open_item_)
        << " wk_item should not be nullptr. index: " << index
        << " has_empty_window_open_item: "
        << (empty_window_open_item_ != nullptr);
  }
}

bool WKBasedNavigationManagerImpl::IsPlaceholderUrl(const GURL& url) const {
  return placeholder_navigation_util::IsPlaceholderUrl(url);
}

int WKBasedNavigationManagerImpl::GetWKCurrentItemIndex() const {
  id<CRWWebViewNavigationProxy> proxy = delegate_->GetWebViewNavigationProxy();
  if (proxy.backForwardList.currentItem) {
    return static_cast<int>(proxy.backForwardList.backList.count);
  }
  return -1;
}

WKBackForwardListItem* WKBasedNavigationManagerImpl::GetWKItemAtIndex(
    int index) const {
  if (index < 0 || index >= GetItemCount()) {
    return nil;
  }

  // Convert the index to an offset relative to backForwardList.currentItem (
  // which is also the last committed item), then use WKBackForwardList API to
  // retrieve the item.
  int offset = index - GetWKCurrentItemIndex();
  id<CRWWebViewNavigationProxy> proxy = delegate_->GetWebViewNavigationProxy();
  return [proxy.backForwardList itemAtIndex:offset];
}

}  // namespace web
