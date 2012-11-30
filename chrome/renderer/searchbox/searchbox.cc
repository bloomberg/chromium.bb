// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/searchbox/searchbox.h"

#include "chrome/common/render_messages.h"
#include "chrome/renderer/searchbox/searchbox_extension.h"
#include "content/public/renderer/render_view.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"

SearchBox::SearchBox(content::RenderView* render_view)
    : content::RenderViewObserver(render_view),
      content::RenderViewObserverTracker<SearchBox>(render_view),
      verbatim_(false),
      selection_start_(0),
      selection_end_(0),
      results_base_(0),
      last_results_base_(0),
      theme_area_height_(0),
      display_instant_results_(false) {
}

SearchBox::~SearchBox() {
}

void SearchBox::SetSuggestions(
    const std::vector<InstantSuggestion>& suggestions) {
  if (!suggestions.empty() &&
      suggestions[0].behavior == INSTANT_COMPLETE_REPLACE) {
    query_ = suggestions[0].text;
    verbatim_ = true;
    selection_start_ = selection_end_ = query_.size();
  }
  // Explicitly allow empty vector to be sent to the browser.
  render_view()->Send(new ChromeViewHostMsg_SetSuggestions(
      render_view()->GetRoutingID(), render_view()->GetPageId(), suggestions));
}

void SearchBox::ShowInstantPreview(InstantShownReason reason,
                                   int height,
                                   InstantSizeUnits units) {
  render_view()->Send(new ChromeViewHostMsg_ShowInstantPreview(
      render_view()->GetRoutingID(), render_view()->GetPageId(), reason,
      height, units));
}

gfx::Rect SearchBox::GetRect() {
  // Need to adjust for scale.
  if (rect_.IsEmpty())
    return rect_;
  WebKit::WebView* web_view = render_view()->GetWebView();
  if (!web_view)
    return rect_;
  double zoom = WebKit::WebView::zoomLevelToZoomFactor(web_view->zoomLevel());
  if (zoom == 0)
    return rect_;
  return gfx::Rect(static_cast<int>(static_cast<float>(rect_.x()) / zoom),
                   static_cast<int>(static_cast<float>(rect_.y()) / zoom),
                   static_cast<int>(static_cast<float>(rect_.width()) / zoom),
                   static_cast<int>(static_cast<float>(rect_.height()) / zoom));
}

const std::vector<InstantAutocompleteResult>&
    SearchBox::GetAutocompleteResults() {
  // Remember the last requested autocomplete_results to account for race
  // conditions between autocomplete providers returning new data and the user
  // clicking on a suggestion.
  last_autocomplete_results_ = autocomplete_results_;
  last_results_base_ = results_base_;
  return autocomplete_results_;
}

const InstantAutocompleteResult* SearchBox::GetAutocompleteResultWithId(
    size_t restricted_id) const {
  if (restricted_id < last_results_base_ ||
      restricted_id >= last_results_base_ + last_autocomplete_results_.size())
    return NULL;
  return &last_autocomplete_results_[restricted_id - last_results_base_];
}

const ThemeBackgroundInfo& SearchBox::GetThemeBackgroundInfo() {
  return theme_info_;
}

int SearchBox::GetThemeAreaHeight() {
  return theme_area_height_;
}

bool SearchBox::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(SearchBox, message)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_SearchBoxChange, OnChange)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_SearchBoxSubmit, OnSubmit)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_SearchBoxCancel, OnCancel)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_SearchBoxResize, OnResize)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_DetermineIfPageSupportsInstant,
                        OnDetermineIfPageSupportsInstant)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_SearchBoxAutocompleteResults,
                        OnAutocompleteResults)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_SearchBoxUpOrDownKeyPressed,
                        OnUpOrDownKeyPressed)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_SearchBoxModeChanged,
                        OnModeChanged)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_SearchBoxSetDisplayInstantResults,
                        OnSetDisplayInstantResults)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_SearchBoxThemeChanged,
                        OnThemeChanged)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_SearchBoxThemeAreaHeightChanged,
                        OnThemeAreaHeightChanged)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void SearchBox::OnChange(const string16& query,
                         bool verbatim,
                         size_t selection_start,
                         size_t selection_end) {
  query_ = query;
  verbatim_ = verbatim;
  selection_start_ = selection_start;
  selection_end_ = selection_end;
  if (render_view()->GetWebView() && render_view()->GetWebView()->mainFrame()) {
    extensions_v8::SearchBoxExtension::DispatchChange(
        render_view()->GetWebView()->mainFrame());
  }
}

