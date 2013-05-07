// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/searchbox/searchbox.h"

#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/omnibox_focus_state.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "chrome/renderer/searchbox/searchbox_extension.h"
#include "content/public/renderer/render_view.h"
#include "grit/renderer_resources.h"
#include "net/base/escape.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

// Size of the results cache.
const size_t kMaxInstantAutocompleteResultItemCacheSize = 100;

bool IsThemeInfoEqual(const ThemeBackgroundInfo& new_theme_info,
    const ThemeBackgroundInfo& old_theme_info) {
  return old_theme_info.color_r == new_theme_info.color_r &&
      old_theme_info.color_g == new_theme_info.color_g &&
      old_theme_info.color_b == new_theme_info.color_b &&
      old_theme_info.color_a == new_theme_info.color_a &&
      old_theme_info.theme_id == new_theme_info.theme_id &&
      old_theme_info.image_horizontal_alignment ==
          new_theme_info.image_horizontal_alignment &&
      old_theme_info.image_vertical_alignment ==
          new_theme_info.image_vertical_alignment &&
      old_theme_info.image_tiling == new_theme_info.image_tiling &&
      old_theme_info.image_height == new_theme_info.image_height &&
      old_theme_info.has_attribution == new_theme_info.has_attribution;
}

bool AreMostVisitedItemsEqual(
    const std::vector<InstantMostVisitedItemIDPair>& new_items,
    const std::vector<InstantMostVisitedItemIDPair>& old_items) {
  if (old_items.size() != new_items.size())
    return false;
  for (size_t i = 0; i < new_items.size(); i++) {
    const InstantMostVisitedItem& old_item = old_items[i].second;
    const InstantMostVisitedItem& new_item = new_items[i].second;
    if (new_item.url != old_item.url || new_item.title != old_item.title)
      return false;
  }
  return true;
}

}  // namespace

SearchBox::SearchBox(content::RenderView* render_view)
    : content::RenderViewObserver(render_view),
      content::RenderViewObserverTracker<SearchBox>(render_view),
      verbatim_(false),
      query_is_restricted_(false),
      selection_start_(0),
      selection_end_(0),
      start_margin_(0),
      is_key_capture_enabled_(false),
      display_instant_results_(false),
      omnibox_font_size_(0),
      autocomplete_results_cache_(kMaxInstantAutocompleteResultItemCacheSize),
      most_visited_items_cache_(kMaxInstantMostVisitedItemCacheSize) {
}

SearchBox::~SearchBox() {
}

void SearchBox::SetSuggestions(
    const std::vector<InstantSuggestion>& suggestions) {
  if (!suggestions.empty() &&
      suggestions[0].behavior == INSTANT_COMPLETE_REPLACE) {
    SetQuery(suggestions[0].text, true);
    selection_start_ = selection_end_ = query_.size();
  }
  // Explicitly allow empty vector to be sent to the browser.
  render_view()->Send(new ChromeViewHostMsg_SetSuggestions(
      render_view()->GetRoutingID(), render_view()->GetPageId(), suggestions));
}

void SearchBox::MarkQueryAsRestricted() {
  query_is_restricted_ = true;
  query_.clear();
}

void SearchBox::ShowInstantOverlay(int height, InstantSizeUnits units) {
  render_view()->Send(new ChromeViewHostMsg_ShowInstantOverlay(
      render_view()->GetRoutingID(), render_view()->GetPageId(), height,
      units));
}

void SearchBox::FocusOmnibox() {
  render_view()->Send(new ChromeViewHostMsg_FocusOmnibox(
      render_view()->GetRoutingID(), render_view()->GetPageId(),
      OMNIBOX_FOCUS_VISIBLE));
}

void SearchBox::StartCapturingKeyStrokes() {
  render_view()->Send(new ChromeViewHostMsg_FocusOmnibox(
      render_view()->GetRoutingID(), render_view()->GetPageId(),
      OMNIBOX_FOCUS_INVISIBLE));
}

