// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NAVIGATION_NAVIGATION_MANAGER_IMPL_H_
#define IOS_WEB_NAVIGATION_NAVIGATION_MANAGER_IMPL_H_

#include <stddef.h>

#include <memory>
#include <vector>

#import "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#import "ios/web/public/navigation_item_list.h"
#import "ios/web/public/navigation_manager.h"
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

// Defines the ways how a pending navigation can be initiated.
enum class NavigationInitiationType {
  // Navigation initiation type is only valid for pending navigations, use NONE
  // if a navigation is already committed.
  NONE = 0,

  // Navigation was initiated by actual user action.
  USER_INITIATED,

  // Navigation was initiated by renderer. Examples of renderer-initiated
  // navigations include:
  //  * <a> link click
  //  * changing window.location.href
  //  * redirect via the <meta http-equiv="refresh"> tag
  //  * using window.history.pushState
  RENDERER_INITIATED,
};

// Implementation of NavigationManager.
// Generally mirrors upstream's NavigationController.
class NavigationManagerImpl : public NavigationManager {
 public:
  NavigationManagerImpl();
  ~NavigationManagerImpl() override;

  // Setters for NavigationManagerDelegate and BrowserState.
  void SetDelegate(NavigationManagerDelegate* delegate);
  void SetBrowserState(BrowserState* browser_state);

  // Sets the CRWSessionController that backs this object.
  // Keeps a strong reference to |session_controller|.
  // This method should only be called when deserializing |session_controller|
  // and joining it with its NavigationManager. Other cases should call
  // InitializeSession() or ReplaceSessionHistory().
  // TODO(stuartmorgan): Also move deserialization of CRWSessionControllers
  // under the control of this class, and move the bulk of CRWSessionController
  // logic into it.
  void SetSessionController(CRWSessionController* session_controller);

  // Initializes a new session history.
  void InitializeSession();

  // Replace the session history with a new one, where |items| is the
  // complete set of navigation items in the new history, and |current_index|
  // is the index of the currently active item.
  void ReplaceSessionHistory(std::vector<std::unique_ptr<NavigationItem>> items,
                             int current_index);

  // Helper functions for notifying WebStateObservers of changes.
  // TODO(stuartmorgan): Make these private once the logic triggering them moves
  // into this layer.
  void OnNavigationItemsPruned(size_t pruned_item_count);
  void OnNavigationItemChanged();
  void OnNavigationItemCommitted();

  // Temporary accessors and content/ class pass-throughs.
  // TODO(stuartmorgan): Re-evaluate this list once the refactorings have
  // settled down.
  CRWSessionController* GetSessionController();

  // Adds a transient item with the given URL. A transient item will be
  // discarded on any navigation.
  void AddTransientItem(const GURL& url);

  // Adds a new item with the given url, referrer, navigation type, initiation
  // type and user agent override option, making it the pending item. If pending
  // item is the same as the current item, this does nothing. |referrer| may be
  // nil if there isn't one. The item starts out as pending, and will be lost
  // unless |-commitPendingItem| is called.
  void AddPendingItem(const GURL& url,
                      const web::Referrer& referrer,
                      ui::PageTransition navigation_type,
                      NavigationInitiationType initiation_type,
                      UserAgentOverrideOption user_agent_override_option);

  // Commits the pending item, if any.
  void CommitPendingItem();

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

  // Returns the current list of transient url rewriters, passing ownership to
  // the caller.
  // TODO(crbug.com/546197): remove once NavigationItem creation occurs in this
  // class.
  std::unique_ptr<std::vector<BrowserURLRewriter::URLRewriter>>
  GetTransientURLRewriters();

  // Called to reset the transient url rewriter list.
  void RemoveTransientURLRewriters();

  // Returns the navigation index that differs from the current item (or pending
  // item if it exists) by the specified |offset|, skipping redirect navigation
  // items. The index returned is not guaranteed to be valid.
  // TODO(crbug.com/661316): Make this method private once navigation code is
  // moved from CRWWebController to NavigationManagerImpl.
  int GetIndexForOffset(int offset) const;

 private:
  // The SessionStorageBuilder functions require access to private variables of
  // NavigationManagerImpl.
  friend SessionStorageBuilder;

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

  DISALLOW_COPY_AND_ASSIGN(NavigationManagerImpl);
};

}  // namespace web

#endif  // IOS_WEB_NAVIGATION_NAVIGATION_MANAGER_IMPL_H_