void SearchBox::OnSubmit(const string16& query) {
  query_ = query;
  verbatim_ = true;
  selection_start_ = selection_end_ = query_.size();
  if (render_view()->GetWebView() && render_view()->GetWebView()->mainFrame()) {
    extensions_v8::SearchBoxExtension::DispatchSubmit(
        render_view()->GetWebView()->mainFrame());
  }
  Reset();
}

void SearchBox::OnCancel(const string16& query) {
  query_ = query;
  verbatim_ = true;
  selection_start_ = selection_end_ = query_.size();
  if (render_view()->GetWebView() && render_view()->GetWebView()->mainFrame()) {
    extensions_v8::SearchBoxExtension::DispatchCancel(
        render_view()->GetWebView()->mainFrame());
  }
  Reset();
}

void SearchBox::OnResize(const gfx::Rect& bounds) {
  rect_ = bounds;
  if (render_view()->GetWebView() && render_view()->GetWebView()->mainFrame()) {
    extensions_v8::SearchBoxExtension::DispatchResize(
        render_view()->GetWebView()->mainFrame());
  }
}

void SearchBox::OnDetermineIfPageSupportsInstant() {
  if (render_view()->GetWebView() && render_view()->GetWebView()->mainFrame()) {
    bool result = extensions_v8::SearchBoxExtension::PageSupportsInstant(
        render_view()->GetWebView()->mainFrame());
    render_view()->Send(new ChromeViewHostMsg_InstantSupportDetermined(
        render_view()->GetRoutingID(), render_view()->GetPageId(), result));
  }
}

void SearchBox::OnAutocompleteResults(
    const std::vector<InstantAutocompleteResult>& results) {
  results_base_ += autocomplete_results_.size();
  autocomplete_results_ = results;
  if (render_view()->GetWebView() && render_view()->GetWebView()->mainFrame()) {
    extensions_v8::SearchBoxExtension::DispatchAutocompleteResults(
        render_view()->GetWebView()->mainFrame());
  }
}

void SearchBox::OnUpOrDownKeyPressed(int count) {
  if (render_view()->GetWebView() && render_view()->GetWebView()->mainFrame()) {
    extensions_v8::SearchBoxExtension::DispatchUpOrDownKeyPress(
        render_view()->GetWebView()->mainFrame(), count);
  }
}

void SearchBox::OnModeChanged(const chrome::search::Mode& mode) {
  mode_ = mode;
  if (render_view()->GetWebView() && render_view()->GetWebView()->mainFrame()) {
    extensions_v8::SearchBoxExtension::DispatchContextChange(
        render_view()->GetWebView()->mainFrame());
  }
}

void SearchBox::OnSetDisplayInstantResults(bool display_instant_results) {
  display_instant_results_ = display_instant_results;
}

void SearchBox::OnThemeChanged(const ThemeBackgroundInfo& theme_info) {
  theme_info_ = theme_info;
  if (render_view()->GetWebView() && render_view()->GetWebView()->mainFrame()) {
    extensions_v8::SearchBoxExtension::DispatchThemeChange(
        render_view()->GetWebView()->mainFrame());
  }
}

void SearchBox::OnThemeAreaHeightChanged(int height) {
  theme_area_height_ = height;
  if (render_view()->GetWebView() && render_view()->GetWebView()->mainFrame()) {
    extensions_v8::SearchBoxExtension::DispatchThemeAreaHeightChange(
        render_view()->GetWebView()->mainFrame());
  }
}

void SearchBox::Reset() {
  query_.clear();
  verbatim_ = false;
  selection_start_ = 0;
  selection_end_ = 0;
  results_base_ = 0;
  rect_ = gfx::Rect();
  autocomplete_results_.clear();
  mode_ = chrome::search::Mode();
  theme_info_ = ThemeBackgroundInfo();
  theme_area_height_ = 0;
  display_instant_results_ = false;
}
