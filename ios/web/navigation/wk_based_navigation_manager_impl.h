// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NAVIGATION_WK_BASED_NAVIGATION_MANAGER_IMPL_H_
#define IOS_WEB_NAVIGATION_WK_BASED_NAVIGATION_MANAGER_IMPL_H_

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/macros.h"
#import "ios/web/navigation/navigation_item_impl.h"
#import "ios/web/navigation/navigation_manager_impl.h"
#include "ios/web/navigation/time_smoother.h"
#include "ios/web/public/reload_type.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

@class WKBackForwardListItem;

namespace web {
class BrowserState;
class NavigationItem;
struct Referrer;
class SessionStorageBuilder;

// WKBackForwardList based implementation of NavigationManagerImpl.
// This class relies on the following WKWebView APIs, defined by the
// CRWWebViewNavigationProxy protocol:
//   @property URL
//   @property backForwardList
//   @property canGoBack
//   @property canGoForward
//   - goBack
//   - goForward
//   - goToBackForwardListItem:
//
// This navigation manager uses WKBackForwardList as the ground truth for back-
// forward navigation history. It uses the Associated Objects runtime feature
// to link a NavigationItemImpl object to each WKBackForwardListItem to store
// additional states needed by the embedder.
//
// For all main frame navigations (both UI-initiated and renderer-initiated),
// the NavigationItemImpl objects are created proactively via AddPendingItem and
// CommitPendingItem.
//
// Non-main-frame navigations can only be initiated from the renderer. The
// NavigationItemImpl objects in this case are created lazily in GetItemAtIndex
// because the provisional load and commit events for iframe navigation are not
// visible via the WKNavigationDelegate interface. Consequently, pending item
// and previous item are only tracked for the main frame.
//
// Empty Window Open Navigation edge case:
//
//   If window.open() is called with an empty URL, WKWebView does not seem to
//   create a WKBackForwardListItem for the first about:blank navigation. Any
//   subsequent navigation in this window will replace the about:blank entry.
//   This is consistent with the HTML spec regarding Location-object navigation
//   when the browser context's only Document is about:blank:
//   https://html.spec.whatwg.org/multipage/history.html (Section 7.7.4)
//
//   This navigation manager will still create a pendingNavigationItem for this
//   "empty window open item" and allow CommitPendingItem() to be called on it.
//   All accessors will behave identically as if the navigation history has a
//   single normal entry. The only difference is that a subsequent call to
//   CommitPendingItem() will *replace* the empty window open item. From this
//   point onward, it is as if the empty window open item never occurred.
class WKBasedNavigationManagerImpl : public NavigationManagerImpl {
 public:
  WKBasedNavigationManagerImpl();
  ~WKBasedNavigationManagerImpl() override;

  // NavigationManagerImpl:
  void SetSessionController(CRWSessionController* session_controller) override;
  void InitializeSession() override;
  void OnNavigationItemsPruned(size_t pruned_item_count) override;
  void OnNavigationItemChanged() override;
  void OnNavigationItemCommitted() override;
  CRWSessionController* GetSessionController() const override;
  void AddTransientItem(const GURL& url) override;
  void AddPendingItem(
      const GURL& url,
      const web::Referrer& referrer,
      ui::PageTransition navigation_type,
      NavigationInitiationType initiation_type,
      UserAgentOverrideOption user_agent_override_option) override;
  void CommitPendingItem() override;
  int GetIndexForOffset(int offset) const override;
  // Returns the previous navigation item in the main frame.
  int GetPreviousItemIndex() const override;
  void SetPreviousItemIndex(int previous_item_index) override;
  void AddPushStateItemIfNecessary(const GURL& url,
                                   NSString* state_object,
                                   ui::PageTransition transition) override;

