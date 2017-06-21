// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NAVIGATION_LEGACY_NAVIGATION_MANAGER_IMPL_H_
#define IOS_WEB_NAVIGATION_LEGACY_NAVIGATION_MANAGER_IMPL_H_

#include <stddef.h>

#include <memory>
#include <vector>

#import "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#import "ios/web/navigation/navigation_manager_impl.h"
#import "ios/web/public/navigation_item_list.h"
#include "ios/web/public/reload_type.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

@class CRWSessionController;

namespace web {
class BrowserState;
class NavigationItem;
struct Referrer;
class NavigationManagerDelegate;
class SessionStorageBuilder;

// Implementation of NavigationManagerImpl.
class LegacyNavigationManagerImpl : public NavigationManagerImpl {
 public:
  LegacyNavigationManagerImpl();
  ~LegacyNavigationManagerImpl() override;

  // NavigationManagerImpl:
  void SetDelegate(NavigationManagerDelegate* delegate) override;
  void SetBrowserState(BrowserState* browser_state) override;
  void SetSessionController(CRWSessionController* session_controller) override;
  void InitializeSession() override;
  void ReplaceSessionHistory(std::vector<std::unique_ptr<NavigationItem>> items,
                             int current_index) override;
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
  std::unique_ptr<std::vector<BrowserURLRewriter::URLRewriter>>
  GetTransientURLRewriters() override;
  void RemoveTransientURLRewriters() override;
  int GetIndexForOffset(int offset) const override;

  // NavigationManager:
  BrowserState* GetBrowserState() const override;
  WebState* GetWebState() const override;
  NavigationItem* GetVisibleItem() const override;
  NavigationItem* GetLastCommittedItem() const override;
  NavigationItem* GetPendingItem() const override;
  NavigationItem* GetTransientItem() const override;
  void DiscardNonCommittedItems() override;
  void LoadURLWithParams(const NavigationManager::WebLoadParams&) override;
  void AddTransientURLRewriter(
      BrowserURLRewriter::URLRewriter rewriter) override;
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
  void GoToIndex(int index) override;
  void Reload(ReloadType reload_type, bool check_for_reposts) override;
  NavigationItemList GetBackwardItems() const override;
  NavigationItemList GetForwardItems() const override;
  void CopyStateFromAndPrune(const NavigationManager* source) override;
  bool CanPruneAllButLastCommittedItem() const override;

 private:
  // The SessionStorageBuilder functions require access to private variables of
  // NavigationManagerImpl.
  friend SessionStorageBuilder;

  // NavigationManagerImpl methods used by SessionStorageBuilder.
  NavigationItemImpl* GetNavigationItemImplAtIndex(size_t index) const override;
  size_t GetPreviousItemIndex() const override;

  // Returns true if the PageTransition for the underlying navigation item at
  // |index| has ui::PAGE_TRANSITION_IS_REDIRECT_MASK.
  bool IsRedirectItemAtIndex(int index) const;

  // Returns the most recent NavigationItem that does not have an app-specific
  // URL.
  NavigationItem* GetLastCommittedNonAppSpecificItem() const;

  // The primary delegate for this manager.
  NavigationManagerDelegate* delegate_;

  // The BrowserState that is associated with this instance.
  BrowserState* browser_state_;

  // CRWSessionController that backs this instance.
  // TODO(stuartmorgan): Fold CRWSessionController into this class.
  base::scoped_nsobject<CRWSessionController> session_controller_;

  // List of transient url rewriters added by |AddTransientURLRewriter()|.
  std::unique_ptr<std::vector<BrowserURLRewriter::URLRewriter>>
      transient_url_rewriters_;

  DISALLOW_COPY_AND_ASSIGN(LegacyNavigationManagerImpl);
};

}  // namespace web

#endif  // IOS_WEB_NAVIGATION_LEGACY_NAVIGATION_MANAGER_IMPL_H_
