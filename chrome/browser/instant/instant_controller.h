// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INSTANT_INSTANT_CONTROLLER_H_
#define CHROME_BROWSER_INSTANT_INSTANT_CONTROLLER_H_

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "base/time.h"
#include "base/timer.h"
#include "chrome/browser/instant/instant_commit_type.h"
#include "chrome/browser/instant/instant_model.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/common/instant_types.h"
#include "chrome/common/search_types.h"
#include "content/public/common/page_transition_types.h"
#include "googleurl/src/gurl.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/rect.h"

struct AutocompleteMatch;
class AutocompleteProvider;
class InstantLoader;
class InstantTab;
class TemplateURL;

namespace chrome {
class BrowserInstantController;
}

namespace content {
class WebContents;
}

// InstantController maintains a WebContents that is intended to give a preview
// of search suggestions and results. InstantController is owned by Browser via
// BrowserInstantController.
class InstantController {
 public:
  // The URL for the local omnibox popup.
  static const char* kLocalOmniboxPopupURL;

  // |use_local_preview_only| will force the use of kLocalOmniboxPopupURL as the
  // instant URL and is only applicable if |extended_enabled| is true.
  InstantController(chrome::BrowserInstantController* browser,
                    bool extended_enabled,
                    bool use_local_preview_only);
  ~InstantController();

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

  // The preview WebContents. May be NULL. InstantController retains ownership.
  content::WebContents* GetPreviewContents() const;

  // Returns true if the Instant preview is showing a search results preview.
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
  // the loader which will notify the renderer. Create |instant_tab_| if the
  // |new_mode| reflects an Instant search results page.
  void SearchModeChanged(const chrome::search::Mode& old_mode,
                         const chrome::search::Mode& new_mode);

  // The user switched tabs. Hide the preview. Create |instant_tab_| if the
  // newly active tab is an Instant search results page.
  void ActiveTabChanged();

  // The user is about to switch tabs. Commit the preview if needed.
  void TabDeactivated(content::WebContents* contents);

  // Sets whether Instant should show result previews.
  void SetInstantEnabled(bool instant_enabled);

  // The theme has changed. Pass the message to the preview page.
  void ThemeChanged(const ThemeBackgroundInfo& theme_info);

  // The theme area height has changed. Pass the message to the preview page.
  void ThemeAreaHeightChanged(int height);

  // Returns the transition type of the last AutocompleteMatch passed to Update.
  content::PageTransition last_transition_type() const {
    return last_transition_type_;
  }

  const InstantModel* model() const { return &model_; }

  // Invoked by the page when it has suggested text.
  void SetSuggestions(const content::WebContents* contents,
                      const std::vector<InstantSuggestion>& suggestions);

  // Invoked by the page when its support for the Instant API is determined.
  void InstantSupportDetermined(const content::WebContents* contents,
                                bool supports_instant);

  // Invoked by InstantLoader to request that the preview be shown.
  void ShowInstantPreview(InstantShownReason reason,
                          int height,
                          InstantSizeUnits units);

  // Invoked by InstantLoader to request the browser to start capturing user key
  // strokes.
  void StartCapturingKeyStrokes();

  // Invoked by InstantLoader to request the browser to stop capturing user key
  // strokes.
  void StopCapturingKeyStrokes();

  // Invoked by InstantLoader when it has swapped a different WebContents into
  // the preview, usually because a prerendered page was navigated to.
  void SwappedWebContents();

  // Invoked by InstantLoader when the preview gains focus, usually due to the
  // user clicking on it.
  void InstantLoaderContentsFocused();

  // Invoked by the InstantLoader when its RenderView crashes.
  void InstantLoaderRenderViewGone();

  // Invoked by InstantLoader when the instant page is about to navigate.
  void InstantLoaderAboutToNavigateMainFrame(const GURL& url);

  // Invoked by the InstantLoader when the instant page wants to navigate to
  // the speicfied URL.
  void NavigateToURL(const GURL& url, content::PageTransition transition);

