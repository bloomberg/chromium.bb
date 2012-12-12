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

  // Sends ViewHostMsg_SetSuggestions to the browser.
  void SetSuggestions(const std::vector<InstantSuggestion>& suggestions);

  // Sends ViewHostMsg_ShowInstantPreview to the browser.
  void ShowInstantPreview(InstantShownReason reason,
                          int height,
                          InstantSizeUnits units);

  // Sends ViewHostMsg_StartCapturingKeyStrokes to the browser.
  void StartCapturingKeyStrokes();

  // Sends ViewHostMsg_StopCapturingKeyStrokes to the browser.
  void StopCapturingKeyStrokes();

  const string16& query() const { return query_; }
  bool verbatim() const { return verbatim_; }
  size_t selection_start() const { return selection_start_; }
  size_t selection_end() const { return selection_end_; }
  int results_base() const { return results_base_; }
  const chrome::search::Mode& mode() const { return mode_; }
  bool is_key_capture_enabled() const { return is_key_capture_enabled_; }
  bool display_instant_results() const { return display_instant_results_; }

  gfx::Rect GetRect();
  const std::vector<InstantAutocompleteResult>& GetAutocompleteResults();
  // Searchbox retains ownership of this object.
  const InstantAutocompleteResult*
      GetAutocompleteResultWithId(size_t restricted_id) const;
  const ThemeBackgroundInfo& GetThemeBackgroundInfo();
  int GetThemeAreaHeight();

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
  void OnResize(const gfx::Rect& bounds);
  void OnDetermineIfPageSupportsInstant();
  void OnAutocompleteResults(
      const std::vector<InstantAutocompleteResult>& results);
  void OnUpOrDownKeyPressed(int count);
  void OnKeyCaptureChange(bool is_key_capture_enabled);
  void OnModeChanged(const chrome::search::Mode& mode);
  void OnSetDisplayInstantResults(bool display_instant_results);
  void OnThemeChanged(const ThemeBackgroundInfo& theme_info);
  void OnThemeAreaHeightChanged(int height);

  // Sets the searchbox values to their initial value.
  void Reset();

  string16 query_;
  bool verbatim_;
  size_t selection_start_;
  size_t selection_end_;
  size_t results_base_;
  gfx::Rect rect_;
  std::vector<InstantAutocompleteResult> autocomplete_results_;
  size_t last_results_base_;
  std::vector<InstantAutocompleteResult> last_autocomplete_results_;
  bool is_key_capture_enabled_;
  chrome::search::Mode mode_;
  ThemeBackgroundInfo theme_info_;
  int theme_area_height_;
  bool display_instant_results_;

  DISALLOW_COPY_AND_ASSIGN(SearchBox);
};

#endif  // CHROME_RENDERER_SEARCHBOX_SEARCHBOX_H_