void SearchBox::StopCapturingKeyStrokes() {
  render_view()->Send(new ChromeViewHostMsg_FocusOmnibox(
      render_view()->GetRoutingID(), render_view()->GetPageId(),
      OMNIBOX_FOCUS_NONE));
}

void SearchBox::NavigateToURL(const GURL& url,
                              content::PageTransition transition,
                              WindowOpenDisposition disposition) {
  render_view()->Send(new ChromeViewHostMsg_SearchBoxNavigate(
      render_view()->GetRoutingID(), render_view()->GetPageId(),
      url, transition, disposition));
}

void SearchBox::DeleteMostVisitedItem(
    InstantRestrictedID most_visited_item_id) {
  render_view()->Send(new ChromeViewHostMsg_SearchBoxDeleteMostVisitedItem(
      render_view()->GetRoutingID(), most_visited_item_id));
}

void SearchBox::UndoMostVisitedDeletion(
    InstantRestrictedID most_visited_item_id) {
  render_view()->Send(new ChromeViewHostMsg_SearchBoxUndoMostVisitedDeletion(
      render_view()->GetRoutingID(), most_visited_item_id));
}

void SearchBox::UndoAllMostVisitedDeletions() {
  render_view()->Send(
      new ChromeViewHostMsg_SearchBoxUndoAllMostVisitedDeletions(
      render_view()->GetRoutingID()));
}

void SearchBox::ShowBars() {
  DVLOG(1) << render_view() << " ShowBars";
  render_view()->Send(new ChromeViewHostMsg_SearchBoxShowBars(
      render_view()->GetRoutingID(), render_view()->GetPageId()));
}

void SearchBox::HideBars() {
  DVLOG(1) << render_view() << " HideBars";
  render_view()->Send(new ChromeViewHostMsg_SearchBoxHideBars(
      render_view()->GetRoutingID(), render_view()->GetPageId()));
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

void SearchBox::GetAutocompleteResults(
    std::vector<InstantAutocompleteResultIDPair>* results) const {
  autocomplete_results_cache_.GetCurrentItems(results);
}

bool SearchBox::GetAutocompleteResultWithID(
    InstantRestrictedID autocomplete_result_id,
    InstantAutocompleteResult* result) const {
  return autocomplete_results_cache_.GetItemWithRestrictedID(
      autocomplete_result_id, result);
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
    IPC_MESSAGE_HANDLER(ChromeViewMsg_SearchBoxBarsHidden, OnBarsHidden)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_DetermineIfPageSupportsInstant,
                        OnDetermineIfPageSupportsInstant)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_SearchBoxAutocompleteResults,
                        OnAutocompleteResults)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_SearchBoxUpOrDownKeyPressed,
                        OnUpOrDownKeyPressed)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_SearchBoxEscKeyPressed, OnEscKeyPressed)
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
    IPC_MESSAGE_HANDLER(ChromeViewMsg_SearchBoxMostVisitedItemsChanged,
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
  SetQuery(query, verbatim);
  selection_start_ = selection_start;
  selection_end_ = selection_end;

  // If |query| is empty, this is due to the user backspacing away all the text
  // in the omnibox, or hitting Escape to restore the "permanent URL", or
  // switching tabs, etc. In all these cases, there will be no corresponding
  // OnAutocompleteResults(), so clear the autocomplete results ourselves, by
  // adding an empty set. Don't notify the page using an "onnativesuggestions"
  // event, though.
  if (query.empty()) {
    autocomplete_results_cache_.AddItems(
        std::vector<InstantAutocompleteResult>());
  }

  if (render_view()->GetWebView() && render_view()->GetWebView()->mainFrame()) {
    DVLOG(1) << render_view() << " OnChange";
    extensions_v8::SearchBoxExtension::DispatchChange(
        render_view()->GetWebView()->mainFrame());
  }
}