 private:
  FRIEND_TEST_ALL_PREFIXES(InstantTest, OmniboxFocusLoadsInstant);
  FRIEND_TEST_ALL_PREFIXES(InstantTest, SetWithTemplateURL);
  FRIEND_TEST_ALL_PREFIXES(InstantTest, NonInstantSearchProvider);
  FRIEND_TEST_ALL_PREFIXES(InstantTest, InstantLoaderRefresh);
  FRIEND_TEST_ALL_PREFIXES(InstantExtendedTest, ExtendedModeIsOn);
  FRIEND_TEST_ALL_PREFIXES(InstantExtendedTest, OmniboxFocusLoadsInstant);

  // Helper for OmniboxFocusChanged. Commit or discard the preview.
  void OmniboxLostFocus(gfx::NativeView view_gaining_focus);

  // Creates a new loader if necessary, using the instant_url property of the
  // |template_url| (for example, if the Instant URL has changed since the last
  // time the loader was created). If |fallback_to_local| is true will use
  // kLocalOmniboxPopupURL as the fallback url (in extended mode) in case
  // the |template_url| doesn't have a valid Instant URL. Returns true if an
  // instant URL could be determined.
  bool ResetLoader(const TemplateURL* template_url,
                   const content::WebContents* active_tab,
                   bool fallback_to_local);

  // Ensures that the |loader_| uses the default Instant URL, recreating it if
  // necessary, and returns true. Returns false if the Instant URL could not be
  // determined or the active tab is NULL (browser is shutting down).
  bool CreateDefaultLoader();

  // Called when the |loader_| might be stale. If it's actually stale, and the
  // omnibox doesn't have focus, and the preview isn't showing, the |loader_| is
  // deleted and recreated. Else the refresh is skipped.
  void OnStaleLoader();

  // If the active tab is an Instant search results page, sets |instant_tab_| to
  // point to it. Else, deletes any existing |instant_tab_|.
  void ResetInstantTab();

  // Called by Update() to ensure we have an Instant page that can process
  // |match|. Returns true if we should continue with the Update().
  bool ResetLoaderForMatch(const AutocompleteMatch& match);

  // Hide the preview. Also sends an onchange event (with blank query) to the
  // preview, telling it to clear out results for any old queries.
  void HideLoader();

  // Like HideLoader(), but doesn't call OnStaleLoader(). Use HideLoader()
  // unless you are going to call loader_.reset() yourself subsequently.
  void HideInternal();

  // Counterpart to HideLoader(). Asks the |browser_| to display the preview
  // with the given |height|.
  void ShowLoader(InstantShownReason reason,
                  int height,
                  InstantSizeUnits units);

  // Send the omnibox popup bounds to the page.
  void SendPopupBoundsToPage();

  // If |template_url| is a valid TemplateURL for use with Instant, fills in
  // |instant_url| and returns true; returns false otherwise.
  // Note: If the command-line switch kInstantURL is set, this method uses its
  // value for |instant_url| and returns true without examining |template_url|.
  bool GetInstantURL(const TemplateURL* template_url,
                     std::string* instant_url) const;

  chrome::BrowserInstantController* const browser_;

  // Whether the extended API and regular API are enabled. If both are false,
  // Instant is effectively disabled.
  const bool extended_enabled_;
  bool instant_enabled_;

  // If true, the instant URL is set to kLocalOmniboxPopupURL.
  const bool use_local_preview_only_;

  // The state of the preview page, i.e., the page owned by |loader_|. Ignored
  // if |instant_tab_| is in use.
  InstantModel model_;

  // The preview WebContents.
  scoped_ptr<InstantLoader> loader_;

  // A committed WebContents that supports Instant. If non-NULL, the |loader_|
  // is guaranteed to be hidden and messages will be sent to this instead.
  scoped_ptr<InstantTab> instant_tab_;

  // The most recent full_text passed to Update(). If empty, we'll not accept
  // search suggestions from |loader_| or |instant_tab_|.
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

  // Timer used to ensure that the Instant page does not get too stale.
  base::OneShotTimer<InstantController> stale_loader_timer_;

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

  DISALLOW_COPY_AND_ASSIGN(InstantController);
};

#endif  // CHROME_BROWSER_INSTANT_INSTANT_CONTROLLER_H_
