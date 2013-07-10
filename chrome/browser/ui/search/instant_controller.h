// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_INSTANT_CONTROLLER_H_
#define CHROME_BROWSER_UI_SEARCH_INSTANT_CONTROLLER_H_

#include <list>
#include <string>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "chrome/browser/search/instant_service_observer.h"
#include "chrome/browser/ui/search/instant_page.h"
#include "chrome/common/instant_types.h"
#include "chrome/common/omnibox_focus_state.h"
#include "chrome/common/search_types.h"
#include "content/public/common/page_transition_types.h"
#include "net/base/network_change_notifier.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/rect.h"
#include "url/gurl.h"

class BrowserInstantController;
class InstantNTP;
class InstantService;
class InstantTab;
class Profile;

namespace content {
class WebContents;
}

// Macro used for logging debug events. |message| should be a std::string.
#define LOG_INSTANT_DEBUG_EVENT(controller, message) \
    controller->LogDebugEvent(message)

// InstantController drives Chrome Instant, i.e., the browser implementation of
// the Embedded Search API (see http://dev.chromium.org/embeddedsearch).
//
// In extended mode, InstantController maintains and coordinates two
// instances of InstantPage:
//  (1) An InstantNTP instance which is a preloaded search page that will be
//      swapped-in the next time the user navigates to the New Tab Page. It is
//      never shown to the user in an uncommitted state.
//  (2) An InstantTab instance which points to the currently active tab, if it
//      supports the Embedded Search API.
//
// Both are backed by a WebContents. InstantNTP owns its WebContents and
// InstantTab does not.
//
// InstantController is owned by Browser via BrowserInstantController.
class InstantController : public InstantPage::Delegate,
                          public InstantServiceObserver {
 public:
  InstantController(BrowserInstantController* browser,
                    bool extended_enabled);
  virtual ~InstantController();

  // Releases and returns the NTP WebContents. May be NULL. Loads a new
  // WebContents for the NTP.
  scoped_ptr<content::WebContents> ReleaseNTPContents() WARN_UNUSED_RESULT;

  // Sets the stored start-edge margin and width of the omnibox.
  void SetOmniboxBounds(const gfx::Rect& bounds);

  // Called when the default search provider changes. Resets InstantNTP.
  void OnDefaultSearchProviderChanged();

  // Notifies |instant_Tab_| to toggle voice search.
  void ToggleVoiceSearch();

  // The ntp WebContents. May be NULL. InstantController retains ownership.
  content::WebContents* GetNTPContents() const;

  // Called if the browser is navigating to a search URL for |search_terms| with
  // search-term-replacement enabled. If |instant_tab_| can be used to process
  // the search, this does so and returns true. Else, returns false.
  bool SubmitQuery(const string16& search_terms);

  // If the network status changes, try to reset NTP and Overlay.
  void OnNetworkChanged(net::NetworkChangeNotifier::ConnectionType type);

  // Called to indicate that the omnibox focus state changed with the given
  // |reason|. If |focus_state| is FOCUS_NONE, |view_gaining_focus| is set to
  // the view gaining focus.
  void OmniboxFocusChanged(OmniboxFocusState focus_state,
                           OmniboxFocusChangeReason reason,
                           gfx::NativeView view_gaining_focus);

  // The search mode in the active tab has changed. Bind |instant_tab_| if the
  // |new_mode| reflects an Instant search results page.
  void SearchModeChanged(const SearchMode& old_mode,
                         const SearchMode& new_mode);

  // The user switched tabs. Bind |instant_tab_| if the newly active tab is an
  // Instant search results page.
  void ActiveTabChanged();

  // The user is about to switch tabs.
  void TabDeactivated(content::WebContents* contents);

  // Adds a new event to |debug_events_| and also DVLOG's it. Ensures that
  // |debug_events_| doesn't get too large.
  void LogDebugEvent(const std::string& info) const;

  // Resets list of debug events.
  void ClearDebugEvents();

  // Loads a new NTP to replace |ntp_|.
  void ReloadStaleNTP();

  // Returns the correct Instant URL to use from the following possibilities:
  //   o The default search engine's Instant URL
  //   o The local page (see GetLocalInstantURL())
  // Returns empty string if no valid Instant URL is available (this is only
  // possible in non-extended mode where we don't have a local page fall-back).
  virtual std::string GetInstantURL() const;

  // See comments for |debug_events_| below.
  const std::list<std::pair<int64, std::string> >& debug_events() {
    return debug_events_;
  }

  // Used by BrowserInstantController to notify InstantController about the
  // instant support change event for the active web contents.
  void InstantSupportChanged(InstantSupportState instant_support);

 protected:
  // Accessors are made protected for testing purposes.
  virtual bool extended_enabled() const;

  virtual InstantTab* instant_tab() const;
  virtual InstantNTP* ntp() const;

  virtual Profile* profile() const;

  // Returns true if Javascript is enabled and false otherwise.
  virtual bool IsJavascriptEnabled() const;

  // Returns true if the browser is in startup.
  virtual bool InStartup() const;

 private:
  friend class InstantExtendedManualTest;
  friend class InstantTestBase;
#define UNIT_F(test) FRIEND_TEST_ALL_PREFIXES(InstantControllerTest, test)
  UNIT_F(DoesNotSwitchToLocalNTPIfOnCurrentNTP);
  UNIT_F(DoesNotSwitchToLocalNTPIfOnLocalNTP);
  UNIT_F(IsJavascriptEnabled);
  UNIT_F(IsJavascriptEnabledChecksContentSettings);
  UNIT_F(IsJavascriptEnabledChecksPrefs);
  UNIT_F(PrefersRemoteNTPOnStartup);
  UNIT_F(SwitchesToLocalNTPIfJSDisabled);
  UNIT_F(SwitchesToLocalNTPIfNoInstantSupport);
  UNIT_F(SwitchesToLocalNTPIfNoNTPReady);
  UNIT_F(SwitchesToLocalNTPIfPathBad);
#undef UNIT_F
  FRIEND_TEST_ALL_PREFIXES(InstantExtendedTest, ExtendedModeIsOn);
  FRIEND_TEST_ALL_PREFIXES(InstantExtendedTest, MostVisited);
  FRIEND_TEST_ALL_PREFIXES(InstantExtendedTest, NTPIsPreloaded);
  FRIEND_TEST_ALL_PREFIXES(InstantExtendedTest, PreloadedNTPIsUsedInNewTab);
  FRIEND_TEST_ALL_PREFIXES(InstantExtendedTest, PreloadedNTPIsUsedInSameTab);
  FRIEND_TEST_ALL_PREFIXES(InstantExtendedTest, PreloadedNTPForWrongProvider);
  FRIEND_TEST_ALL_PREFIXES(InstantExtendedTest, PreloadedNTPRenderProcessGone);
  FRIEND_TEST_ALL_PREFIXES(InstantExtendedTest,
                           PreloadedNTPDoesntSupportInstant);
  FRIEND_TEST_ALL_PREFIXES(InstantExtendedTest, ProcessIsolation);
  FRIEND_TEST_ALL_PREFIXES(InstantExtendedTest, UnrelatedSiteInstance);
  FRIEND_TEST_ALL_PREFIXES(InstantExtendedTest, OnDefaultSearchProviderChanged);
  FRIEND_TEST_ALL_PREFIXES(InstantExtendedNetworkTest,
                           NTPReactsToNetworkChanges);
  FRIEND_TEST_ALL_PREFIXES(InstantExtendedTest,
                           AcceptingURLSearchDoesNotNavigate);
  FRIEND_TEST_ALL_PREFIXES(InstantExtendedTest, AcceptingJSSearchDoesNotRunJS);
  FRIEND_TEST_ALL_PREFIXES(InstantExtendedTest,
                           ReloadSearchAfterBackReloadsCorrectQuery);
  FRIEND_TEST_ALL_PREFIXES(InstantExtendedFirstTabTest,
                           RedirectToLocalOnLoadFailure);
  FRIEND_TEST_ALL_PREFIXES(InstantExtendedTest, KeyboardTogglesVoiceSearch);
  FRIEND_TEST_ALL_PREFIXES(InstantExtendedTest, HomeButtonAffectsMargin);
  FRIEND_TEST_ALL_PREFIXES(InstantExtendedTest, SearchReusesInstantTab);
  FRIEND_TEST_ALL_PREFIXES(InstantExtendedTest,
                           SearchDoesntReuseInstantTabWithoutSupport);
  FRIEND_TEST_ALL_PREFIXES(InstantExtendedTest,
                           TypedSearchURLDoesntReuseInstantTab);
  FRIEND_TEST_ALL_PREFIXES(InstantExtendedTest,
                           DispatchMVChangeEventWhileNavigatingBackToNTP);

  // Overridden from InstantPage::Delegate:
  // TODO(shishir): We assume that the WebContent's current RenderViewHost is
  // the RenderViewHost being created which is not always true. Fix this.
  virtual void InstantPageRenderViewCreated(
      const content::WebContents* contents) OVERRIDE;
  virtual void InstantSupportDetermined(
      const content::WebContents* contents,
      bool supports_instant) OVERRIDE;
  virtual void InstantPageRenderProcessGone(
      const content::WebContents* contents) OVERRIDE;
  virtual void InstantPageAboutToNavigateMainFrame(
      const content::WebContents* contents,
      const GURL& url) OVERRIDE;
  virtual void FocusOmnibox(const content::WebContents* contents,
                            OmniboxFocusState state) OVERRIDE;
  virtual void NavigateToURL(
      const content::WebContents* contents,
      const GURL& url,
      content::PageTransition transition,
      WindowOpenDisposition disposition,
      bool is_search_type) OVERRIDE;
  virtual void InstantPageLoadFailed(content::WebContents* contents) OVERRIDE;

  // Overridden from InstantServiceObserver:
  virtual void ThemeInfoChanged(const ThemeBackgroundInfo& theme_info) OVERRIDE;
  virtual void MostVisitedItemsChanged(
      const std::vector<InstantMostVisitedItem>& items) OVERRIDE;

  // Invoked by the InstantLoader when the Instant page wants to delete a
  // Most Visited item.
  virtual void DeleteMostVisitedItem(const GURL& url) OVERRIDE;

  // Invoked by the InstantLoader when the Instant page wants to undo a
  // Most Visited deletion.
  virtual void UndoMostVisitedDeletion(const GURL& url) OVERRIDE;

  // Invoked by the InstantLoader when the Instant page wants to undo all
  // Most Visited deletions.
  virtual void UndoAllMostVisitedDeletions() OVERRIDE;

  // Helper function to navigate the given contents to the local fallback
  // Instant URL and trim the history correctly.
  void RedirectToLocalNTP(content::WebContents* contents);

  // Helper for OmniboxFocusChanged. Commit or discard the overlay.
  void OmniboxLostFocus(gfx::NativeView view_gaining_focus);

  // Returns the local Instant URL. (Just a convenience wrapper around
  // chrome::GetLocalInstantURL.)
  virtual std::string GetLocalInstantURL() const;

  // Returns true if |page| has an up-to-date Instant URL and supports Instant.
  // Note that local URLs will not pass this check.
  bool PageIsCurrent(const InstantPage* page) const;

  // Recreates |ntp_| using |instant_url|.
  void ResetNTP(const std::string& instant_url);

  // Returns true if we should switch to using the local NTP.
  bool ShouldSwitchToLocalNTP() const;

  // If the active tab is an Instant search results page, sets |instant_tab_| to
  // point to it. Else, deletes any existing |instant_tab_|.
  void ResetInstantTab();

  // Sends theme info, omnibox bounds, font info, etc. down to the Instant tab.
  void UpdateInfoForInstantTab();

  // Returns whether input is in progress, i.e. if the omnibox has focus and the
  // active tab is in mode SEARCH_SUGGESTIONS.
  bool IsInputInProgress() const;

  // Returns true if the local page is being used.
  bool UsingLocalPage() const;

  // Returns the InstantService for the browser profile.
  InstantService* GetInstantService() const;

  BrowserInstantController* const browser_;

  // Whether the extended API and regular API are enabled. If both are false,
  // Instant is effectively disabled.
  const bool extended_enabled_;

  // The instances of InstantPage maintained by InstantController.
  scoped_ptr<InstantNTP> ntp_;
  scoped_ptr<InstantTab> instant_tab_;

  // Omnibox focus state.
  OmniboxFocusState omnibox_focus_state_;

  // The reason for the most recent omnibox focus change.
  OmniboxFocusChangeReason omnibox_focus_change_reason_;

  // The search model mode for the active tab.
  SearchMode search_mode_;

  // The start-edge margin and width of the omnibox, used by the page to align
  // its suggestions with the omnibox.
  gfx::Rect omnibox_bounds_;

  // List of events and their timestamps, useful in debugging Instant behaviour.
  mutable std::list<std::pair<int64, std::string> > debug_events_;

  DISALLOW_COPY_AND_ASSIGN(InstantController);
};

#endif  // CHROME_BROWSER_UI_SEARCH_INSTANT_CONTROLLER_H_
