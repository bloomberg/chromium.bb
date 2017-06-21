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
  ~NavigationManagerImpl() override {}

  // Setters for NavigationManagerDelegate and BrowserState.
  virtual void SetDelegate(NavigationManagerDelegate* delegate) = 0;
  virtual void SetBrowserState(BrowserState* browser_state) = 0;

  // Sets the CRWSessionController that backs this object.
  // Keeps a strong reference to |session_controller|.
  // This method should only be called when deserializing |session_controller|
  // and joining it with its NavigationManager. Other cases should call
  // InitializeSession() or ReplaceSessionHistory().
  // TODO(stuartmorgan): Also move deserialization of CRWSessionControllers
  // under the control of this class, and move the bulk of CRWSessionController
  // logic into it.
  virtual void SetSessionController(
      CRWSessionController* session_controller) = 0;

  // Initializes a new session history.
  virtual void InitializeSession() = 0;

  // Replace the session history with a new one, where |items| is the
  // complete set of navigation items in the new history, and |current_index|
  // is the index of the currently active item.
  virtual void ReplaceSessionHistory(
      std::vector<std::unique_ptr<NavigationItem>> items,
      int current_index) = 0;

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

  // Returns the current list of transient url rewriters, passing ownership to
  // the caller.
  // TODO(crbug.com/546197): remove once NavigationItem creation occurs in this
  // class.
  virtual std::unique_ptr<std::vector<BrowserURLRewriter::URLRewriter>>
  GetTransientURLRewriters() = 0;

  // Called to reset the transient url rewriter list.
  virtual void RemoveTransientURLRewriters() = 0;

  // Returns the navigation index that differs from the current item (or pending
  // item if it exists) by the specified |offset|, skipping redirect navigation
  // items. The index returned is not guaranteed to be valid.
  // TODO(crbug.com/661316): Make this method private once navigation code is
  // moved from CRWWebController to NavigationManagerImpl.
  virtual int GetIndexForOffset(int offset) const = 0;

 protected:
  // The SessionStorageBuilder functions require access to private variables of
  // NavigationManagerImpl.
  friend SessionStorageBuilder;

  // Identical to GetItemAtIndex() but returns the underlying NavigationItemImpl
  // instead of the public NavigationItem interface. This is used by
  // SessionStorageBuilder to persist session state.
  virtual NavigationItemImpl* GetNavigationItemImplAtIndex(
      size_t index) const = 0;

  // Returns the index of the previous item. Only used by SessionStorageBuilder.
  virtual size_t GetPreviousItemIndex() const = 0;
};

}  // namespace web

#endif  // IOS_WEB_NAVIGATION_NAVIGATION_MANAGER_IMPL_H_
