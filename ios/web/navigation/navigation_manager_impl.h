// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NAVIGATION_NAVIGATION_MANAGER_IMPL_H_
#define IOS_WEB_NAVIGATION_NAVIGATION_MANAGER_IMPL_H_

#include <vector>

#include "base/basictypes.h"
#include "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_vector.h"
#include "ios/web/public/navigation_manager.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

@class CRWSessionController;
@class CRWSessionEntry;

namespace web {
class BrowserState;
class NavigationItem;
struct Referrer;
class NavigationManagerDelegate;
class NavigationManagerFacadeDelegate;

// Implementation of NavigationManager.
// Generally mirrors upstream's NavigationController.
class NavigationManagerImpl : public NavigationManager {
 public:
  NavigationManagerImpl(NavigationManagerDelegate* delegate,
                        BrowserState* browser_state);
  ~NavigationManagerImpl() override;

  // Sets the CRWSessionController that backs this object.
  // Keeps a strong reference to |session_controller|.
  // This method should only be called when deserializing |session_controller|
  // and joining it with its NavigationManager. Other cases should call
  // InitializeSession() or ReplaceSessionHistory().
  // TODO(stuartmorgan): Also move deserialization of CRWSessionControllers
  // under the control of this class, and move the bulk of CRWSessionController
  // logic into it.
  void SetSessionController(CRWSessionController* session_controller);

  // Initializes a new session history, supplying a unique |window_name| for the
  // window (or nil). |opener_id| is the id of opener, or nil if there is none.
  // |opened_by_dom| is YES if the page was opened by DOM.
  // |opener_index| is the navigation index of the opener, or -1 if there is
  // none.
  void InitializeSession(NSString* window_name,
                         NSString* opener_id,
                         BOOL opened_by_dom,
                         int opener_navigation_index);

  // Replace the session history with a new one, where |items| is the
  // complete set of navigation items in the new history, and |current_index|
  // is the index of the currently active item.
  void ReplaceSessionHistory(ScopedVector<NavigationItem> items,
                             int current_index);

  // Sets the delegate used to drive the navigation controller facade.
  void SetFacadeDelegate(NavigationManagerFacadeDelegate* facade_delegate);
  NavigationManagerFacadeDelegate* GetFacadeDelegate() const;

  // Helper functions for communicating with the facade layer.
  // TODO(stuartmorgan): Make these private once the logic triggering them moves
  // into this layer.
  void OnNavigationItemChanged();
  void OnNavigationItemCommitted();

  // Returns the transient item if any. This is an item which is removed and
  // discarded if any navigation occurs. Note that the returned item is owned
  // by the navigation manager and may be deleted at any time.
  NavigationItem* GetTransientItem() const;

  // Temporary accessors and content/ class pass-throughs.
  // TODO(stuartmorgan): Re-evaluate this list once the refactorings have
  // settled down.
  CRWSessionController* GetSessionController();
  int GetLastCommittedEntryIndex() const;
  bool RemoveEntryAtIndex(int index);
  void LoadURL(const GURL& url,
               const Referrer& referrer,
               ui::PageTransition type);
  bool CanGoBack() const;
  bool CanGoForward() const;
  void GoBack();
  void GoForward();

  // Convenience accessors to get the underlying NavigationItems from the
  // SessionEntries returned from |session_controller_|'s -lastUserEntry and
  // -previousEntry methods.
  // TODO(crbug.com/546365): Remove these methods.
  NavigationItem* GetLastUserItem() const;
  NavigationItem* GetPreviousItem() const;

  // Temporary method. Returns a vector of NavigationItems corresponding to
  // the SessionEntries of the uderlying CRWSessionController.
  // TODO(crbug.com/546365): Remove this method.
  std::vector<NavigationItem*> GetItems();

  // NavigationManager:
  BrowserState* GetBrowserState() const override;
  WebState* GetWebState() const override;
  NavigationItem* GetVisibleItem() const override;
  NavigationItem* GetLastCommittedItem() const override;
  NavigationItem* GetPendingItem() const override;
  void DiscardNonCommittedItems() override;
  void LoadIfNecessary() override;
  void AddTransientURLRewriter(
      BrowserURLRewriter::URLRewriter rewriter) override;
  int GetEntryCount() const override;
  NavigationItem* GetItemAtIndex(size_t index) const override;
  int GetCurrentEntryIndex() const override;
  int GetPendingItemIndex() const override;

  // Returns the current list of transient url rewriters, passing ownership to
  // the caller.
  // TODO(kkhorimoto): remove once NavigationItem creation occurs in this class.
  scoped_ptr<std::vector<BrowserURLRewriter::URLRewriter>>
  GetTransientURLRewriters();

  // Called to reset the transient url rewriter list.
  void RemoveTransientURLRewriters();

  // Copy state from |navigation_manager|, including a copy of that object's
  // CRWSessionController.
  void CopyState(NavigationManagerImpl* navigation_manager);
 private:
  // The primary delegate for this manager.
  NavigationManagerDelegate* delegate_;

  // The BrowserState that is associated with this instance.
  BrowserState* browser_state_;

  // CRWSessionController that backs this instance.
  // TODO(stuartmorgan): Fold CRWSessionController into this class.
  base::scoped_nsobject<CRWSessionController> session_controller_;

  // Weak pointer to the facade delegate.
  NavigationManagerFacadeDelegate* facade_delegate_;

  // List of transient url rewriters added by |AddTransientURLRewriter()|.
  scoped_ptr<std::vector<BrowserURLRewriter::URLRewriter>>
      transient_url_rewriters_;

  DISALLOW_COPY_AND_ASSIGN(NavigationManagerImpl);
};

}  // namespace web

#endif  // IOS_WEB_NAVIGATION_NAVIGATION_MANAGER_IMPL_H_
