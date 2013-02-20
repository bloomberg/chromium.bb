// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INSTANT_INSTANT_CONTROLLER_H_
#define CHROME_BROWSER_INSTANT_INSTANT_CONTROLLER_H_

#include <list>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "base/time.h"
#include "base/timer.h"
#include "chrome/browser/instant/instant_commit_type.h"
#include "chrome/browser/instant/instant_model.h"
#include "chrome/browser/instant/instant_page.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/common/instant_types.h"
#include "chrome/common/search_types.h"
#include "content/public/common/page_transition_types.h"
#include "googleurl/src/gurl.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/rect.h"

struct AutocompleteMatch;
class AutocompleteProvider;
class InstantNTP;
class InstantOverlay;
class InstantTab;
class TemplateURL;

namespace chrome {
class BrowserInstantController;
}

namespace content {
class WebContents;
}

// Macro used for logging debug events. |message| should be a std::string.
#define LOG_INSTANT_DEBUG_EVENT(controller, message) \
    controller->LogDebugEvent(message)

// InstantController drives Chrome Instant, i.e., the browser implementation of
// the Embedded Search API (see http://dev.chromium.org/embeddedsearch).
//
// In extended mode, InstantController maintains and coordinates three
// instances of InstantPage:
//  (1) An InstantOverlay instance that is used to show search suggestions and
//      results in an overlay over a non-search page.
//  (2) An InstantNTP instance which is a preloaded sarch page that will be
//      swapped-in the next time the user navigates to the New Tab Page. It is
//      never shown to the user in an uncommitted state.
//  (3) An InstantTab instance which points to the currently active tab, if it
//      supports the Embedded Search API.
//
// All three are backed by a WebContents. InstantOverlay and InstantNTP own
// their corresponding WebContents; InstantTab does not. In non-extended mode,
// only an InstantOverlay instance is kept.
//
// InstantController is owned by Browser via BrowserInstantController.
class InstantController : public InstantPage::Delegate {
 public:
  InstantController(chrome::BrowserInstantController* browser,
                    bool extended_enabled);
  virtual ~InstantController();

  // Invoked as the user types into the omnibox. |user_text| is what the user
  // has typed. |full_text| is what the omnibox is showing. These may differ if
  // the user typed only some text, and the rest was inline autocompleted. If
  // |verbatim| is true, search results are shown for the exact omnibox text,
  // rather than the best guess as to what the user means. Returns true if the
  // update is accepted (i.e., if |match| is a search rather than a URL).
  // |is_keyword_search| is true if keyword searching is in effect.
  bool Update(const AutocompleteMatch& match,
              const string16& user_text,
              const string16& full_text,
              size_t selection_start,
              size_t selection_end,
              bool verbatim,
              bool user_input_in_progress,
              bool omnibox_popup_is_open,
              bool escape_pressed,
              bool is_keyword_search);

  // Releases and returns the NTP WebContents. May be NULL. Loads a new
  // WebContents for the NTP.
  scoped_ptr<content::WebContents> ReleaseNTPContents() WARN_UNUSED_RESULT;

  // Sets the bounds of the omnibox popup, in screen coordinates.
  void SetPopupBounds(const gfx::Rect& bounds);

  // Sets the start and end margins of the omnibox text area.
  void SetMarginSize(int start, int end);

  // Send autocomplete results from |providers| to the preview page.
  void HandleAutocompleteResults(
      const std::vector<AutocompleteProvider*>& providers);

  // Called when the user presses up or down. |count| is a repeat count,
  // negative for moving up, positive for moving down. Returns true if Instant
  // handled the key press.
  bool OnUpOrDownKeyPressed(int count);

  // Called when the user has arrowed into the suggestions but wants to cancel,
  // typically by pressing ESC. The omnibox text is expected to have been
  // reverted to |full_text| by the OmniboxEditModel prior to calling this.
  // |match| is the match reverted to.
  void OnCancel(const AutocompleteMatch& match,
                const string16& full_text);

  // The preview WebContents. May be NULL. InstantController retains ownership.
  content::WebContents* GetPreviewContents() const;

