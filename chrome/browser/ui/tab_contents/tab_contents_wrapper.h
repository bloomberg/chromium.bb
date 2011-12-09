// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TAB_CONTENTS_TAB_CONTENTS_WRAPPER_H_
#define CHROME_BROWSER_UI_TAB_CONTENTS_TAB_CONTENTS_WRAPPER_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/tab_contents/tab_contents_observer.h"

class AlternateErrorPageTabObserver;
class AutocompleteHistoryManager;
class AutofillManager;
class AutofillExternalDelegate;
class AutomationTabHelper;
class BlockedContentTabHelper;
class BookmarkTabHelper;
class ConstrainedWindowTabHelper;
class CoreTabHelper;
class DownloadRequestLimiterObserver;
class ExtensionTabHelper;
class ExtensionWebNavigationTabObserver;
class ExternalProtocolObserver;
class FaviconTabHelper;
class FindTabHelper;
class HistoryTabHelper;
class InfoBarTabHelper;
class OmniboxSearchHint;
class PasswordManager;
class PasswordManagerDelegate;
class PDFTabObserver;
class PluginObserver;
class PrefService;
class PrefsTabHelper;
class Profile;
class RestoreTabHelper;
class SadTabObserver;
class SearchEngineTabHelper;
class SnapshotTabHelper;
class TabContentsSSLHelper;
class TabContentsWrapperSyncedTabDelegate;
class TabSpecificContentSettings;
class ThumbnailGenerator;
class TranslateTabHelper;
class WebIntentPickerController;

namespace IPC {
class Message;
}

namespace prerender {
class PrerenderTabHelper;
}

namespace printing {
class PrintViewManager;
class PrintPreviewMessageHandler;
}