void SearchBox::OnSubmit(const string16& query) {
  // Submit() is called when the user hits Enter to commit the omnibox text.
  // If |query| is non-blank, the user committed a search. If it's blank, the
  // omnibox text was a URL, and the user is navigating to it, in which case
  // we shouldn't update the |query_| or associated state.
  if (!query.empty()) {
    SetQuery(query, true);
    selection_start_ = selection_end_ = query_.size();
  }

  if (render_view()->GetWebView() && render_view()->GetWebView()->mainFrame()) {
    DVLOG(1) << render_view() << " OnSubmit";
    extensions_v8::SearchBoxExtension::DispatchSubmit(
        render_view()->GetWebView()->mainFrame());
  }

  if (!query.empty())
    Reset();
}

void SearchBox::OnCancel(const string16& query) {
  SetQuery(query, true);
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

void SearchBox::OnBarsHidden() {
  if (render_view()->GetWebView() && render_view()->GetWebView()->mainFrame()) {
    extensions_v8::SearchBoxExtension::DispatchBarsHidden(
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
  autocomplete_results_cache_.AddItems(results);
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

void SearchBox::OnEscKeyPressed() {
  if (render_view()->GetWebView() && render_view()->GetWebView()->mainFrame()) {
    DVLOG(1) << render_view() << " OnEscKeyPressed ";
    extensions_v8::SearchBoxExtension::DispatchEscKeyPress(
        render_view()->GetWebView()->mainFrame());
  }
}

void SearchBox::OnCancelSelection(const string16& query,
                                  bool verbatim,
                                  size_t selection_start,
                                  size_t selection_end) {
  SetQuery(query, verbatim);
  selection_start_ = selection_start;
  selection_end_ = selection_end;
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
  if (IsThemeInfoEqual(theme_info, theme_info_))
    return;
  theme_info_ = theme_info;
  if (render_view()->GetWebView() && render_view()->GetWebView()->mainFrame()) {
    extensions_v8::SearchBoxExtension::DispatchThemeChange(
        render_view()->GetWebView()->mainFrame());
  }
}

void SearchBox::OnFontInformationReceived(const string16& omnibox_font,
                                          size_t omnibox_font_size) {
  omnibox_font_ = omnibox_font;
  omnibox_font_size_ = omnibox_font_size;
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

void SearchBox::Reset() {
  query_.clear();
  verbatim_ = false;
  query_is_restricted_ = false;
  selection_start_ = 0;
  selection_end_ = 0;
  popup_bounds_ = gfx::Rect();
  start_margin_ = 0;
  is_key_capture_enabled_ = false;
  theme_info_ = ThemeBackgroundInfo();
  // Don't reset display_instant_results_ to prevent clearing it on committed
  // results pages in extended mode. Otherwise resetting it is a no-op because
  // a new loader is created when it changes; see crbug.com/164662.
  // Also don't reset omnibox_font_ or omnibox_font_size_ since it never
  // changes.
}

void SearchBox::SetQuery(const string16& query, bool verbatim) {
  query_ = query;
  verbatim_ = verbatim;
  query_is_restricted_ = false;
}

void SearchBox::OnMostVisitedChanged(
    const std::vector<InstantMostVisitedItemIDPair>& items) {
  std::vector<InstantMostVisitedItemIDPair> old_items;
  most_visited_items_cache_.GetCurrentItems(&old_items);
  if (AreMostVisitedItemsEqual(items, old_items))
    return;

  most_visited_items_cache_.AddItemsWithRestrictedID(items);

  if (render_view()->GetWebView() && render_view()->GetWebView()->mainFrame()) {
    extensions_v8::SearchBoxExtension::DispatchMostVisitedChanged(
        render_view()->GetWebView()->mainFrame());
  }
}

void SearchBox::GetMostVisitedItems(
    std::vector<InstantMostVisitedItemIDPair>* items) const {
  return most_visited_items_cache_.GetCurrentItems(items);
}

bool SearchBox::GetMostVisitedItemWithID(
    InstantRestrictedID most_visited_item_id,
    InstantMostVisitedItem* item) const {
  return most_visited_items_cache_.GetItemWithRestrictedID(most_visited_item_id,
                                                           item);
}
