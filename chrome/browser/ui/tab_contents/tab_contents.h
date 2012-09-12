// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TAB_CONTENTS_TAB_CONTENTS_H_
#define CHROME_BROWSER_UI_TAB_CONTENTS_TAB_CONTENTS_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/web_contents_observer.h"

class AlternateErrorPageTabObserver;
class AutocompleteHistoryManager;
class AutofillExternalDelegate;
class AutofillManager;
class AutomationTabHelper;
class BasePanelBrowserTest;
class BlockedContentTabHelper;
class BookmarkTabHelper;
class Browser;
class BrowserCommandsTabContentsCreator;
class BrowserLauncherItemControllerContentsCreator;
class BrowserTabstripTabContentsCreator;
class ChromeWebContentsHandler;
class ConstrainedWebDialogDelegateBase;
class ConstrainedWindowTabHelper;
class CoreTabHelper;
class ExtensionTabUtil;
class ExternalProtocolObserver;
class ExternalTabContainerWin;
class FaviconTabHelper;
class FindBackendTestContentsCreator;
class FindTabHelper;
class GeolocationPermissionContextTests;
class HistoryTabHelper;
class HungPluginTabHelper;
class InfoBarControllerContentsCreator;
class InfoBarTabHelper;
class InstantLoader;
class NavigationMetricsRecorder;
class OffscreenTabContentsCreator;
class OmniboxSearchHint;
class PanelHost;
class PasswordManager;
class PasswordManagerDelegate;
class PDFTabObserver;
class PepperBrokerObserver;
class PluginObserver;
class PrefsTabHelper;
class Profile;
class SadTabHelper;
class SearchEngineTabHelper;
class ShellWindow;
class SnapshotTabHelper;
class TabAutofillManagerDelegate;
class TabContentsSSLHelper;
class TabContentsTestHarness;
class TabSpecificContentSettings;
class TabStripModel;
class TabStripModelContentsCreator;
class ThumbnailGenerator;
class TranslateTabHelper;
class TranslationInfoBarTestContentsCreator;
class WebDialogGtk;
class WebDialogWindowControllerTabContentsCreator;
class WebIntentInlineDispositionBrowserTest;
class WebIntentPickerCocoa;
class WebIntentPickerController;
class WebIntentPickerGtk;
class WebUITestContentsCreator;
class ZoomController;

#if defined(ENABLE_ONE_CLICK_SIGNIN)
class OneClickSigninHelper;
#endif

namespace android_webview {
class AwBrowserDependencyFactoryImpl;
}

namespace browser_sync {
class SyncedTabDelegate;
}

namespace captive_portal {
class CaptivePortalTabHelper;
}

namespace chrome {
namespace search {
class SearchTabHelper;
}
}

namespace chrome_browser_net {
class LoadTimeStatsTabHelper;
}

namespace chromeos {
class SimpleWebViewDialog;
class WebUILoginView;
}

namespace extensions {
class WebAuthFlow;
class WebNavigationTabObserver;
}

namespace prerender {
class PrerenderContents;
class PrerenderTabHelper;
}

namespace printing {
class PrintViewManager;
class PrintPreviewMessageHandler;
}

namespace safe_browsing {
class SafeBrowsingTabObserver;
}

// Wraps WebContents and all of its supporting objects in order to control
// their ownership and lifetime.
//
// WARNING: Not every place where HTML can run has a TabContents. This class is
// *only* used in a visible, actual, tab inside a browser. Examples of things
// that do not have a TabContents include:
// - Extension background pages and popup bubbles
// - HTML notification bubbles
// - Screensavers on Chrome OS
// - Other random places we decide to display HTML over time
//
// Consider carefully whether your feature is something that makes sense only
// when a tab is displayed, or could make sense in other cases we use HTML. It
// may makes sense to push down into WebContents and make configurable, or at
// least to make easy for other WebContents hosts to include and support.
class TabContents : public content::WebContentsObserver {
 public:
  class Factory {
   private:
    // TabContents is going away <http://crbug.com/107201> so don't allow any
    // more code to construct instances. Explicitly befriend those who currently
    // do so.

    friend class android_webview::AwBrowserDependencyFactoryImpl;
    friend class BasePanelBrowserTest;
    friend class Browser;
    friend class BrowserCommandsTabContentsCreator;
    friend class BrowserLauncherItemControllerContentsCreator;
    friend class BrowserTabstripTabContentsCreator;
    friend class chromeos::SimpleWebViewDialog;
    friend class chromeos::WebUILoginView;
    friend class ChromeWebContentsHandler;
    friend class ConstrainedWebDialogDelegateBase;
    friend class extensions::WebAuthFlow;
    friend class ExtensionTabUtil;
    friend class ExternalTabContainerWin;
    friend class FindBackendTestContentsCreator;
    friend class GeolocationPermissionContextTests;
    friend class InfoBarControllerContentsCreator;
    friend class InstantLoader;
    friend class OffscreenTabContentsCreator;
    friend class PanelHost;
    friend class prerender::PrerenderContents;
    friend class ShellWindow;
    friend class TabContentsTestHarness;
    friend class TabStripModel;
    friend class TabStripModelContentsCreator;
    friend class TranslationInfoBarTestContentsCreator;
    friend class WebDialogGtk;
    friend class WebDialogWindowControllerTabContentsCreator;
    friend class WebIntentInlineDispositionBrowserTest;
    friend class WebIntentPickerCocoa;
    friend class WebIntentPickerGtk;
    friend class WebUITestContentsCreator;