  // Returns true if the Instant overlay is showing a search results preview.
  bool IsPreviewingSearchResults() const;

  // If the preview is showing search results, commits the preview, calling
  // CommitInstant() on the browser, and returns true. Else, returns false.
  bool CommitIfPossible(InstantCommitType type);

  // Called to indicate that the omnibox focus state changed with the given
  // |reason|. If |focus_state| is FOCUS_NONE, |view_gaining_focus| is set to
  // the view gaining focus.
  void OmniboxFocusChanged(OmniboxFocusState focus_state,
                           OmniboxFocusChangeReason reason,
                           gfx::NativeView view_gaining_focus);

  // The search mode in the active tab has changed. Pass the message down to
  // the overlay which will notify the renderer. Create |instant_tab_| if the
  // |new_mode| reflects an Instant search results page.
  void SearchModeChanged(const chrome::search::Mode& old_mode,
                         const chrome::search::Mode& new_mode);

  // The user switched tabs. Hide the preview. Create |instant_tab_| if the
  // newly active tab is an Instant search results page.
  void ActiveTabChanged();

  // The user is about to switch tabs. Commit the preview if needed.
  void TabDeactivated(content::WebContents* contents);

  // Sets whether Instant should show result previews. |use_local_preview_only|
  // will force the use of kLocalOmniboxPopupURL as the Instant URL and is only
  // applicable if |extended_enabled_| is true.
  void SetInstantEnabled(bool instant_enabled, bool use_local_preview_only);

  // The theme has changed. Pass the message to the preview page.
  void ThemeChanged(const ThemeBackgroundInfo& theme_info);

  // Called when someone else swapped in a different contents in the |overlay_|.
  void SwappedOverlayContents();

  // Called when contents for |overlay_| received focus.
  void FocusedOverlayContents();

  // Called when the |overlay_| might be stale. If it's actually stale, and the
  // omnibox doesn't have focus, and the preview isn't showing, the |overlay_|
  // is deleted and recreated. Else the refresh is skipped.
  void ReloadOverlayIfStale();

  // Adds a new event to |debug_events_| and also DVLOG's it. Ensures that
  // |debug_events_| doesn't get too large.
  void LogDebugEvent(const std::string& info) const;

  // See comments for |debug_events_| below.
  const std::list<std::pair<int64, std::string> >& debug_events() {
    return debug_events_;
  }

  // Returns the transition type of the last AutocompleteMatch passed to Update.
  content::PageTransition last_transition_type() const {
    return last_transition_type_;
  }

  // Non-const for Add/RemoveObserver only.  Other model changes should only
  // happen through the InstantController interface.
  InstantModel* model() { return &model_; }

 private:
  FRIEND_TEST_ALL_PREFIXES(InstantTest, OmniboxFocusLoadsInstant);
  FRIEND_TEST_ALL_PREFIXES(InstantTest, SetWithTemplateURL);
  FRIEND_TEST_ALL_PREFIXES(InstantTest, NonInstantSearchProvider);
  FRIEND_TEST_ALL_PREFIXES(InstantTest, InstantOverlayRefresh);
  FRIEND_TEST_ALL_PREFIXES(InstantExtendedTest, ExtendedModeIsOn);
  FRIEND_TEST_ALL_PREFIXES(InstantExtendedTest, OmniboxFocusLoadsInstant);
  FRIEND_TEST_ALL_PREFIXES(InstantExtendedTest, NTPIsPreloaded);
  FRIEND_TEST_ALL_PREFIXES(InstantExtendedTest, PreloadedNTPIsUsedInNewTab);
  FRIEND_TEST_ALL_PREFIXES(InstantExtendedTest, PreloadedNTPIsUsedInSameTab);
  FRIEND_TEST_ALL_PREFIXES(InstantExtendedTest, ProcessIsolation);
  FRIEND_TEST_ALL_PREFIXES(InstantExtendedTest, UnrelatedSiteInstance);

