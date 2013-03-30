// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_SEARCHBOX_SEARCHBOX_H_
#define CHROME_RENDERER_SEARCHBOX_SEARCHBOX_H_

#include <vector>

#include "base/basictypes.h"
#include "base/string16.h"
#include "chrome/common/instant_restricted_id_cache.h"
#include "chrome/common/instant_types.h"
#include "chrome/common/search_types.h"
#include "content/public/common/page_transition_types.h"
#include "content/public/renderer/render_view_observer.h"
#include "content/public/renderer/render_view_observer_tracker.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/rect.h"

namespace content {
class RenderView;
}

class SearchBox : public content::RenderViewObserver,
                  public content::RenderViewObserverTracker<SearchBox> {
 public:
  explicit SearchBox(content::RenderView* render_view);
  virtual ~SearchBox();

  // Sends ChromeViewHostMsg_SetSuggestion to the browser.
  void SetSuggestions(const std::vector<InstantSuggestion>& suggestions);

  // Clears the current query text, used to ensure that restricted query strings
  // are not retained.
  void ClearQuery();

  // Sends ChromeViewHostMsg_ShowInstantOverlay to the browser.
  void ShowInstantOverlay(int height, InstantSizeUnits units);

  // Sends ChromeViewHostMsg_FocusOmnibox to the browser.
  void FocusOmnibox();

  // Sends ChromeViewHostMsg_StartCapturingKeyStrokes to the browser.
  void StartCapturingKeyStrokes();

  // Sends ChromeViewHostMsg_StopCapturingKeyStrokes to the browser.
  void StopCapturingKeyStrokes();

  // Sends ChromeViewHostMsg_SearchBoxNavigate to the browser.
  void NavigateToURL(const GURL& url,
                     content::PageTransition transition,
                     WindowOpenDisposition disposition);

  // Shows any attached bars.
  void ShowBars();

  // Hides any attached bars.  When the bars are hidden, the "onbarshidden"
  // event is fired to notify the page.
  void HideBars();

  const string16& query() const { return query_; }
  bool verbatim() const { return verbatim_; }
  size_t selection_start() const { return selection_start_; }
  size_t selection_end() const { return selection_end_; }
  bool is_key_capture_enabled() const { return is_key_capture_enabled_; }
  bool display_instant_results() const { return display_instant_results_; }
  const string16& omnibox_font() const { return omnibox_font_; }
  size_t omnibox_font_size() const { return omnibox_font_size_; }

  // In extended Instant, returns the start-edge margin of the location bar in
  // screen pixels.
  int GetStartMargin() const;

  // Returns the bounds of the omnibox popup in screen coordinates.
  gfx::Rect GetPopupBounds() const;

  const ThemeBackgroundInfo& GetThemeBackgroundInfo();

  // --- Autocomplete result APIs.

  // Returns the most recent InstantAutocompleteResults sent by the browser.
  void GetAutocompleteResults(
      std::vector<InstantAutocompleteResultIDPair>* results) const;

  // If the |autocomplete_result_id| is found in the cache, sets |item| to it
  // and returns true.
  bool GetAutocompleteResultWithID(InstantRestrictedID autocomplete_result_id,
                                   InstantAutocompleteResult* result) const;

  // --- Most Visited items APIs.

  // Returns the latest most visited items sent by the browser.
  void GetMostVisitedItems(
      std::vector<InstantMostVisitedItemIDPair>* items) const;

  // If the |most_visited_item_id| is found in the cache, sets |item| to it
  // and returns true.
  bool GetMostVisitedItemWithID(InstantRestrictedID most_visited_item_id,
                                InstantMostVisitedItem* item) const;

  // Sends ChromeViewHostMsg_SearchBoxDeleteMostVisitedItem to the browser.
  void DeleteMostVisitedItem(InstantRestrictedID most_visited_item_id);

  // Sends ChromeViewHostMsg_SearchBoxUndoMostVisitedDeletion to the browser.
  void UndoMostVisitedDeletion(InstantRestrictedID most_visited_item_id);

  // Sends ChromeViewHostMsg_SearchBoxUndoAllMostVisitedDeletions to the
  // browser.
  void UndoAllMostVisitedDeletions();

  // Generates a data:// URL containing the HTML required to display a result.
  bool GenerateDataURLForSuggestionRequest(const GURL& request_url,
                                           GURL* data_url) const;

  // Sets the InstantAutocompleteResultStyle to the input value.
  void SetInstantAutocompleteResultStyle(
      const InstantAutocompleteResultStyle& style);

  // Formats a URL for display to the user. Strips out prefixes like whitespace,
  // "http://" and "www." unless the user input (|query_|) matches the prefix.
  // Also removes trailing whitespaces and "/" unless the user input matches the
  // trailing "/".
  void FormatURLForDisplay(string16* url) const;

  // Returns true if the Instant page should use iframes to display suggestions.
  static bool ShouldUseIframes();

 private:
  // Overridden from content::RenderViewObserver:
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void DidClearWindowObject(WebKit::WebFrame* frame) OVERRIDE;

  void OnChange(const string16& query,
                bool verbatim,
                size_t selection_start,
                size_t selection_end);
  void OnSubmit(const string16& query);
  void OnCancel(const string16& query);
  void OnPopupResize(const gfx::Rect& bounds);
  void OnMarginChange(int margin, int width);
  void OnBarsHidden();
  void OnDetermineIfPageSupportsInstant();
  void OnAutocompleteResults(
      const std::vector<InstantAutocompleteResult>& results);
  void OnUpOrDownKeyPressed(int count);
  void OnCancelSelection(const string16& query);
  void OnKeyCaptureChange(bool is_key_capture_enabled);
  void OnSetDisplayInstantResults(bool display_instant_results);
  void OnThemeChanged(const ThemeBackgroundInfo& theme_info);
  void OnThemeAreaHeightChanged(int height);
  void OnFontInformationReceived(const string16& omnibox_font,
                                 size_t omnibox_font_size);
  void OnMostVisitedChanged(
      const std::vector<InstantMostVisitedItemIDPair>& items);

  // Returns the current zoom factor of the render view or 1 on failure.
  double GetZoom() const;

  // Sets the searchbox values to their initial value.
  void Reset();

  string16 query_;
  bool verbatim_;
  size_t selection_start_;
  size_t selection_end_;
  int start_margin_;
  gfx::Rect popup_bounds_;
  bool is_key_capture_enabled_;
  ThemeBackgroundInfo theme_info_;
  bool display_instant_results_;
  string16 omnibox_font_;
  size_t omnibox_font_size_;
  InstantRestrictedIDCache<InstantAutocompleteResult>
      autocomplete_results_cache_;
  InstantRestrictedIDCache<InstantMostVisitedItem> most_visited_items_cache_;
  InstantAutocompleteResultStyle autocomplete_results_style_;

  DISALLOW_COPY_AND_ASSIGN(SearchBox);
};

#endif  // CHROME_RENDERER_SEARCHBOX_SEARCHBOX_H_
