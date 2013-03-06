// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/searchbox/searchbox.h"

#include "base/utf_string_conversions.h"
#include "chrome/common/render_messages.h"
#include "chrome/renderer/searchbox/searchbox_extension.h"
#include "content/public/renderer/render_view.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"

namespace {

// Prefix for a thumbnail URL.
const char kThumbnailUrlPrefix[] = "chrome-search://thumb/";

// Prefix for a thumbnail URL.
const char kFaviconUrlPrefix[] = "chrome-search://favicon/";

}

SearchBox::SearchBox(content::RenderView* render_view)
    : content::RenderViewObserver(render_view),
      content::RenderViewObserverTracker<SearchBox>(render_view),
      verbatim_(false),
      selection_start_(0),
      selection_end_(0),
      results_base_(0),
      start_margin_(0),
      last_results_base_(0),
      is_key_capture_enabled_(false),
      display_instant_results_(false),
      omnibox_font_size_(0),
      last_restricted_id_(0) {
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

void SearchBox::ShowInstantOverlay(InstantShownReason reason,
                                   int height,
                                   InstantSizeUnits units) {
  render_view()->Send(new ChromeViewHostMsg_ShowInstantOverlay(
      render_view()->GetRoutingID(), render_view()->GetPageId(), reason,
      height, units));
}

void SearchBox::StartCapturingKeyStrokes() {
  render_view()->Send(new ChromeViewHostMsg_StartCapturingKeyStrokes(
      render_view()->GetRoutingID(), render_view()->GetPageId()));
}

void SearchBox::StopCapturingKeyStrokes() {
  render_view()->Send(new ChromeViewHostMsg_StopCapturingKeyStrokes(
      render_view()->GetRoutingID(), render_view()->GetPageId()));
}

void SearchBox::NavigateToURL(const GURL& url,
                              content::PageTransition transition,
                              WindowOpenDisposition disposition) {
  render_view()->Send(new ChromeViewHostMsg_SearchBoxNavigate(
      render_view()->GetRoutingID(), render_view()->GetPageId(),
      url, transition, disposition));
}

void SearchBox::DeleteMostVisitedItem(int restrict_id) {
  string16 url = RestrictedIdToURL(restrict_id);
  render_view()->Send(new ChromeViewHostMsg_InstantDeleteMostVisitedItem(
      render_view()->GetRoutingID(), GURL(url)));
}

void SearchBox::UndoMostVisitedDeletion(int restrict_id) {
  string16 url = RestrictedIdToURL(restrict_id);
  render_view()->Send(new ChromeViewHostMsg_InstantUndoMostVisitedDeletion(
      render_view()->GetRoutingID(), GURL(url)));
}

void SearchBox::UndoAllMostVisitedDeletions() {
  render_view()->Send(new ChromeViewHostMsg_InstantUndoAllMostVisitedDeletions(
      render_view()->GetRoutingID()));
}

int SearchBox::GetStartMargin() const {
  return static_cast<int>(start_margin_ / GetZoom());
}

gfx::Rect SearchBox::GetPopupBounds() const {
  double zoom = GetZoom();
  return gfx::Rect(static_cast<int>(popup_bounds_.x() / zoom),
                   static_cast<int>(popup_bounds_.y() / zoom),
                   static_cast<int>(popup_bounds_.width() / zoom),
                   static_cast<int>(popup_bounds_.height() / zoom));
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

bool SearchBox::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(SearchBox, message)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_SearchBoxChange, OnChange)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_SearchBoxSubmit, OnSubmit)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_SearchBoxCancel, OnCancel)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_SearchBoxPopupResize, OnPopupResize)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_SearchBoxMarginChange, OnMarginChange)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_DetermineIfPageSupportsInstant,
                        OnDetermineIfPageSupportsInstant)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_SearchBoxAutocompleteResults,
                        OnAutocompleteResults)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_SearchBoxUpOrDownKeyPressed,
                        OnUpOrDownKeyPressed)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_SearchBoxCancelSelection,
                        OnCancelSelection)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_SearchBoxSetDisplayInstantResults,
                        OnSetDisplayInstantResults)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_SearchBoxKeyCaptureChanged,
                        OnKeyCaptureChange)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_SearchBoxThemeChanged,
                        OnThemeChanged)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_SearchBoxFontInformation,
                        OnFontInformationReceived)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_InstantMostVisitedItemsChanged,
                        OnMostVisitedChanged)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void SearchBox::DidClearWindowObject(WebKit::WebFrame* frame) {
  extensions_v8::SearchBoxExtension::DispatchOnWindowReady(frame);
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
    DVLOG(1) << render_view() << " OnChange";
    extensions_v8::SearchBoxExtension::DispatchChange(
        render_view()->GetWebView()->mainFrame());
  }
}

void SearchBox::OnSubmit(const string16& query) {
  query_ = query;
  verbatim_ = true;
  selection_start_ = selection_end_ = query_.size();
  if (render_view()->GetWebView() && render_view()->GetWebView()->mainFrame()) {
    DVLOG(1) << render_view() << " OnSubmit";
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
    DVLOG(1) << render_view() << " OnCancel";
    extensions_v8::SearchBoxExtension::DispatchCancel(
        render_view()->GetWebView()->mainFrame());
  }
  Reset();
}

void SearchBox::OnPopupResize(const gfx::Rect& bounds) {
  popup_bounds_ = bounds;
  if (render_view()->GetWebView() && render_view()->GetWebView()->mainFrame()) {
    DVLOG(1) << render_view() << " OnPopupResize";
    extensions_v8::SearchBoxExtension::DispatchResize(
        render_view()->GetWebView()->mainFrame());
  }
}