  // Overridden from InstantPage::Delegate:
  // TODO(shishir): We assume that the WebContent's current RenderViewHost is
  // the RenderViewHost being created which is not always true. Fix this.
  virtual void InstantPageRenderViewCreated(
      const content::WebContents* contents) OVERRIDE;
  virtual void InstantSupportDetermined(
      const content::WebContents* contents,
      bool supports_instant) OVERRIDE;
  virtual void InstantPageRenderViewGone(
      const content::WebContents* contents) OVERRIDE;
  virtual void InstantPageAboutToNavigateMainFrame(
      const content::WebContents* contents,
      const GURL& url) OVERRIDE;
  virtual void SetSuggestions(
      const content::WebContents* contents,
      const std::vector<InstantSuggestion>& suggestions) OVERRIDE;
  virtual void ShowInstantOverlay(
      const content::WebContents* contents,
      InstantShownReason reason,
      int height,
      InstantSizeUnits units) OVERRIDE;
  virtual void StartCapturingKeyStrokes(
      const content::WebContents* contents) OVERRIDE;
  virtual void StopCapturingKeyStrokes(content::WebContents* contents) OVERRIDE;
  virtual void NavigateToURL(
      const content::WebContents* contents,
      const GURL& url,
      content::PageTransition transition) OVERRIDE;

  // Helper for OmniboxFocusChanged. Commit or discard the preview.
  void OmniboxLostFocus(gfx::NativeView view_gaining_focus);

  // Creates a new NTP, using the instant_url property of the default
  // TemplateURL.
  void ResetNTP();

  // Ensures that |overlay_| uses the Instant URL returned by GetInstantURL(),
  // creating a new overlay if necessary. In extended mode, will fallback to
  // using the kLocalOmniboxPopupURL as the Instant URL in case GetInstantURL()
  // returns false. Returns true if an Instant URL could be determined.
  // For |ignore_blacklist| look at comments in |GetInstantURL|.
  bool EnsureOverlayIsCurrent(bool ignore_blacklist);

  // Recreates the |overlay_| with |instant_url|. Note that |overlay_| is
  // deleted in this call.
  void CreateOverlay(const std::string& instant_url,
                     const content::WebContents* active_tab);

  // If the |overlay_| being used is in fallback mode, it will be switched back
  // to the remote overlay if the overlay is not showing and the omnibox does
  // not have focus.
  void MaybeSwitchToRemoteOverlay();

  // If the active tab is an Instant search results page, sets |instant_tab_| to
  // point to it. Else, deletes any existing |instant_tab_|.
  void ResetInstantTab();

  // Hide the preview. Also sends an onchange event (with blank query) to the
  // preview, telling it to clear out results for any old queries.
  void HideOverlay();

  // Like HideOverlay(), but doesn't call OnStaleOverlay(). Use HideOverlay()
  // unless you are going to call overlay_.reset() yourself subsequently.
  void HideInternal();

  // Counterpart to HideOverlay(). Asks the |browser_| to display the preview
  // with the given |height| in |units|.
  void ShowOverlay(InstantShownReason reason,
                   int height,
                   InstantSizeUnits units);

  // Send the omnibox popup bounds to the page.
  void SendPopupBoundsToPage();

  // Determines the Instant URL based on a number of factors:
  // If |extended_enabled_|:
  //   - If |use_local_preview_only_| is true return kLocalOmniboxPopupURL, else
  //   - If the Instant URL is specified by command line, returns it, else
  //   - If the default Instant URL is present returns it.
  // If !|extended_enabled_|:
  //   - If the Instant URL is specified by command line, returns it, else
  //   - If the default Instant URL is present returns it.
  //
  // If |ignore_blacklist| is set to true, Instant URLs are not filtered through
  // the blacklist.
  //
  // Returns true if a valid Instant URL could be found that is not blacklisted.
  bool GetInstantURL(Profile* profile,
                     bool ignore_blacklist,
                     std::string* instant_url) const;

  // Adds the URL for the page to the blacklist. Deletes the contents held and
  // recreates a new page.
  void BlacklistAndResetOverlay();
  void BlacklistAndResetNTP();

  // Removes |url| from the blacklist.
  void RemoveFromBlacklist(const std::string& url);

