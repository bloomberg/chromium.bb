// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_SEARCHBOX_SEARCHBOX_H_
#define CHROME_RENDERER_SEARCHBOX_SEARCHBOX_H_

#include <vector>

#include "base/basictypes.h"
#include "base/string16.h"
#include "chrome/common/instant_types.h"
#include "content/public/renderer/render_view_observer.h"
#include "content/public/renderer/render_view_observer_tracker.h"
#include "ui/gfx/rect.h"

namespace content {
class RenderView;
}

namespace IPC {
class Message;
}

class SearchBox : public content::RenderViewObserver,
                  public content::RenderViewObserverTracker<SearchBox> {
 public:
  explicit SearchBox(content::RenderView* render_view);
  virtual ~SearchBox();

  // Sends ViewHostMsg_SetSuggestions to the browser.
  void SetSuggestions(const std::vector<InstantSuggestion>& suggestions);

  // Sends ViewHostMsg_SetInstantPreviewHeight to the browser.
  void SetInstantPreviewHeight(int height, InstantSizeUnits units);

  const string16& query() const { return query_; }
  bool verbatim() const { return verbatim_; }
  size_t selection_start() const { return selection_start_; }
  size_t selection_end() const { return selection_end_; }
  int results_base() const { return results_base_; }
  gfx::Rect GetRect();
  const std::vector<InstantAutocompleteResult>& autocomplete_results() const {
    return autocomplete_results_;
  }

 private:
  // RenderViewObserver implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

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

  // Sets the searchbox values to their initial value.
  void Reset();

  string16 query_;
  bool verbatim_;
  size_t selection_start_;
  size_t selection_end_;
  int results_base_;
  gfx::Rect rect_;
  std::vector<InstantAutocompleteResult> autocomplete_results_;

  DISALLOW_COPY_AND_ASSIGN(SearchBox);
};

#endif  // CHROME_RENDERER_SEARCHBOX_SEARCHBOX_H_