  // NavigationManager:
  BrowserState* GetBrowserState() const override;
  WebState* GetWebState() const override;
  NavigationItem* GetVisibleItem() const override;
  void DiscardNonCommittedItems() override;
  int GetItemCount() const override;
  NavigationItem* GetItemAtIndex(size_t index) const override;
  int GetIndexOfItem(const NavigationItem* item) const override;
  int GetPendingItemIndex() const override;
  int GetLastCommittedItemIndex() const override;
  bool RemoveItemAtIndex(int index) override;
  bool CanGoBack() const override;
  bool CanGoForward() const override;
  bool CanGoToOffset(int offset) const override;
  void GoBack() override;
  void GoForward() override;
  NavigationItemList GetBackwardItems() const override;
  NavigationItemList GetForwardItems() const override;
  void CopyStateFromAndPrune(const NavigationManager* source) override;
  bool CanPruneAllButLastCommittedItem() const override;
  void Restore(int last_committed_item_index,
               std::vector<std::unique_ptr<NavigationItem>> items) override;

 private:
  // The SessionStorageBuilder functions require access to private variables of
  // NavigationManagerImpl.
  friend SessionStorageBuilder;

  // NavigationManagerImpl:
  NavigationItemImpl* GetNavigationItemImplAtIndex(size_t index) const override;
  NavigationItemImpl* GetLastCommittedItemImpl() const override;
  // Returns the pending navigation item in the main frame.
  NavigationItemImpl* GetPendingItemImpl() const override;
  NavigationItemImpl* GetTransientItemImpl() const override;
  void FinishGoToIndex(int index,
                       NavigationInitiationType initiation_type) override;
  bool IsPlaceholderUrl(const GURL& url) const override;

  // Returns the absolute index of WKBackForwardList's |currentItem|. Returns -1
  // if |currentItem| is nil.
  int GetWKCurrentItemIndex() const;

  // Returns the WKNavigationItem object at the absolute index, where index = 0
  // corresponds to the oldest entry in the back-forward history, and
  // index = GetItemCount() - 1 corresponds to the newest entry in the back-
  // forward history. Returns nil if |index| is outside [0, GetItemCount()).
  WKBackForwardListItem* GetWKItemAtIndex(int index) const;

  // The pending main frame navigation item. This is nullptr if there is no
  // pending item or if the pending item is a back-forward navigation, in which
  // case the NavigationItemImpl is stored on the WKBackForwardListItem.
  std::unique_ptr<NavigationItemImpl> pending_item_;

  // -1 if pending_item_ represents a new navigation or there is no pending
  // navigation. Otherwise, this is the index of the pending_item in the
  // back-forward list.
  int pending_item_index_;

  // Index of the previous navigation item in the main frame. If there is none,
  // this field will have value -1.
  int previous_item_index_;

  // Index of the last committed item in the main frame. If there is none, this
  // field will equal to -1.
  int last_committed_item_index_;

  // The NavigationItem that corresponds to the empty window open navigation. It
  // has to be stored separately because it has no WKBackForwardListItem. It is
  // not null if when CommitPendingItem() is last called, the WKBackForwardList
  // is empty but not nil. Any subsequent call to CommitPendingItem() will reset
  // this field to null.
  std::unique_ptr<NavigationItemImpl> empty_window_open_item_;

  // The transient item in main frame.
  std::unique_ptr<NavigationItemImpl> transient_item_;

  // Time smoother for navigation item timestamps. See comment in
  // navigation_controller_impl.h.
  // NOTE: This is mutable because GetNavigationItemImplAtIndex() needs to call
  // TimeSmoother::GetSmoothedTime() with a const 'this'. Since NavigationItems
  // have to be lazily created on read, this is the only workaround.
  mutable TimeSmoother time_smoother_;

  DISALLOW_COPY_AND_ASSIGN(WKBasedNavigationManagerImpl);
};

}  // namespace web

#endif  // IOS_WEB_NAVIGATION_WK_BASED_NAVIGATION_MANAGER_IMPL_H_
