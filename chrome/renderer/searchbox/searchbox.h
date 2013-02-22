// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_SEARCHBOX_SEARCHBOX_H_
#define CHROME_RENDERER_SEARCHBOX_SEARCHBOX_H_

#include <vector>

#include "base/basictypes.h"
#include "base/string16.h"
#include "chrome/common/instant_types.h"
#include "chrome/common/search_types.h"
#include "content/public/common/page_transition_types.h"
#include "content/public/renderer/render_view_observer.h"
#include "content/public/renderer/render_view_observer_tracker.h"
#include "ui/gfx/rect.h"

namespace content {
class RenderView;
}

class SearchBox : public content::RenderViewObserver,
                  public content::RenderViewObserverTracker<SearchBox> {
 public:
  explicit SearchBox(content::RenderView* render_view);
  virtual ~SearchBox();

  // Sends ChromeViewHostMsg_SetSuggestions to the browser.
  void SetSuggestions(const std::vector<InstantSuggestion>& suggestions);

  // Sends ChromeViewHostMsg_ShowInstantOverlay to the browser.
  void ShowInstantOverlay(InstantShownReason reason,
                          int height,
                          InstantSizeUnits units);

  // Sends ChromeViewHostMsg_StartCapturingKeyStrokes to the browser.
  void StartCapturingKeyStrokes();

  // Sends ChromeViewHostMsg_StopCapturingKeyStrokes to the browser.
  void StopCapturingKeyStrokes();

  // Sends ChromeViewHostMsg_SearchBoxNavigate to the browser.
  void NavigateToURL(const GURL& url, content::PageTransition transition);

  // Sends ChromeViewHostMsg_InstantDeleteMostVisitedItem to the browser.
  void DeleteMostVisitedItem(int restrict_id);

  // Sends ChromeViewHostMsg_InstantUndoMostVisitedDeletion to the browser.
  void UndoMostVisitedDeletion(int restrict_id);

  // Sends ChromeViewHostMsg_InstantUndoAllMostVisitedDeletions to the browser.
  void UndoAllMostVisitedDeletions();

  const string16& query() const { return query_; }
  bool verbatim() const { return verbatim_; }
  size_t selection_start() const { return selection_start_; }
  size_t selection_end() const { return selection_end_; }
  int results_base() const { return results_base_; }
  bool is_key_capture_enabled() const { return is_key_capture_enabled_; }
  bool display_instant_results() const { return display_instant_results_; }
  const string16& omnibox_font() const { return omnibox_font_; }
  size_t omnibox_font_size() const { return omnibox_font_size_; }

  // These functions return the start/end margins of the page text area,
  // adjusted for the page zoom.
  int GetStartMargin() const;
  int GetEndMargin() const;

  // Returns the bounds of the omnibox popup in screen coordinates.
  gfx::Rect GetPopupBounds() const;

  const std::vector<InstantAutocompleteResult>& GetAutocompleteResults();
  // Searchbox retains ownership of this object.
  const InstantAutocompleteResult*
      GetAutocompleteResultWithId(size_t restricted_id) const;
  const ThemeBackgroundInfo& GetThemeBackgroundInfo();

  // Most Visited items.
  const std::vector<MostVisitedItem>& GetMostVisitedItems();

  // Secure Urls.
  int UrlToRestrictedId(const string16 url);
  string16 RestrictedIdToURL(int id);
  string16 GenerateThumbnailUrl(int id);
  string16 GenerateFaviconUrl(int id);

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
  void OnMarginChange(int start, int end);
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
  void OnMostVisitedChanged(const std::vector<MostVisitedItem>& items);

  // Returns the current zoom factor of the render view or 1 on failure.
  double GetZoom() const;

  // Sets the searchbox values to their initial value.
  void Reset();

  string16 query_;
  bool verbatim_;
  size_t selection_start_;
  size_t selection_end_;
  size_t results_base_;
  int start_margin_;
  int end_margin_;
  gfx::Rect popup_bounds_;
  std::vector<InstantAutocompleteResult> autocomplete_results_;
  size_t last_results_base_;
  std::vector<InstantAutocompleteResult> last_autocomplete_results_;
  bool is_key_capture_enabled_;
  ThemeBackgroundInfo theme_info_;
  bool display_instant_results_;
  string16 omnibox_font_;
  size_t omnibox_font_size_;
  std::vector<MostVisitedItem> most_visited_items_;

  // URL to Restricted Id mapping.
  // TODO(dcblack): Unify this logic to work with both Most Visited and
  // history suggestions.  (crbug/175768)
  std::map<string16, int> url_to_restricted_id_map_;
  std::map<int, string16> restricted_id_to_url_map_;
  int last_restricted_id_;

  DISALLOW_COPY_AND_ASSIGN(SearchBox);
};

#endif  // CHROME_RENDERER_SEARCHBOX_SEARCHBOX_H_