  InstantOverlay* overlay() const { return overlay_.get(); }
  InstantTab* instant_tab() const { return instant_tab_.get(); }
  InstantNTP* ntp() const { return ntp_.get(); }

  chrome::BrowserInstantController* const browser_;

  // Whether the extended API and regular API are enabled. If both are false,
  // Instant is effectively disabled.
  const bool extended_enabled_;
  bool instant_enabled_;

  // If true, the instant URL is set to kLocalOmniboxPopupURL.
  bool use_local_preview_only_;

  // The state of the preview page, i.e., the page owned by |overlay_|. Ignored
  // if |instant_tab_| is in use.
  InstantModel model_;

  // The three instances of InstantPage maintained by InstantController as
  // described above. All three may be non-NULL in extended mode.  If
  // |instant_tab_| is not NULL, then |overlay_| is guaranteed to be hidden and
  // messages will be sent to |instant_tab_| instead.
  //
  // In non-extended mode, only |overlay_| is ever non-NULL.
  scoped_ptr<InstantOverlay> overlay_;
  scoped_ptr<InstantNTP> ntp_;
  scoped_ptr<InstantTab> instant_tab_;

  // The most recent full_text passed to Update(). If empty, we'll not accept
  // search suggestions from |overlay_| or |instant_tab_|.
  string16 last_omnibox_text_;

  // True if the last Update() had an inline autocompletion. Used only to make
  // sure that we don't accidentally suggest gray text suggestion in that case.
  bool last_omnibox_text_has_inline_autocompletion_;

  // The most recent verbatim passed to Update(). Used only to ensure that we
  // don't accidentally suggest an inline autocompletion.
  bool last_verbatim_;

  // The most recent suggestion received from the page, minus any prefix that
  // the user has typed.
  InstantSuggestion last_suggestion_;

  // See comments on the getter above.
  content::PageTransition last_transition_type_;

  // True if the last match passed to Update() was a search (versus a URL).
  // Used to ensure that the preview page is committable.
  bool last_match_was_search_;

  // Omnibox focus state.
  OmniboxFocusState omnibox_focus_state_;

  // The search model mode for the active tab.
  chrome::search::Mode search_mode_;

  // Current omnibox popup bounds.
  gfx::Rect popup_bounds_;

  // Last popup bounds passed to the page.
  gfx::Rect last_popup_bounds_;

  // Size of the start-edge omnibox text area margin.
  int start_margin_;

  // Size of the end-edge omnibox text area margin.
  int end_margin_;

  // Timer used to update the bounds of the omnibox popup.
  base::OneShotTimer<InstantController> update_bounds_timer_;

  // For each key K => value N, the map says that we found that the search
  // engine identified by Instant URL K didn't support the Instant API, or
  // caused RenderView crashes in each of the last N times that we loaded it.
  // If an Instant URL isn't present in the map at all or has a value 0,
  // it means that search engine supports the Instant API (or we assume it does,
  // since we haven't determined it doesn't) and it did not cause a crash.
  std::map<std::string, int> blacklisted_urls_;

  // Search terms extraction (for autocomplete history matches) doesn't work
  // on Instant URLs. So, whenever the user commits an Instant search, we add
  // an equivalent non-Instant search URL to history, so that the search shows
  // up in autocomplete history matches.
  // TODO(sreeram): Remove when http://crbug.com/155373 is fixed.
  GURL url_for_history_;

  // The timestamp at which query editing began. This value is used when the
  // preview is showed and cleared when the preview is hidden.
  base::Time first_interaction_time_;

  // Whether to allow the preview to show search suggestions. In general, the
  // preview is allowed to show search suggestions whenever |search_mode_| is
  // MODE_SEARCH_SUGGESTIONS, except in those cases where this is false.
  bool allow_preview_to_show_search_suggestions_;

  // List of events and their timestamps, useful in debugging Instant behaviour.
  mutable std::list<std::pair<int64, std::string> > debug_events_;

  DISALLOW_COPY_AND_ASSIGN(InstantController);
};

#endif  // CHROME_BROWSER_INSTANT_INSTANT_CONTROLLER_H_
