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
#include "chrome/browser/ui/search/instant_page.h"
#include "chrome/common/instant_types.h"
#include "chrome/common/omnibox_focus_state.h"
#include "chrome/common/search_types.h"
#include "content/public/common/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/rect.h"

class BrowserInstantController;
class GURL;
class InstantService;
class InstantTab;
class Profile;

namespace content {
class WebContents;
}

namespace gfx {
class Rect;
}

// Macro used for logging debug events. |message| should be a std::string.
#define LOG_INSTANT_DEBUG_EVENT(controller, message) \
    controller->LogDebugEvent(message)

// InstantController drives Chrome Instant, i.e., the browser implementation of
// the Embedded Search API (see http://dev.chromium.org/embeddedsearch).
//
// In extended mode, InstantController maintains and coordinates an InstantTab
// instance of InstantPage. An InstantTab instance points to the currently
// active tab, if it supports the Embedded Search API. InstantTab is backed by a
// WebContents and it does not own that WebContents.
//
// InstantController is owned by Browser via BrowserInstantController.
class InstantController : public InstantPage::Delegate {
 public:
  explicit InstantController(BrowserInstantController* browser);
  virtual ~InstantController();

  // Sets the stored start-edge margin and width of the omnibox.
  void SetOmniboxBounds(const gfx::Rect& bounds);

  // Sends the current SearchProvider suggestion to the Instant page if any.
  void SetSuggestionToPrefetch(const InstantSuggestion& suggestion);

  // Notifies |instant_Tab_| to toggle voice search.
  void ToggleVoiceSearch();

  // Called if the browser is navigating to a search URL for |search_terms| with
  // search-term-replacement enabled. If |instant_tab_| can be used to process
  // the search, this does so and returns true. Else, returns false.
  bool SubmitQuery(const string16& search_terms);

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

  // See comments for |debug_events_| below.
  const std::list<std::pair<int64, std::string> >& debug_events() {
    return debug_events_;
  }

  // Gets the stored start-edge margin and width of the omnibox.
  const gfx::Rect omnibox_bounds() {
    return omnibox_bounds_;
  }

  // Used by BrowserInstantController to notify InstantController about the
  // instant support change event for the active web contents.
  void InstantSupportChanged(InstantSupportState instant_support);

 protected:
  // Accessors are made protected for testing purposes.
  virtual InstantTab* instant_tab() const;

  virtual Profile* profile() const;

 private:
  friend class InstantExtendedManualTest;
  friend class InstantTestBase;

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
  virtual void InstantSupportDetermined(
      const content::WebContents* contents,
      bool supports_instant) OVERRIDE;
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
  virtual void PasteIntoOmnibox(const content::WebContents* contents,
                                const string16& text) OVERRIDE;
  virtual void InstantPageLoadFailed(content::WebContents* contents) OVERRIDE;

  // Helper function to navigate the given contents to the local fallback
  // Instant URL and trim the history correctly.
  void RedirectToLocalNTP(content::WebContents* contents);

  // Helper for OmniboxFocusChanged. Commit or discard the overlay.
  void OmniboxLostFocus(gfx::NativeView view_gaining_focus);

  // If the active tab is an Instant search results page, sets |instant_tab_| to
  // point to it. Else, deletes any existing |instant_tab_|.
  void ResetInstantTab();

  // Sends theme info, omnibox bounds, etc. down to the Instant tab.
  void UpdateInfoForInstantTab();

  // Returns whether input is in progress, i.e. if the omnibox has focus and the
  // active tab is in mode SEARCH_SUGGESTIONS.
  bool IsInputInProgress() const;

  // Returns true if the local page is being used.
  bool UsingLocalPage() const;

  // Returns the InstantService for the browser profile.
  InstantService* GetInstantService() const;

  BrowserInstantController* const browser_;

  // The instance of InstantPage maintained by InstantController.
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