    static TabContents* CreateTabContents(content::WebContents* contents);
    static TabContents* CloneTabContents(TabContents* contents);
  };

  virtual ~TabContents();

  // Helper to retrieve the existing instance that owns a given WebContents.
  // Returns NULL if there is no such existing instance.
  // NOTE: This is not intended for general use. It is intended for situations
  // like callbacks from content/ where only a WebContents is available. In the
  // general case, please do NOT use this; plumb TabContents through the chrome/
  // code instead of WebContents.
  static TabContents* FromWebContents(content::WebContents* contents);
  static const TabContents* FromWebContents(
      const content::WebContents* contents);

  // Returns the WebContents that this owns.
  content::WebContents* web_contents() const;

  // Returns the Profile that is associated with this TabContents.
  Profile* profile() const;

  // True if this TabContents is being torn down.
  bool in_destructor() const { return in_destructor_; }

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

#if defined(ENABLE_CAPTIVE_PORTAL_DETECTION)
  captive_portal::CaptivePortalTabHelper* captive_portal_tab_helper() {
    return captive_portal_tab_helper_.get();
  }
#endif

  ConstrainedWindowTabHelper* constrained_window_tab_helper() {
    return constrained_window_tab_helper_.get();
  }

  CoreTabHelper* core_tab_helper() { return core_tab_helper_.get(); }

  FaviconTabHelper* favicon_tab_helper() { return favicon_tab_helper_.get(); }
  FindTabHelper* find_tab_helper() { return find_tab_helper_.get(); }
  HistoryTabHelper* history_tab_helper() { return history_tab_helper_.get(); }
  HungPluginTabHelper* hung_plugin_tab_helper() {
    return hung_plugin_tab_helper_.get();
  }
  InfoBarTabHelper* infobar_tab_helper() { return infobar_tab_helper_.get(); }

#if defined(ENABLE_ONE_CLICK_SIGNIN)
  OneClickSigninHelper* one_click_signin_helper() {
    return one_click_signin_helper_.get();
  }
#endif

  PasswordManager* password_manager() { return password_manager_.get(); }
  PrefsTabHelper* prefs_tab_helper() { return prefs_tab_helper_.get(); }

  prerender::PrerenderTabHelper* prerender_tab_helper() {
    return prerender_tab_helper_.get();
  }

  printing::PrintViewManager* print_view_manager() {
    return print_view_manager_.get();
  }

  SadTabHelper* sad_tab_helper() { return sad_tab_helper_.get(); }

  SearchEngineTabHelper* search_engine_tab_helper() {
    return search_engine_tab_helper_.get();
  }

  chrome::search::SearchTabHelper* search_tab_helper() {
    return search_tab_helper_.get();
  }

  SnapshotTabHelper* snapshot_tab_helper() {
    return snapshot_tab_helper_.get();
  }

  TabContentsSSLHelper* ssl_helper() { return ssl_helper_.get(); }

  browser_sync::SyncedTabDelegate* synced_tab_delegate() {
    return synced_tab_delegate_.get();
  }

  TabSpecificContentSettings* content_settings() {
    return content_settings_.get();
  }

  // NOTE: This returns NULL unless in-browser thumbnail generation is enabled.
  ThumbnailGenerator* thumbnail_generator() {
    return thumbnail_generator_.get();
  }

  TranslateTabHelper* translate_tab_helper() {
    return translate_tab_helper_.get();
  }

  WebIntentPickerController* web_intent_picker_controller() {
    return web_intent_picker_controller_.get();
  }

  ZoomController* zoom_controller() {
    return zoom_controller_.get();
  }

  // Overrides -----------------------------------------------------------------

  // content::WebContentsObserver overrides:
  virtual void WebContentsDestroyed(content::WebContents* tab) OVERRIDE;

 private:
  friend class TabContentsFactory;

  // Takes ownership of |contents|, which must be heap-allocated (as it lives
  // in a scoped_ptr) and can not be NULL.
  explicit TabContents(content::WebContents* contents);

  // Create a TabContents with the same state as this one. The returned
  // heap-allocated pointer is owned by the caller.
  TabContents* Clone();

