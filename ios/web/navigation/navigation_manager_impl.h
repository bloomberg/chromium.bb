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
#import "ios/web/navigation/navigation_item_impl.h"
#import "ios/web/public/navigation_item_list.h"
#import "ios/web/public/navigation_manager.h"
#include "ios/web/public/reload_type.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

@class CRWSessionController;

namespace web {
class BrowserState;
class NavigationItem;
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
  virtual void SetDelegate(NavigationManagerDelegate* delegate);
  virtual void SetBrowserState(BrowserState* browser_state);

  // Sets the CRWSessionController that backs this object.
  // Keeps a strong reference to |session_controller|.
  // This method should only be called when deserializing |session_controller|
  // and joining it with its NavigationManager. Other cases should call
  // InitializeSession() or Restore().
  // TODO(stuartmorgan): Also move deserialization of CRWSessionControllers
  // under the control of this class, and move the bulk of CRWSessionController
  // logic into it.
  virtual void SetSessionController(
      CRWSessionController* session_controller) = 0;

  // Initializes a new session history.
  virtual void InitializeSession() = 0;

  // Helper functions for notifying WebStateObservers of changes.
  // TODO(stuartmorgan): Make these private once the logic triggering them moves
  // into this layer.
  virtual void OnNavigationItemsPruned(size_t pruned_item_count) = 0;
  virtual void OnNavigationItemChanged() = 0;
  virtual void OnNavigationItemCommitted() = 0;

  // Temporary accessors and content/ class pass-throughs.
  // TODO(stuartmorgan): Re-evaluate this list once the refactorings have
  // settled down.
  virtual CRWSessionController* GetSessionController() const = 0;

  // Adds a transient item with the given URL. A transient item will be
  // discarded on any navigation.
  virtual void AddTransientItem(const GURL& url) = 0;

  // Adds a new item with the given url, referrer, navigation type, initiation
  // type and user agent override option, making it the pending item. If pending
  // item is the same as the current item, this does nothing. |referrer| may be
  // nil if there isn't one. The item starts out as pending, and will be lost
  // unless |-commitPendingItem| is called.
  virtual void AddPendingItem(
      const GURL& url,
      const web::Referrer& referrer,
      ui::PageTransition navigation_type,
      NavigationInitiationType initiation_type,
      UserAgentOverrideOption user_agent_override_option) = 0;

  // Commits the pending item, if any.
  virtual void CommitPendingItem() = 0;

  // Returns the navigation index that differs from the current item (or pending
  // item if it exists) by the specified |offset|, skipping redirect navigation
  // items. The index returned is not guaranteed to be valid.
  // TODO(crbug.com/661316): Make this method private once navigation code is
  // moved from CRWWebController to NavigationManagerImpl.
  virtual int GetIndexForOffset(int offset) const = 0;

  // Returns the index of the previous item. Only used by SessionStorageBuilder.
  virtual int GetPreviousItemIndex() const = 0;

  // Sets the index of the previous item. Only used by SessionStorageBuilder.
  virtual void SetPreviousItemIndex(int previous_item_index) = 0;

  // Resets the transient url rewriter list.
  void RemoveTransientURLRewriters();

  // Creates a NavigationItem using the given properties. Calling this method
  // resets the transient URLRewriters cached in this instance.
  // TODO(crbug.com/738020): This method is only used by CRWSessionController.
  // Remove it after switching to WKBasedNavigationManagerImpl.
  std::unique_ptr<NavigationItemImpl> CreateNavigationItem(
      const GURL& url,
      const Referrer& referrer,
      ui::PageTransition transition,
      NavigationInitiationType initiation_type);

  // Updates the URL of the yet to be committed pending item. Useful for page
  // redirects. Does nothing if there is no pending item.
  void UpdatePendingItemUrl(const GURL& url) const;

  // The current NavigationItem. During a pending navigation, returns the
  // NavigationItem for that navigation. If a transient NavigationItem exists,
  // this NavigationItem will be returned.
  // TODO(crbug.com/661316): Make this private once all navigation code is moved
  // out of CRWWebController.
  NavigationItemImpl* GetCurrentItemImpl() const;

  // NavigationManager:
  NavigationItem* GetLastCommittedItem() const final;
  NavigationItem* GetPendingItem() const final;
  NavigationItem* GetTransientItem() const final;
  void LoadURLWithParams(const NavigationManager::WebLoadParams&) final;
  void AddTransientURLRewriter(BrowserURLRewriter::URLRewriter rewriter) final;
  void GoToIndex(int index) final;
  void Reload(ReloadType reload_type, bool check_for_reposts) final;
  void LoadIfNecessary() final;

 protected:
  // The SessionStorageBuilder functions require access to private variables of
  // NavigationManagerImpl.
  friend SessionStorageBuilder;

  // TODO(crbug.com/738020): Remove legacy code and merge
  // WKBasedNavigationManager into this class after the navigation experiment.

  // Checks whether or not two URLs differ only in the fragment.
  static bool IsFragmentChangeNavigationBetweenUrls(const GURL& existing_url,
                                                    const GURL& new_url);

  // Applies the user agent override to |pending_item|, or inherits the user
  // agent of |inherit_from| if |user_agent_override_option| is INHERIT.
  static void UpdatePendingItemUserAgentType(
      UserAgentOverrideOption override_option,
      const NavigationItem* inherit_from,
      NavigationItem* pending_item);

  // Creates a NavigationItem using the given properties, where |previous_url|
  // is the URL of the navigation just prior to the current one. If
  // |url_rewriters| is not nullptr, apply them before applying the permanent
  // URL rewriters from BrowserState.
  // TODO(crbug.com/738020): Make this private when WKBasedNavigationManagerImpl
  // is merged into this class.
  std::unique_ptr<NavigationItemImpl> CreateNavigationItemWithRewriters(
      const GURL& url,
      const Referrer& referrer,
      ui::PageTransition transition,
      NavigationInitiationType initiation_type,
      const GURL& previous_url,
      const std::vector<BrowserURLRewriter::URLRewriter>* url_rewriters) const;

  // Returns the most recent NavigationItem that does not have an app-specific
  // URL.
  NavigationItem* GetLastCommittedNonAppSpecificItem() const;

  // Identical to GetItemAtIndex() but returns the underlying NavigationItemImpl
  // instead of the public NavigationItem interface. This is used by
  // SessionStorageBuilder to persist session state.
  virtual NavigationItemImpl* GetNavigationItemImplAtIndex(
      size_t index) const = 0;

  // Implementation for corresponding NavigationManager getters.
  virtual NavigationItemImpl* GetPendingItemImpl() const = 0;
  virtual NavigationItemImpl* GetTransientItemImpl() const = 0;
  virtual NavigationItemImpl* GetLastCommittedItemImpl() const = 0;

  // Subclass specific implementation to update session state.
  virtual void FinishGoToIndex(int index) = 0;

  // The primary delegate for this manager.
  NavigationManagerDelegate* delegate_;

  // The BrowserState that is associated with this instance.
  BrowserState* browser_state_;

  // List of transient url rewriters added by |AddTransientURLRewriter()|.
  std::vector<BrowserURLRewriter::URLRewriter> transient_url_rewriters_;
};

}  // namespace web

#endif  // IOS_WEB_NAVIGATION_NAVIGATION_MANAGER_IMPL_H_
