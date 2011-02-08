// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TAB_CONTENTS_TAB_CONTENTS_WRAPPER_H_
#define CHROME_BROWSER_UI_TAB_CONTENTS_TAB_CONTENTS_WRAPPER_H_
#pragma once

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "base/compiler_specific.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_observer.h"
#include "chrome/common/notification_registrar.h"

class Extension;
class FindManager;
class NavigationController;
class PasswordManager;
class PasswordManagerDelegate;
class TabContentsWrapperDelegate;

// Wraps TabContents and all of its supporting objects in order to control
// their ownership and lifetime, while allowing TabContents to remain generic
// and re-usable in other projects.
// TODO(pinkerton): Eventually, this class will become TabContents as far as
// the browser front-end is concerned, and the current TabContents will be
// renamed to something like WebPage or WebView (ben's suggestions).
class TabContentsWrapper : public NotificationObserver,
                           public TabContentsObserver {
 public:
  // Takes ownership of |contents|, which must be heap-allocated (as it lives
  // in a scoped_ptr) and can not be NULL.
  explicit TabContentsWrapper(TabContents* contents);
  ~TabContentsWrapper();

  // Used to retrieve this object from |tab_contents_|, which is placed in
  // its property bag to avoid adding additional interfaces.
  static PropertyAccessor<TabContentsWrapper*>* property_accessor();

  // Create a TabContentsWrapper with the same state as this one. The returned
  // heap-allocated pointer is owned by the caller.
  TabContentsWrapper* Clone();

  // Helper to retrieve the existing instance that wraps a given TabContents.
  // Returns NULL if there is no such existing instance.
  static TabContentsWrapper* GetCurrentWrapperForContents(
      TabContents* contents);

  TabContentsWrapperDelegate* delegate() const { return delegate_; }
  void set_delegate(TabContentsWrapperDelegate* d) { delegate_ = d; }

  TabContents* tab_contents() const { return tab_contents_.get(); }
  NavigationController& controller() const {
    return tab_contents()->controller();
  }
  TabContentsView* view() const { return tab_contents()->view(); }
  RenderViewHost* render_view_host() const {
    return tab_contents()->render_view_host();
  }
  Profile* profile() const { return tab_contents()->profile(); }

  // Convenience methods until extensions are removed from TabContents.
  void SetExtensionAppById(const std::string& extension_app_id) {
    tab_contents()->SetExtensionAppById(extension_app_id);
  }
  const Extension* extension_app() const {
    return tab_contents()->extension_app();
  }
  bool is_app() const { return tab_contents()->is_app(); }

  bool is_starred() const { return is_starred_; }

  // Returns the PasswordManager, creating it if necessary.
  PasswordManager* GetPasswordManager();

  // Returns the FindManager, creating it if necessary.
  FindManager* GetFindManager();

  // Overrides -----------------------------------------------------------------

  // TabContentsObserver overrides:
  virtual void NavigateToPendingEntry() OVERRIDE;
  virtual void DidNavigateMainFramePostCommit(
      const NavigationController::LoadCommittedDetails& details,
      const ViewHostMsg_FrameNavigate_Params& params) OVERRIDE;

  // NotificationObserver overrides:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

 private:
  // Internal helpers ----------------------------------------------------------

  // Updates the starred state from the bookmark bar model. If the state has
  // changed, the delegate is notified.
  void UpdateStarredStateForCurrentURL();

  // Data for core operation ---------------------------------------------------

  // Delegate for notifying our owner about stuff. Not owned by us.
  TabContentsWrapperDelegate* delegate_;

  // Registers and unregisters us for notifications.
  NotificationRegistrar registrar_;

  // Data for current page -----------------------------------------------------

  // Whether the current URL is starred.
  bool is_starred_;

  // Supporting objects --------------------------------------------------------

  // PasswordManager and its delegate, lazily created. The delegate must
  // outlive the manager, per documentation in password_manager.h.
  scoped_ptr<PasswordManagerDelegate> password_manager_delegate_;
  scoped_ptr<PasswordManager> password_manager_;

  // FindManager, lazily created.
  scoped_ptr<FindManager> find_manager_;

  // TabContents (MUST BE LAST) ------------------------------------------------

  // The supporting objects need to outlive the TabContents dtor (as they may
  // be called upon during its execution). As a result, this must come last
  // in the list.
  scoped_ptr<TabContents> tab_contents_;

  DISALLOW_COPY_AND_ASSIGN(TabContentsWrapper);
};

#endif  // CHROME_BROWSER_UI_TAB_CONTENTS_TAB_CONTENTS_WRAPPER_H_
