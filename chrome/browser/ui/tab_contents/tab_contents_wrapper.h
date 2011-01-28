// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TAB_CONTENTS_TAB_CONTENTS_WRAPPER_H_
#define CHROME_BROWSER_UI_TAB_CONTENTS_TAB_CONTENTS_WRAPPER_H_
#pragma once

#include "base/scoped_ptr.h"
#include "base/compiler_specific.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_observer.h"

class Extension;
class NavigationController;
class PasswordManager;
class PasswordManagerDelegate;
class TabContentsDelegate;

// Wraps TabContents and all of its supporting objects in order to control
// their ownership and lifetime, while allowing TabContents to remain generic
// and re-usable in other projects.
// TODO(pinkerton): Eventually, this class will become TabContents as far as
// the browser front-end is concerned, and the current TabContents will be
// renamed to something like WebPage or WebView (ben's suggestions).
class TabContentsWrapper : public TabContentsObserver {
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

  TabContents* tab_contents() const { return tab_contents_.get(); }
  NavigationController& controller() const {
    return tab_contents()->controller();
  }
  TabContentsView* view() const { return tab_contents()->view(); }
  RenderViewHost* render_view_host() const {
    return tab_contents()->render_view_host();
  }
  Profile* profile() const { return tab_contents()->profile(); }
  TabContentsDelegate* delegate() const { return tab_contents()->delegate(); }
  void set_delegate(TabContentsDelegate* d) { tab_contents()->set_delegate(d); }

  // Convenience methods until extensions are removed from TabContents.
  void SetExtensionAppById(const std::string& extension_app_id) {
    tab_contents()->SetExtensionAppById(extension_app_id);
  }
  const Extension* extension_app() const {
    return tab_contents()->extension_app();
  }
  bool is_app() const { return tab_contents()->is_app(); }

  // Returns the PasswordManager, creating it if necessary.
  PasswordManager* GetPasswordManager();

  // TabContentsObserver overrides:
  virtual void NavigateToPendingEntry() OVERRIDE;

 private:
  // PasswordManager and its delegate, lazily created. The delegate must
  // outlive the manager, per documentation in password_manager.h.
  scoped_ptr<PasswordManagerDelegate> password_manager_delegate_;
  scoped_ptr<PasswordManager> password_manager_;

  // The supporting objects need to outlive the TabContents dtor (as they may
  // be called upon during its execution). As a result, this must come last
  // in the list.
  scoped_ptr<TabContents> tab_contents_;
};

#endif  // CHROME_BROWSER_UI_TAB_CONTENTS_TAB_CONTENTS_WRAPPER_H_
