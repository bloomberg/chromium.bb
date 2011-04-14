// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TAB_CONTENTS_TAB_CONTENTS_WRAPPER_H_
#define CHROME_BROWSER_UI_TAB_CONTENTS_TAB_CONTENTS_WRAPPER_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/printing/print_view_manager.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/tab_contents/tab_contents_observer.h"
#include "content/common/notification_registrar.h"

namespace prerender {
class PrerenderObserver;
}

namespace printing {
class PrintPreviewMessageHandler;
}

class AutocompleteHistoryManager;
class AutofillManager;
class AutomationTabHelper;
class DownloadTabHelper;
class Extension;
class ExtensionTabHelper;
class ExtensionWebNavigationTabObserver;
class FileSelectObserver;
class FindTabHelper;
class NavigationController;
class PasswordManager;
class PasswordManagerDelegate;
class SearchEngineTabHelper;
class TabContentsWrapperDelegate;
class TranslateTabHelper;

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

  static void RegisterUserPrefs(PrefService* prefs);

  // Initial title assigned to NavigationEntries from Navigate.
  static string16 GetDefaultTitle();

  // Returns a human-readable description the tab's loading state.
  string16 GetStatusText() const;

  // Create a TabContentsWrapper with the same state as this one. The returned
  // heap-allocated pointer is owned by the caller.
  TabContentsWrapper* Clone();

  // Helper to retrieve the existing instance that wraps a given TabContents.
  // Returns NULL if there is no such existing instance.
  // NOTE: This is not intended for general use. It is intended for situations
  // like callbacks from content/ where only a TabContents is available. In the
  // general case, please do NOT use this; plumb TabContentsWrapper through the
  // chrome/ code instead of TabContents.
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
  bool is_starred() const { return is_starred_; }

  // Tab Helpers ---------------------------------------------------------------

  AutocompleteHistoryManager* autocomplete_history_manager() {
    return autocomplete_history_manager_.get();
  }

  AutofillManager* autofill_manager() { return autofill_manager_.get(); }

  // Used only for testing/automation.
  AutomationTabHelper* automation_tab_helper() {
    return automation_tab_helper_.get();
  }

  DownloadTabHelper* download_tab_helper() {
    return download_tab_helper_.get();
  }

  ExtensionTabHelper* extension_tab_helper() {
    return extension_tab_helper_.get();
  }

  FindTabHelper* find_tab_helper() { return find_tab_helper_.get(); }

  PasswordManager* password_manager() { return password_manager_.get(); }

  printing::PrintViewManager* print_view_manager() {
    return print_view_manager_.get();
  }

  SearchEngineTabHelper* search_engine_tab_helper() {
    return search_engine_tab_helper_.get();
  }

  TranslateTabHelper* translate_tab_helper() {
    return translate_tab_helper_.get();
  }

  // Overrides -----------------------------------------------------------------

  // TabContentsObserver overrides:
  virtual void DidNavigateMainFramePostCommit(
      const NavigationController::LoadCommittedDetails& details,
      const ViewHostMsg_FrameNavigate_Params& params) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // NotificationObserver overrides:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

 private:
  // Internal helpers ----------------------------------------------------------

  // Message handlers.
  void OnPageContents(const GURL& url,
                      int32 page_id,
                      const string16& contents);
  void OnJSOutOfMemory();
  void OnRegisterProtocolHandler(const std::string& protocol,
                                 const GURL& url,
                                 const string16& title);
  void OnMsgThumbnail(const GURL& url,
                      const ThumbnailScore& score,
                      const SkBitmap& bitmap);

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

  // Tab Helpers ---------------------------------------------------------------
  // (These provide API for callers and have a getter function listed in the
  // "Tab Helpers" section in the member functions area, above.)

  scoped_ptr<AutocompleteHistoryManager> autocomplete_history_manager_;
  scoped_ptr<AutofillManager> autofill_manager_;
  scoped_ptr<AutomationTabHelper> automation_tab_helper_;
  scoped_ptr<DownloadTabHelper> download_tab_helper_;
  scoped_ptr<ExtensionTabHelper> extension_tab_helper_;
  scoped_ptr<FindTabHelper> find_tab_helper_;

  // PasswordManager and its delegate. The delegate must outlive the manager,
  // per documentation in password_manager.h.
  scoped_ptr<PasswordManagerDelegate> password_manager_delegate_;
  scoped_ptr<PasswordManager> password_manager_;

  // Handles print job for this contents.
  scoped_ptr<printing::PrintViewManager> print_view_manager_;

  scoped_ptr<SearchEngineTabHelper> search_engine_tab_helper_;
  scoped_ptr<TranslateTabHelper> translate_tab_helper_;

  // Per-tab observers ---------------------------------------------------------
  // (These provide no API for callers; objects that need to exist 1:1 with tabs
  // and silently do their thing live here.)

  scoped_ptr<FileSelectObserver> file_select_observer_;
  scoped_ptr<prerender::PrerenderObserver> prerender_observer_;
  scoped_ptr<printing::PrintPreviewMessageHandler> print_preview_;
  scoped_ptr<ExtensionWebNavigationTabObserver> webnavigation_observer_;

  // TabContents (MUST BE LAST) ------------------------------------------------

  // The supporting objects need to outlive the TabContents dtor (as they may
  // be called upon during its execution). As a result, this must come last
  // in the list.
  scoped_ptr<TabContents> tab_contents_;

  DISALLOW_COPY_AND_ASSIGN(TabContentsWrapper);
};

#endif  // CHROME_BROWSER_UI_TAB_CONTENTS_TAB_CONTENTS_WRAPPER_H_