namespace safe_browsing {
class SafeBrowsingTabObserver;
}

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
  virtual ~TabContentsWrapper();

  // Used to retrieve this object from |tab_contents_|, which is placed in
  // its property bag to avoid adding additional interfaces.
  static base::PropertyAccessor<TabContentsWrapper*>* property_accessor();

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
  static const TabContentsWrapper* GetCurrentWrapperForContents(
      const TabContents* contents);

  TabContents* tab_contents() const { return tab_contents_.get(); }
  NavigationController& controller() const {
    return tab_contents()->controller();
  }
  TabContentsView* view() const { return tab_contents()->view(); }
  RenderViewHost* render_view_host() const {
    return tab_contents()->render_view_host();
  }
  WebUI* web_ui() const { return tab_contents()->web_ui(); }

  Profile* profile() const;

  // Tab Helpers ---------------------------------------------------------------

  AutocompleteHistoryManager* autocomplete_history_manager() {
    return autocomplete_history_manager_.get();
  }

  AutofillManager* autofill_manager() { return autofill_manager_.get(); }

  // Used only for testing/automation.
  AutomationTabHelper* automation_tab_helper() {
    return automation_tab_helper_.get();
  }

  BlockedContentTabHelper* blocked_content_tab_helper() {
    return blocked_content_tab_helper_.get();
  }

  BookmarkTabHelper* bookmark_tab_helper() {
    return bookmark_tab_helper_.get();
  }

  ConstrainedWindowTabHelper* constrained_window_tab_helper() {
    return constrained_window_tab_helper_.get();
  }

  CoreTabHelper* core_tab_helper() { return core_tab_helper_.get(); }

  ExtensionTabHelper* extension_tab_helper() {
    return extension_tab_helper_.get();
  }

  const ExtensionTabHelper* extension_tab_helper() const {
    return extension_tab_helper_.get();
  }

  FaviconTabHelper* favicon_tab_helper() { return favicon_tab_helper_.get(); }
  FindTabHelper* find_tab_helper() { return find_tab_helper_.get(); }
  HistoryTabHelper* history_tab_helper() { return history_tab_helper_.get(); }
  InfoBarTabHelper* infobar_tab_helper() { return infobar_tab_helper_.get(); }
  PasswordManager* password_manager() { return password_manager_.get(); }
  PrefsTabHelper* prefs_tab_helper() { return prefs_tab_helper_.get(); }

  prerender::PrerenderTabHelper* prerender_tab_helper() {
    return prerender_tab_helper_.get();
  }

  printing::PrintViewManager* print_view_manager() {
    return print_view_manager_.get();
  }

  RestoreTabHelper* restore_tab_helper() {
    return restore_tab_helper_.get();
  }

  const RestoreTabHelper* restore_tab_helper() const {
    return restore_tab_helper_.get();
  }

  SearchEngineTabHelper* search_engine_tab_helper() {
    return search_engine_tab_helper_.get();
  }

  SnapshotTabHelper* snapshot_tab_helper() {
    return snapshot_tab_helper_.get();
  }

  TabContentsSSLHelper* ssl_helper() { return ssl_helper_.get(); }

  TabContentsWrapperSyncedTabDelegate* synced_tab_delegate() {
    return synced_tab_delegate_.get();
  }

  TabSpecificContentSettings* content_settings() {
    return content_settings_.get();
  }

  TranslateTabHelper* translate_tab_helper() {
    return translate_tab_helper_.get();
  }

  WebIntentPickerController* web_intent_picker_controller() {
    return web_intent_picker_controller_.get();
  }

  // Overrides -----------------------------------------------------------------

  // TabContentsObserver overrides:
  virtual void TabContentsDestroyed(TabContents* tab) OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(
      PrefsTabHelperTest, OverridePrefsOnViewCreation);

  // Tab Helpers ---------------------------------------------------------------
  // (These provide API for callers and have a getter function listed in the
  // "Tab Helpers" section in the member functions area, above.)

  scoped_ptr<AutocompleteHistoryManager> autocomplete_history_manager_;
  scoped_refptr<AutofillManager> autofill_manager_;
  scoped_ptr<AutofillExternalDelegate> autofill_external_delegate_;
  scoped_ptr<AutomationTabHelper> automation_tab_helper_;
  scoped_ptr<BlockedContentTabHelper> blocked_content_tab_helper_;
  scoped_ptr<BookmarkTabHelper> bookmark_tab_helper_;
  scoped_ptr<ConstrainedWindowTabHelper> constrained_window_tab_helper_;
  scoped_ptr<CoreTabHelper> core_tab_helper_;
  scoped_ptr<ExtensionTabHelper> extension_tab_helper_;
  scoped_ptr<FaviconTabHelper> favicon_tab_helper_;
  scoped_ptr<FindTabHelper> find_tab_helper_;
  scoped_ptr<HistoryTabHelper> history_tab_helper_;
  scoped_ptr<InfoBarTabHelper> infobar_tab_helper_;

  // PasswordManager and its delegate. The delegate must outlive the manager,
  // per documentation in password_manager.h.
  scoped_ptr<PasswordManagerDelegate> password_manager_delegate_;
  scoped_ptr<PasswordManager> password_manager_;

  scoped_ptr<PrefsTabHelper> prefs_tab_helper_;
  scoped_ptr<prerender::PrerenderTabHelper> prerender_tab_helper_;

  // Handles print job for this contents.
  scoped_ptr<printing::PrintViewManager> print_view_manager_;

  scoped_ptr<RestoreTabHelper> restore_tab_helper_;

  // Handles displaying a web intents picker to the user.
  scoped_ptr<WebIntentPickerController> web_intent_picker_controller_;

  scoped_ptr<SearchEngineTabHelper> search_engine_tab_helper_;
  scoped_ptr<SnapshotTabHelper> snapshot_tab_helper_;
  scoped_ptr<TabContentsSSLHelper> ssl_helper_;
  scoped_ptr<TabContentsWrapperSyncedTabDelegate> synced_tab_delegate_;

  // The TabSpecificContentSettings object is used to query the blocked content
  // state by various UI elements.
  scoped_ptr<TabSpecificContentSettings> content_settings_;

  scoped_ptr<TranslateTabHelper> translate_tab_helper_;

  // Per-tab observers ---------------------------------------------------------
  // (These provide no API for callers; objects that need to exist 1:1 with tabs
  // and silently do their thing live here.)

  scoped_ptr<AlternateErrorPageTabObserver> alternate_error_page_tab_observer_;
  scoped_ptr<DownloadRequestLimiterObserver> download_request_limiter_observer_;
  scoped_ptr<ExtensionWebNavigationTabObserver> webnavigation_observer_;
  scoped_ptr<ExternalProtocolObserver> external_protocol_observer_;
  scoped_ptr<OmniboxSearchHint> omnibox_search_hint_;
  scoped_ptr<PDFTabObserver> pdf_tab_observer_;
  scoped_ptr<PluginObserver> plugin_observer_;
  scoped_ptr<printing::PrintPreviewMessageHandler> print_preview_;
  scoped_ptr<SadTabObserver> sad_tab_observer_;
  scoped_ptr<safe_browsing::SafeBrowsingTabObserver>
      safe_browsing_tab_observer_;
  scoped_ptr<ThumbnailGenerator> thumbnail_generation_observer_;

  // TabContents (MUST BE LAST) ------------------------------------------------

  // If true, we're running the destructor.
  bool in_destructor_;

  // The supporting objects need to outlive the TabContents dtor (as they may
  // be called upon during its execution). As a result, this must come last
  // in the list.
  scoped_ptr<TabContents> tab_contents_;

  DISALLOW_COPY_AND_ASSIGN(TabContentsWrapper);
};

#endif  // CHROME_BROWSER_UI_TAB_CONTENTS_TAB_CONTENTS_WRAPPER_H_