  // Tab Helpers ---------------------------------------------------------------
  // (These provide API for callers and have a getter function listed in the
  // "Tab Helpers" section in the member functions area, above.)

  scoped_ptr<AutocompleteHistoryManager> autocomplete_history_manager_;
  scoped_refptr<AutofillManager> autofill_manager_;
  scoped_ptr<TabAutofillManagerDelegate> autofill_delegate_;
  scoped_ptr<AutofillExternalDelegate> autofill_external_delegate_;
  scoped_ptr<AutomationTabHelper> automation_tab_helper_;
  scoped_ptr<BlockedContentTabHelper> blocked_content_tab_helper_;
  scoped_ptr<BookmarkTabHelper> bookmark_tab_helper_;
  scoped_ptr<chrome_browser_net::LoadTimeStatsTabHelper>
  load_time_stats_tab_helper_;
#if defined(ENABLE_CAPTIVE_PORTAL_DETECTION)
  scoped_ptr<captive_portal::CaptivePortalTabHelper> captive_portal_tab_helper_;
#endif
  scoped_ptr<ConstrainedWindowTabHelper> constrained_window_tab_helper_;
  scoped_ptr<CoreTabHelper> core_tab_helper_;
  scoped_ptr<FaviconTabHelper> favicon_tab_helper_;
  scoped_ptr<FindTabHelper> find_tab_helper_;
  scoped_ptr<HistoryTabHelper> history_tab_helper_;
  scoped_ptr<HungPluginTabHelper> hung_plugin_tab_helper_;
  scoped_ptr<InfoBarTabHelper> infobar_tab_helper_;

  // PasswordManager and its delegate. The delegate must outlive the manager,
  // per documentation in password_manager.h.
  scoped_ptr<PasswordManagerDelegate> password_manager_delegate_;
  scoped_ptr<PasswordManager> password_manager_;

  scoped_ptr<PrefsTabHelper> prefs_tab_helper_;
  scoped_ptr<prerender::PrerenderTabHelper> prerender_tab_helper_;

  // Handles print job for this contents.
  scoped_ptr<printing::PrintViewManager> print_view_manager_;

  scoped_ptr<SadTabHelper> sad_tab_helper_;
  scoped_ptr<SearchEngineTabHelper> search_engine_tab_helper_;
  scoped_ptr<chrome::search::SearchTabHelper> search_tab_helper_;
  scoped_ptr<SnapshotTabHelper> snapshot_tab_helper_;
  scoped_ptr<TabContentsSSLHelper> ssl_helper_;
  scoped_ptr<browser_sync::SyncedTabDelegate> synced_tab_delegate_;

  // The TabSpecificContentSettings object is used to query the blocked content
  // state by various UI elements.
  scoped_ptr<TabSpecificContentSettings> content_settings_;

  scoped_ptr<ThumbnailGenerator> thumbnail_generator_;
  scoped_ptr<TranslateTabHelper> translate_tab_helper_;

  // Handles displaying a web intents picker to the user.
  scoped_ptr<WebIntentPickerController> web_intent_picker_controller_;

  scoped_ptr<ZoomController> zoom_controller_;

  // Per-tab observers ---------------------------------------------------------
  // (These provide no API for callers; objects that need to exist 1:1 with tabs
  // and silently do their thing live here.)

  scoped_ptr<AlternateErrorPageTabObserver> alternate_error_page_tab_observer_;
  scoped_ptr<extensions::WebNavigationTabObserver> webnavigation_observer_;
  scoped_ptr<ExternalProtocolObserver> external_protocol_observer_;
  scoped_ptr<NavigationMetricsRecorder> navigation_metrics_recorder_;
  scoped_ptr<OmniboxSearchHint> omnibox_search_hint_;
#if defined(ENABLE_ONE_CLICK_SIGNIN)
  scoped_ptr<OneClickSigninHelper> one_click_signin_helper_;
#endif
  scoped_ptr<PDFTabObserver> pdf_tab_observer_;
  scoped_ptr<PepperBrokerObserver> pepper_broker_observer_;
  scoped_ptr<PluginObserver> plugin_observer_;
  scoped_ptr<printing::PrintPreviewMessageHandler> print_preview_;
  scoped_ptr<safe_browsing::SafeBrowsingTabObserver>
      safe_browsing_tab_observer_;

  // WebContents (MUST BE LAST) ------------------------------------------------

  // If true, we're running the destructor.
  bool in_destructor_;

  // The supporting objects need to outlive the WebContents dtor (as they may
  // be called upon during its execution). As a result, this must come last
  // in the list.
  scoped_ptr<content::WebContents> web_contents_;

  DISALLOW_COPY_AND_ASSIGN(TabContents);
};

#endif  // CHROME_BROWSER_UI_TAB_CONTENTS_TAB_CONTENTS_H_
