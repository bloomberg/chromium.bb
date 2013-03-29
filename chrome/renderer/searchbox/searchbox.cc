// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/searchbox/searchbox.h"

#include "base/strings/string_number_conversions.h"
#include "base/utf_string_conversions.h"
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

// The HTML returned when an invalid or unknown restricted ID is requested.
const char kInvalidSuggestionHtml[] =
    "<div style=\"background:red\">invalid rid %d</div>";

// Checks if the input color is in valid range.
bool IsColorValid(int color) {
  return color >= 0 && color <= 0xffffff;
}

// If |url| starts with |prefix|, removes |prefix|.
void StripPrefix(string16* url, const string16& prefix) {
  if (StartsWith(*url, prefix, true))
    url->erase(0, prefix.length());
}

// Removes leading "http://" or "http://www." from |url| unless |userInput|
// starts with those prefixes.
void StripURLPrefixes(string16* url, const string16& userInput) {
  string16 trimmedUserInput;
  TrimWhitespace(userInput, TRIM_TRAILING, &trimmedUserInput);
  if (StartsWith(*url, trimmedUserInput, true))
    return;

  StripPrefix(url, ASCIIToUTF16(chrome::kHttpScheme) + ASCIIToUTF16("://"));

  if (StartsWith(*url, trimmedUserInput, true))
    return;

  StripPrefix(url, ASCIIToUTF16("www."));
}

}  // namespace

SearchBox::SearchBox(content::RenderView* render_view)
    : content::RenderViewObserver(render_view),
      content::RenderViewObserverTracker<SearchBox>(render_view),
      verbatim_(false),
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
    query_ = suggestions[0].text;
    verbatim_ = true;
    selection_start_ = selection_end_ = query_.size();
  }
  // Explicitly allow empty vector to be sent to the browser.
  render_view()->Send(new ChromeViewHostMsg_SetSuggestions(
      render_view()->GetRoutingID(), render_view()->GetPageId(), suggestions));
}

void SearchBox::ClearQuery() {
  query_.clear();
}

void SearchBox::ShowInstantOverlay(int height, InstantSizeUnits units) {
  render_view()->Send(new ChromeViewHostMsg_ShowInstantOverlay(
      render_view()->GetRoutingID(), render_view()->GetPageId(), height,
      units));
}

void SearchBox::FocusOmnibox() {
  render_view()->Send(new ChromeViewHostMsg_FocusOmnibox(
      render_view()->GetRoutingID(), render_view()->GetPageId()));
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

void SearchBox::OnMostVisitedChanged(
    const std::vector<InstantMostVisitedItemIDPair>& items) {
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

bool SearchBox::GenerateDataURLForSuggestionRequest(const GURL& request_url,
                                                    GURL* data_url) const {
  DCHECK(data_url);

  // The origin URL is required so that the iframe knows what origin to post
  // messages to.
  WebKit::WebView* webview = render_view()->GetWebView();
  if (!webview)
    return false;
  GURL embedder_url(webview->mainFrame()->document().url());
  GURL embedder_origin = embedder_url.GetOrigin();
  if (!embedder_origin.is_valid())
    return false;

  DCHECK(StartsWithASCII(request_url.path(), "/", true));
  std::string restricted_id_str = request_url.path().substr(1);

  InstantRestrictedID restricted_id = 0;
  DCHECK_EQ(sizeof(InstantRestrictedID), sizeof(int));
  if (!base::StringToInt(restricted_id_str, &restricted_id))
    return false;

  std::string response_html;
  InstantAutocompleteResult result;
  if (autocomplete_results_cache_.GetItemWithRestrictedID(
          restricted_id, &result)) {
    std::string template_html =
        ResourceBundle::GetSharedInstance().GetRawDataResource(
            IDR_OMNIBOX_RESULT).as_string();

    DCHECK(IsColorValid(autocomplete_results_style_.url_color));
    DCHECK(IsColorValid(autocomplete_results_style_.title_color));

    string16 contents;
    if (result.search_query.empty()) {
      contents = result.destination_url;
      FormatURLForDisplay(&contents);
    } else {
      contents = result.search_query;
    }

    // First, HTML-encode the text so that '&', '<' and such lose their special
    // meaning. Next, URL-encode the text because it will be inserted into
    // "data:" URIs; thus '%' and such lose their special meaning.
    std::string encoded_contents = net::EscapeQueryParamValue(
        net::EscapeForHTML(UTF16ToUTF8(contents)), false);
    std::string encoded_description = net::EscapeQueryParamValue(
        net::EscapeForHTML(UTF16ToUTF8(result.description)), false);

    response_html = base::StringPrintf(
        template_html.c_str(),
        embedder_origin.spec().c_str(),
        UTF16ToUTF8(omnibox_font_).c_str(),
        omnibox_font_size_,
        autocomplete_results_style_.url_color,
        autocomplete_results_style_.title_color,
        encoded_contents.c_str(),
        encoded_description.c_str());
  } else {
    response_html = base::StringPrintf(kInvalidSuggestionHtml, restricted_id);
  }

  *data_url = GURL("data:text/html;charset=utf-8," + response_html);
  return true;
}

void SearchBox::SetInstantAutocompleteResultStyle(
    const InstantAutocompleteResultStyle& style) {
  if (IsColorValid(style.url_color) && IsColorValid(style.title_color))
    autocomplete_results_style_ = style;
}

void SearchBox::FormatURLForDisplay(string16* url) const {
  StripURLPrefixes(url, query());

  string16 trimmedUserInput;
  TrimWhitespace(query(), TRIM_LEADING, &trimmedUserInput);
  if (EndsWith(*url, trimmedUserInput, true))
    return;

  // Strip a lone trailing slash.
  if (EndsWith(*url, ASCIIToUTF16("/"), true))
    url->erase(url->length() - 1, 1);
}