void SearchBox::OnMarginChange(int margin, int width) {
  start_margin_ = margin;

  // Override only the width parameter of the popup bounds.
  popup_bounds_.set_width(width);

  if (render_view()->GetWebView() && render_view()->GetWebView()->mainFrame()) {
    extensions_v8::SearchBoxExtension::DispatchMarginChange(
        render_view()->GetWebView()->mainFrame());
  }
}

void SearchBox::OnDetermineIfPageSupportsInstant() {
  if (render_view()->GetWebView() && render_view()->GetWebView()->mainFrame()) {
    bool result = extensions_v8::SearchBoxExtension::PageSupportsInstant(
        render_view()->GetWebView()->mainFrame());
    DVLOG(1) << render_view() << " PageSupportsInstant: " << result;
    render_view()->Send(new ChromeViewHostMsg_InstantSupportDetermined(
        render_view()->GetRoutingID(), render_view()->GetPageId(), result));
  }
}

void SearchBox::OnAutocompleteResults(
    const std::vector<InstantAutocompleteResult>& results) {
  results_base_ += autocomplete_results_.size();
  autocomplete_results_ = results;
  if (render_view()->GetWebView() && render_view()->GetWebView()->mainFrame()) {
    DVLOG(1) << render_view() << " OnAutocompleteResults";
    extensions_v8::SearchBoxExtension::DispatchAutocompleteResults(
        render_view()->GetWebView()->mainFrame());
  }
}

void SearchBox::OnUpOrDownKeyPressed(int count) {
  if (render_view()->GetWebView() && render_view()->GetWebView()->mainFrame()) {
    DVLOG(1) << render_view() << " OnKeyPress: " << count;
    extensions_v8::SearchBoxExtension::DispatchUpOrDownKeyPress(
        render_view()->GetWebView()->mainFrame(), count);
  }
}

void SearchBox::OnCancelSelection(const string16& query) {
  // TODO(sreeram): crbug.com/176101 The state reset below are somewhat wrong.
  query_ = query;
  verbatim_ = true;
  selection_start_ = selection_end_ = query_.size();
  if (render_view()->GetWebView() && render_view()->GetWebView()->mainFrame()) {
    DVLOG(1) << render_view() << " OnKeyPress ESC";
    extensions_v8::SearchBoxExtension::DispatchEscKeyPress(
        render_view()->GetWebView()->mainFrame());
  }
}

void SearchBox::OnKeyCaptureChange(bool is_key_capture_enabled) {
  if (is_key_capture_enabled != is_key_capture_enabled_ &&
      render_view()->GetWebView() && render_view()->GetWebView()->mainFrame()) {
    is_key_capture_enabled_ = is_key_capture_enabled;
    DVLOG(1) << render_view() << " OnKeyCaptureChange";
    extensions_v8::SearchBoxExtension::DispatchKeyCaptureChange(
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

double SearchBox::GetZoom() const {
  WebKit::WebView* web_view = render_view()->GetWebView();
  if (web_view) {
    double zoom = WebKit::WebView::zoomLevelToZoomFactor(web_view->zoomLevel());
    if (zoom != 0)
      return zoom;
  }
  return 1.0;
}

void SearchBox::OnFontInformationReceived(const string16& omnibox_font,
                                          size_t omnibox_font_size) {
  omnibox_font_ = omnibox_font;
  omnibox_font_size_ = omnibox_font_size;
}

void SearchBox::Reset() {
  query_.clear();
  verbatim_ = false;
  selection_start_ = 0;
  selection_end_ = 0;
  results_base_ = 0;
  popup_bounds_ = gfx::Rect();
  start_margin_ = 0;
  autocomplete_results_.clear();
  is_key_capture_enabled_ = false;
  theme_info_ = ThemeBackgroundInfo();
  // Don't reset display_instant_results_ to prevent clearing it on committed
  // results pages in extended mode. Otherwise resetting it is a no-op because
  // a new loader is created when it changes; see crbug.com/164662.
  // Also don't reset omnibox_font_ or omnibox_font_size_ since it never
  // changes.
}

void SearchBox::OnMostVisitedChanged(
    const std::vector<MostVisitedItem>& items) {
  most_visited_items_ = items;

  if (render_view()->GetWebView() && render_view()->GetWebView()->mainFrame()) {
    extensions_v8::SearchBoxExtension::DispatchMostVisitedChanged(
        render_view()->GetWebView()->mainFrame());
  }
}

const std::vector<MostVisitedItem>& SearchBox::GetMostVisitedItems() {
  return most_visited_items_;
}

int SearchBox::UrlToRestrictedId(string16 url) {
  if (url_to_restricted_id_map_[url])
    return url_to_restricted_id_map_[url];

  last_restricted_id_++;
  url_to_restricted_id_map_[url] = last_restricted_id_;
  restricted_id_to_url_map_[last_restricted_id_] = url;

  return last_restricted_id_;
}

string16 SearchBox::RestrictedIdToURL(int id) {
  return restricted_id_to_url_map_[id];
}

string16 SearchBox::GenerateThumbnailUrl(int id) {
  std::ostringstream ostr;
  ostr << kThumbnailUrlPrefix << id;
  GURL url = GURL(ostr.str());
  return UTF8ToUTF16(url.spec());
}

string16 SearchBox::GenerateFaviconUrl(int id) {
  std::ostringstream ostr;
  ostr << kFaviconUrlPrefix << id;
  GURL url = GURL(ostr.str());
  return UTF8ToUTF16(url.spec());
}
