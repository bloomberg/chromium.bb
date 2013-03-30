// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/searchbox/searchbox_extension.h"

#include "base/i18n/rtl.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/strings/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/url_constants.h"
#include "chrome/renderer/searchbox/searchbox.h"
#include "content/public/renderer/render_view.h"
#include "grit/renderer_resources.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebURLRequest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebScriptSource.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/window_open_disposition.h"
#include "v8/include/v8.h"

namespace {

const char kCSSBackgroundImageFormat[] = "-webkit-image-set("
    "url(chrome-search://theme/IDR_THEME_NTP_BACKGROUND?%s) 1x)";

const char kCSSBackgroundColorFormat[] = "rgba(%d,%d,%d,%s)";

const char kCSSBackgroundPositionCenter[] = "center";
const char kCSSBackgroundPositionLeft[] = "left";
const char kCSSBackgroundPositionTop[] = "top";
const char kCSSBackgroundPositionRight[] = "right";
const char kCSSBackgroundPositionBottom[] = "bottom";

const char kCSSBackgroundRepeatNo[] = "no-repeat";
const char kCSSBackgroundRepeatX[] = "repeat-x";
const char kCSSBackgroundRepeatY[] = "repeat-y";
const char kCSSBackgroundRepeat[] = "repeat";

const char kThemeAttributionUrl[] =
    "chrome-search://theme/IDR_THEME_NTP_ATTRIBUTION";

const char kLTRHtmlTextDirection[] = "ltr";
const char kRTLHtmlTextDirection[] = "rtl";

// Converts a V8 value to a string16.
string16 V8ValueToUTF16(v8::Handle<v8::Value> v) {
  v8::String::Value s(v);
  return string16(reinterpret_cast<const char16*>(*s), s.length());
}

// Converts string16 to V8 String.
v8::Handle<v8::String> UTF16ToV8String(const string16& s) {
  return v8::String::New(reinterpret_cast<const uint16_t*>(s.data()), s.size());
}

// Converts std::string to V8 String.
v8::Handle<v8::String> UTF8ToV8String(const std::string& s) {
  return v8::String::New(s.data(), s.size());
}

// Converts a V8 value to a std::string.
std::string V8ValueToUTF8(v8::Handle<v8::Value> v) {
  std::string result;

  if (v->IsStringObject()) {
    v8::Handle<v8::String> s(v->ToString());
    int len = s->Length();
    if (len > 0)
      s->WriteUtf8(WriteInto(&result, len + 1));
  }
  return result;
}

void Dispatch(WebKit::WebFrame* frame, const WebKit::WebString& script) {
  if (!frame) return;
  frame->executeScript(WebKit::WebScriptSource(script));
}

v8::Handle<v8::String> GenerateThumbnailURL(uint64 most_visited_item_id) {
  return UTF8ToV8String(
      base::StringPrintf("chrome-search://thumb/%s",
                         base::Uint64ToString(most_visited_item_id).c_str()));
}

v8::Handle<v8::String> GenerateFaviconURL(uint64 most_visited_item_id) {
  return UTF8ToV8String(
      base::StringPrintf("chrome-search://favicon/%s",
                         base::Uint64ToString(most_visited_item_id).c_str()));
}

}  // namespace

namespace extensions_v8 {

static const char kSearchBoxExtensionName[] = "v8/EmbeddedSearch";

static const char kDispatchChangeEventScript[] =
    "if (window.chrome &&"
    "    window.chrome.embeddedSearch &&"
    "    window.chrome.embeddedSearch.searchBox &&"
    "    window.chrome.embeddedSearch.searchBox.onchange &&"
    "    typeof window.chrome.embeddedSearch.searchBox.onchange =="
    "        'function') {"
    "  window.chrome.embeddedSearch.searchBox.onchange();"
    "  true;"
    "}";

static const char kDispatchSubmitEventScript[] =
    "if (window.chrome &&"
    "    window.chrome.embeddedSearch &&"
    "    window.chrome.embeddedSearch.searchBox &&"
    "    window.chrome.embeddedSearch.searchBox.onsubmit &&"
    "    typeof window.chrome.embeddedSearch.searchBox.onsubmit =="
    "        'function') {"
    "  window.chrome.embeddedSearch.searchBox.onsubmit();"
    "  true;"
    "}";

static const char kDispatchCancelEventScript[] =
    "if (window.chrome &&"
    "    window.chrome.embeddedSearch &&"
    "    window.chrome.embeddedSearch.searchBox &&"
    "    window.chrome.embeddedSearch.searchBox.oncancel &&"
    "    typeof window.chrome.embeddedSearch.searchBox.oncancel =="
    "        'function') {"
    "  window.chrome.embeddedSearch.searchBox.oncancel();"
    "  true;"
    "}";

static const char kDispatchResizeEventScript[] =
    "if (window.chrome &&"
    "    window.chrome.embeddedSearch &&"
    "    window.chrome.embeddedSearch.searchBox &&"
    "    window.chrome.embeddedSearch.searchBox.onresize &&"
    "    typeof window.chrome.embeddedSearch.searchBox.onresize =="
    "        'function') {"
    "  window.chrome.embeddedSearch.searchBox.onresize();"
    "  true;"
    "}";

// We first send this script down to determine if the page supports instant.
static const char kSupportsInstantScript[] =
    "if (window.chrome &&"
    "    window.chrome.embeddedSearch &&"
    "    window.chrome.embeddedSearch.searchBox &&"
    "    window.chrome.embeddedSearch.searchBox.onsubmit &&"
    "    typeof window.chrome.embeddedSearch.searchBox.onsubmit =="
    "        'function') {"
    "  true;"
    "} else {"
    "  false;"
    "}";

// Extended API.

// Per-context initialization.
static const char kDispatchOnWindowReady[] =
    "if (window.chrome &&"
    "    window.chrome.embeddedSearchOnWindowReady &&"
    "    typeof window.chrome.embeddedSearchOnWindowReady == 'function') {"
    "  window.chrome.embeddedSearchOnWindowReady();"
    "}";

static const char kDispatchAutocompleteResultsEventScript[] =
    "if (window.chrome &&"
    "    window.chrome.embeddedSearch &&"
    "    window.chrome.embeddedSearch.searchBox &&"
    "    window.chrome.embeddedSearch.searchBox.onnativesuggestions &&"
    "    typeof window.chrome.embeddedSearch.searchBox.onnativesuggestions =="
    "        'function') {"
    "  window.chrome.embeddedSearch.searchBox.onnativesuggestions();"
    "  true;"
    "}";

// Takes two printf-style replaceable values: count, key_code.
static const char kDispatchUpOrDownKeyPressEventScript[] =
    "if (window.chrome &&"
    "    window.chrome.embeddedSearch &&"
    "    window.chrome.embeddedSearch.searchBox &&"
    "    window.chrome.embeddedSearch.searchBox.onkeypress &&"
    "    typeof window.chrome.embeddedSearch.searchBox.onkeypress =="
    "        'function') {"
    "  for (var i = 0; i < %d; ++i)"
    "    window.chrome.embeddedSearch.searchBox.onkeypress({keyCode: %d});"
    "  true;"
    "}";

// Takes one printf-style replaceable value: key_code.
static const char kDispatchEscKeyPressEventScript[] =
    "if (window.chrome &&"
    "    window.chrome.embeddedSearch &&"
    "    window.chrome.embeddedSearch.searchBox &&"
    "    window.chrome.embeddedSearch.searchBox.onkeypress &&"
    "    typeof window.chrome.embeddedSearch.searchBox.onkeypress =="
    "        'function') {"
    "  window.chrome.embeddedSearch.searchBox.onkeypress({keyCode: %d});"
    "  true;"
    "}";

static const char kDispatchKeyCaptureChangeScript[] =
    "if (window.chrome &&"
    "    window.chrome.embeddedSearch &&"
    "    window.chrome.embeddedSearch.searchBox &&"
    "    window.chrome.embeddedSearch.searchBox.onkeycapturechange &&"
    "    typeof window.chrome.embeddedSearch.searchBox.onkeycapturechange =="
    "        'function') {"
    "  window.chrome.embeddedSearch.searchBox.onkeycapturechange();"
    "  true;"
    "}";

static const char kDispatchThemeChangeEventScript[] =
    "if (window.chrome &&"
    "    window.chrome.embeddedSearch &&"
    "    window.chrome.embeddedSearch.newTabPage &&"
    "    window.chrome.embeddedSearch.newTabPage.onthemechange &&"
    "    typeof window.chrome.embeddedSearch.newTabPage.onthemechange =="
    "        'function') {"
    "  window.chrome.embeddedSearch.newTabPage.onthemechange();"
    "  true;"
    "}";

static const char kDispatchMarginChangeEventScript[] =
    "if (window.chrome &&"
    "    window.chrome.embeddedSearch &&"
    "    window.chrome.embeddedSearch.searchBox &&"
    "    window.chrome.embeddedSearch.searchBox.onmarginchange &&"
    "    typeof window.chrome.embeddedSearch.searchBox.onmarginchange =="
    "        'function') {"
    "  window.chrome.embeddedSearch.searchBox.onmarginchange();"
    "  true;"
    "}";

static const char kDispatchMostVisitedChangedScript[] =
    "if (window.chrome &&"
    "    window.chrome.embeddedSearch &&"
    "    window.chrome.embeddedSearch.newTabPage &&"
    "    window.chrome.embeddedSearch.newTabPage.onmostvisitedchange &&"
    "    typeof window.chrome.embeddedSearch.newTabPage.onmostvisitedchange =="
    "         'function') {"
    "  window.chrome.embeddedSearch.newTabPage.onmostvisitedchange();"
    "  true;"
    "}";

static const char kDispatchBarsHiddenEventScript[] =
    "if (window.chrome &&"
    "    window.chrome.searchBox &&"
    "    window.chrome.searchBox.onbarshidden &&"
    "    typeof window.chrome.searchBox.onbarshidden == 'function') {"
    "  window.chrome.searchBox.onbarshidden();"
    "  true;"
    "}";

// ----------------------------------------------------------------------------

class SearchBoxExtensionWrapper : public v8::Extension {
 public:
  explicit SearchBoxExtensionWrapper(const base::StringPiece& code);

  // Allows v8's javascript code to call the native functions defined
  // in this class for window.chrome.
  virtual v8::Handle<v8::FunctionTemplate> GetNativeFunction(
      v8::Handle<v8::String> name) OVERRIDE;

  // Helper function to find the RenderView. May return NULL.
  static content::RenderView* GetRenderView();

  // Deletes a Most Visited item.
  static v8::Handle<v8::Value> DeleteMostVisitedItem(const v8::Arguments& args);

  // Gets the value of the user's search query.
  static v8::Handle<v8::Value> GetQuery(const v8::Arguments& args);

  // Gets whether the |value| should be considered final -- as opposed to a
  // partial match. This may be set if the user clicks a suggestion, presses
  // forward delete, or in other cases where Chrome overrides.
  static v8::Handle<v8::Value> GetVerbatim(const v8::Arguments& args);

  // Gets the start of the selection in the search box.
  static v8::Handle<v8::Value> GetSelectionStart(const v8::Arguments& args);

  // Gets the end of the selection in the search box.
  static v8::Handle<v8::Value> GetSelectionEnd(const v8::Arguments& args);

  // Gets the x coordinate (relative to |window|) of the left edge of the
  // region of the search box that overlaps the window.
  static v8::Handle<v8::Value> GetX(const v8::Arguments& args);

  // Gets the y coordinate (relative to |window|) of the right edge of the
  // region of the search box that overlaps the window.
  static v8::Handle<v8::Value> GetY(const v8::Arguments& args);

  // Gets the width of the region of the search box that overlaps the window,
  // i.e., the width of the omnibox.
  static v8::Handle<v8::Value> GetWidth(const v8::Arguments& args);

  // Gets the height of the region of the search box that overlaps the window.
  static v8::Handle<v8::Value> GetHeight(const v8::Arguments& args);

  // Gets Most Visited Items.
  static v8::Handle<v8::Value> GetMostVisitedItems(const v8::Arguments& args);

  // Gets the start-edge margin to use with extended Instant.
  static v8::Handle<v8::Value> GetStartMargin(const v8::Arguments& args);

  // Returns true if the Searchbox itself is oriented right-to-left.
  static v8::Handle<v8::Value> GetRightToLeft(const v8::Arguments& args);

  // Gets the autocomplete results from search box.
  static v8::Handle<v8::Value> GetAutocompleteResults(
      const v8::Arguments& args);

  // Gets whether to display Instant results.
  static v8::Handle<v8::Value> GetDisplayInstantResults(
      const v8::Arguments& args);

  // Gets the background info of the theme currently adopted by browser.
  // Call only when overlay is showing NTP page.
  static v8::Handle<v8::Value> GetThemeBackgroundInfo(
      const v8::Arguments& args);

  // Gets whether the browser is capturing key strokes.
  static v8::Handle<v8::Value> IsKeyCaptureEnabled(const v8::Arguments& args);

  // Gets the font family of the text in the omnibox.
  static v8::Handle<v8::Value> GetFont(const v8::Arguments& args);

  // Gets the font size of the text in the omnibox.
  static v8::Handle<v8::Value> GetFontSize(const v8::Arguments& args);

  // Gets the URL prefix for the iframe used to display suggestions.
  static v8::Handle<v8::Value> GetSuggestionIframeURLPrefix(
      const v8::Arguments& args);

  // Navigates the window to a URL represented by either a URL string or a
  // restricted ID. The two variants handle restricted IDs in their
  // respective namespaces.
  static v8::Handle<v8::Value> NavigateSearchBox(const v8::Arguments& args);
  static v8::Handle<v8::Value> NavigateNewTabPage(const v8::Arguments& args);
  // DEPRECATED: TODO(sreeram): Remove when google.com no longer uses this.
  static v8::Handle<v8::Value> NavigateContentWindow(const v8::Arguments& args);

  // Sets ordered suggestions. Valid for current |value|.
  static v8::Handle<v8::Value> SetSuggestions(const v8::Arguments& args);

  // Sets the text to be autocompleted into the search box.
  static v8::Handle<v8::Value> SetSuggestion(const v8::Arguments& args);

  // Like SetSuggestion() but uses a restricted autocomplete result ID to
  // identify the text.
  static v8::Handle<v8::Value> SetSuggestionFromAutocompleteResult(
      const v8::Arguments& args);

  // Sets the search box text, completely replacing what the user typed.
  static v8::Handle<v8::Value> SetQuery(const v8::Arguments& args);

  // Like |SetQuery| but uses a restricted autocomplete result ID to identify
  // the text.
  static v8::Handle<v8::Value> SetQueryFromAutocompleteResult(
      const v8::Arguments& args);

  // Requests the overlay be shown with the specified contents and height.
  static v8::Handle<v8::Value> ShowOverlay(const v8::Arguments& args);

  // Sets the focus to the omnibox.
  static v8::Handle<v8::Value> FocusOmnibox(const v8::Arguments& args);

  // Start capturing user key strokes.
  static v8::Handle<v8::Value> StartCapturingKeyStrokes(
      const v8::Arguments& args);

  // Stop capturing user key strokes.
  static v8::Handle<v8::Value> StopCapturingKeyStrokes(
      const v8::Arguments& args);

  // Set the CSS values for the dropdown.
  static v8::Handle<v8::Value> SetSuggestionStyle(const v8::Arguments& args);

  // Undoes the deletion of all Most Visited itens.
  static v8::Handle<v8::Value> UndoAllMostVisitedDeletions(
      const v8::Arguments& args);

  // Undoes the deletion of a Most Visited item.
  static v8::Handle<v8::Value> UndoMostVisitedDeletion(
      const v8::Arguments& args);

  // Shows any attached bars.
  static v8::Handle<v8::Value> ShowBars(const v8::Arguments& args);

  // Hides any attached bars.  When the bars are hidden, the "onbarshidden"
  // event is fired to notify the page.
  static v8::Handle<v8::Value> HideBars(const v8::Arguments& args);

  // Returns true if the Instant page should use iframes to display suggestions.
  static v8::Handle<v8::Value> ShouldUseIframes(const v8::Arguments& args);

 private:
  DISALLOW_COPY_AND_ASSIGN(SearchBoxExtensionWrapper);
};

SearchBoxExtensionWrapper::SearchBoxExtensionWrapper(
    const base::StringPiece& code)
    : v8::Extension(kSearchBoxExtensionName, code.data(), 0, 0, code.size()) {
}

v8::Handle<v8::FunctionTemplate> SearchBoxExtensionWrapper::GetNativeFunction(
    v8::Handle<v8::String> name) {
  if (name->Equals(v8::String::New("DeleteMostVisitedItem")))
    return v8::FunctionTemplate::New(DeleteMostVisitedItem);
  if (name->Equals(v8::String::New("GetQuery")))
    return v8::FunctionTemplate::New(GetQuery);
  if (name->Equals(v8::String::New("GetVerbatim")))
    return v8::FunctionTemplate::New(GetVerbatim);
  if (name->Equals(v8::String::New("GetSelectionStart")))
    return v8::FunctionTemplate::New(GetSelectionStart);
  if (name->Equals(v8::String::New("GetSelectionEnd")))
    return v8::FunctionTemplate::New(GetSelectionEnd);
  if (name->Equals(v8::String::New("GetX")))
    return v8::FunctionTemplate::New(GetX);
  if (name->Equals(v8::String::New("GetY")))
    return v8::FunctionTemplate::New(GetY);
  if (name->Equals(v8::String::New("GetWidth")))
    return v8::FunctionTemplate::New(GetWidth);
  if (name->Equals(v8::String::New("GetHeight")))
    return v8::FunctionTemplate::New(GetHeight);
  if (name->Equals(v8::String::New("GetMostVisitedItems")))
    return v8::FunctionTemplate::New(GetMostVisitedItems);
  if (name->Equals(v8::String::New("GetStartMargin")))
    return v8::FunctionTemplate::New(GetStartMargin);
  if (name->Equals(v8::String::New("GetRightToLeft")))
    return v8::FunctionTemplate::New(GetRightToLeft);
  if (name->Equals(v8::String::New("GetAutocompleteResults")))
    return v8::FunctionTemplate::New(GetAutocompleteResults);
  if (name->Equals(v8::String::New("GetDisplayInstantResults")))
    return v8::FunctionTemplate::New(GetDisplayInstantResults);
  if (name->Equals(v8::String::New("GetThemeBackgroundInfo")))
    return v8::FunctionTemplate::New(GetThemeBackgroundInfo);
  if (name->Equals(v8::String::New("IsKeyCaptureEnabled")))
    return v8::FunctionTemplate::New(IsKeyCaptureEnabled);
  if (name->Equals(v8::String::New("GetFont")))
    return v8::FunctionTemplate::New(GetFont);
  if (name->Equals(v8::String::New("GetFontSize")))
    return v8::FunctionTemplate::New(GetFontSize);
  if (name->Equals(v8::String::New("GetSuggestionIframeURLPrefix")))
    return v8::FunctionTemplate::New(GetSuggestionIframeURLPrefix);
  if (name->Equals(v8::String::New("NavigateSearchBox")))
    return v8::FunctionTemplate::New(NavigateSearchBox);
  if (name->Equals(v8::String::New("NavigateNewTabPage")))
    return v8::FunctionTemplate::New(NavigateNewTabPage);
  if (name->Equals(v8::String::New("NavigateContentWindow")))
    return v8::FunctionTemplate::New(NavigateContentWindow);
  if (name->Equals(v8::String::New("SetSuggestions")))
    return v8::FunctionTemplate::New(SetSuggestions);
  if (name->Equals(v8::String::New("SetSuggestion")))
    return v8::FunctionTemplate::New(SetSuggestion);
  if (name->Equals(v8::String::New("SetSuggestionFromAutocompleteResult")))
    return v8::FunctionTemplate::New(SetSuggestionFromAutocompleteResult);
  if (name->Equals(v8::String::New("SetQuery")))
    return v8::FunctionTemplate::New(SetQuery);
  if (name->Equals(v8::String::New("SetQueryFromAutocompleteResult")))
    return v8::FunctionTemplate::New(SetQueryFromAutocompleteResult);
  if (name->Equals(v8::String::New("ShowOverlay")))
    return v8::FunctionTemplate::New(ShowOverlay);
  if (name->Equals(v8::String::New("FocusOmnibox")))
    return v8::FunctionTemplate::New(FocusOmnibox);
  if (name->Equals(v8::String::New("StartCapturingKeyStrokes")))
    return v8::FunctionTemplate::New(StartCapturingKeyStrokes);
  if (name->Equals(v8::String::New("StopCapturingKeyStrokes")))
    return v8::FunctionTemplate::New(StopCapturingKeyStrokes);
  if (name->Equals(v8::String::New("SetSuggestionStyle")))
    return v8::FunctionTemplate::New(SetSuggestionStyle);
  if (name->Equals(v8::String::New("UndoAllMostVisitedDeletions")))
    return v8::FunctionTemplate::New(UndoAllMostVisitedDeletions);
  if (name->Equals(v8::String::New("UndoMostVisitedDeletion")))
    return v8::FunctionTemplate::New(UndoMostVisitedDeletion);
  if (name->Equals(v8::String::New("ShowBars")))
    return v8::FunctionTemplate::New(ShowBars);
  if (name->Equals(v8::String::New("HideBars")))
    return v8::FunctionTemplate::New(HideBars);
  if (name->Equals(v8::String::New("ShouldUseIframes")))
    return v8::FunctionTemplate::New(ShouldUseIframes);
  return v8::Handle<v8::FunctionTemplate>();
}

// static
content::RenderView* SearchBoxExtensionWrapper::GetRenderView() {
  WebKit::WebFrame* webframe = WebKit::WebFrame::frameForCurrentContext();
  if (!webframe) return NULL;

  WebKit::WebView* webview = webframe->view();
  if (!webview) return NULL;  // can happen during closing

  return content::RenderView::FromWebView(webview);
}

// static
v8::Handle<v8::Value> SearchBoxExtensionWrapper::GetQuery(
    const v8::Arguments& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view) return v8::Undefined();

  DVLOG(1) << render_view << " GetQuery: '"
           << SearchBox::Get(render_view)->query() << "'";
  return UTF16ToV8String(SearchBox::Get(render_view)->query());
}

// static
v8::Handle<v8::Value> SearchBoxExtensionWrapper::GetVerbatim(
    const v8::Arguments& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view) return v8::Undefined();

  DVLOG(1) << render_view << " GetVerbatim: "
           << SearchBox::Get(render_view)->verbatim();
  return v8::Boolean::New(SearchBox::Get(render_view)->verbatim());
}

// static
v8::Handle<v8::Value> SearchBoxExtensionWrapper::GetSelectionStart(
    const v8::Arguments& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view) return v8::Undefined();

  return v8::Uint32::New(SearchBox::Get(render_view)->selection_start());
}

// static
v8::Handle<v8::Value> SearchBoxExtensionWrapper::GetSelectionEnd(
    const v8::Arguments& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view) return v8::Undefined();

  return v8::Uint32::New(SearchBox::Get(render_view)->selection_end());
}

// static
v8::Handle<v8::Value> SearchBoxExtensionWrapper::GetX(
    const v8::Arguments& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view) return v8::Undefined();

  return v8::Int32::New(SearchBox::Get(render_view)->GetPopupBounds().x());
}

// static
v8::Handle<v8::Value> SearchBoxExtensionWrapper::GetY(
    const v8::Arguments& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view) return v8::Undefined();

  return v8::Int32::New(SearchBox::Get(render_view)->GetPopupBounds().y());
}

// static
v8::Handle<v8::Value> SearchBoxExtensionWrapper::GetWidth(
    const v8::Arguments& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view) return v8::Undefined();
  return v8::Int32::New(SearchBox::Get(render_view)->GetPopupBounds().width());
}

// static
v8::Handle<v8::Value> SearchBoxExtensionWrapper::GetHeight(
    const v8::Arguments& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view) return v8::Undefined();

  return v8::Int32::New(SearchBox::Get(render_view)->GetPopupBounds().height());
}

// static
v8::Handle<v8::Value> SearchBoxExtensionWrapper::GetStartMargin(
    const v8::Arguments& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view) return v8::Undefined();
  return v8::Int32::New(SearchBox::Get(render_view)->GetStartMargin());
}

// static
v8::Handle<v8::Value> SearchBoxExtensionWrapper::GetRightToLeft(
    const v8::Arguments& args) {
  return v8::Boolean::New(base::i18n::IsRTL());
}

// static
v8::Handle<v8::Value> SearchBoxExtensionWrapper::GetAutocompleteResults(
    const v8::Arguments& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view) return v8::Undefined();

  DVLOG(1) << render_view << " GetAutocompleteResults";
  std::vector<InstantAutocompleteResultIDPair> results;
  SearchBox::Get(render_view)->GetAutocompleteResults(&results);

  v8::Handle<v8::Array> results_array = v8::Array::New(results.size());
  for (size_t i = 0; i < results.size(); ++i) {
    v8::Handle<v8::Object> result = v8::Object::New();
    result->Set(v8::String::New("provider"),
                UTF16ToV8String(results[i].second.provider));
    result->Set(v8::String::New("type"),
                UTF16ToV8String(results[i].second.type));
    result->Set(v8::String::New("description"),
                UTF16ToV8String(results[i].second.description));
    result->Set(v8::String::New("destination_url"),
                UTF16ToV8String(results[i].second.destination_url));
    if (results[i].second.search_query.empty()) {
      string16 url = results[i].second.destination_url;
      SearchBox::Get(render_view)->FormatURLForDisplay(&url);
      result->Set(v8::String::New("contents"), UTF16ToV8String(url));
    } else {
      result->Set(v8::String::New("contents"),
                  UTF16ToV8String(results[i].second.search_query));
      result->Set(v8::String::New("is_search"), v8::Boolean::New(true));
    }
    result->Set(v8::String::New("rid"), v8::Uint32::New(results[i].first));

    v8::Handle<v8::Object> ranking_data = v8::Object::New();
    ranking_data->Set(v8::String::New("relevance"),
                      v8::Int32::New(results[i].second.relevance));
    result->Set(v8::String::New("rankingData"), ranking_data);

    results_array->Set(i, result);
  }
  return results_array;
}

// static
v8::Handle<v8::Value> SearchBoxExtensionWrapper::IsKeyCaptureEnabled(
    const v8::Arguments& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view) return v8::Undefined();

  return v8::Boolean::New(SearchBox::Get(render_view)->
                          is_key_capture_enabled());
}

// static
v8::Handle<v8::Value> SearchBoxExtensionWrapper::GetDisplayInstantResults(
    const v8::Arguments& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view) return v8::Undefined();

  bool display_instant_results =
      SearchBox::Get(render_view)->display_instant_results();
  DVLOG(1) << render_view << " "
           << "GetDisplayInstantResults: " << display_instant_results;
  return v8::Boolean::New(display_instant_results);
}

// static
v8::Handle<v8::Value> SearchBoxExtensionWrapper::GetThemeBackgroundInfo(
    const v8::Arguments& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view) return v8::Undefined();

  DVLOG(1) << render_view << " GetThemeBackgroundInfo";
  const ThemeBackgroundInfo& theme_info =
      SearchBox::Get(render_view)->GetThemeBackgroundInfo();
  v8::Handle<v8::Object> info = v8::Object::New();

  // The theme background color is in RGBA format "rgba(R,G,B,A)" where R, G and
  // B are between 0 and 255 inclusive, and A is a double between 0 and 1
  // inclusive.
  // This is the CSS "background-color" format.
  // Value is always valid.
  info->Set(v8::String::New("colorRgba"), UTF8ToV8String(
      // Convert the alpha using DoubleToString because StringPrintf will use
      // locale specific formatters (e.g., use , instead of . in German).
      base::StringPrintf(
          kCSSBackgroundColorFormat,
          theme_info.color_r,
          theme_info.color_g,
          theme_info.color_b,
          base::DoubleToString(theme_info.color_a / 255.0).c_str())));

  // The theme background image url is of format
  // "-webkit-image-set(url(chrome://theme/IDR_THEME_BACKGROUND?<theme_id>) 1x)"
  // where <theme_id> is the id that identifies the theme.
  // This is the CSS "background-image" format.
  // Value is only valid if there's a custom theme background image.
  if (extensions::Extension::IdIsValid(theme_info.theme_id)) {
    info->Set(v8::String::New("imageUrl"), UTF8ToV8String(
        base::StringPrintf(kCSSBackgroundImageFormat,
                           theme_info.theme_id.c_str())));

    // The theme background image horizontal alignment is one of "left",
    // "right", "center".
    // This is the horizontal component of the CSS "background-position" format.
    // Value is only valid if |imageUrl| is not empty.
    std::string alignment = kCSSBackgroundPositionCenter;
    if (theme_info.image_horizontal_alignment ==
            THEME_BKGRND_IMAGE_ALIGN_LEFT) {
      alignment = kCSSBackgroundPositionLeft;
    } else if (theme_info.image_horizontal_alignment ==
                   THEME_BKGRND_IMAGE_ALIGN_RIGHT) {
      alignment = kCSSBackgroundPositionRight;
    }
    info->Set(v8::String::New("imageHorizontalAlignment"),
              UTF8ToV8String(alignment));

    // The theme background image vertical alignment is one of "top", "bottom",
    // "center".
    // This is the vertical component of the CSS "background-position" format.
    // Value is only valid if |image_url| is not empty.
    if (theme_info.image_vertical_alignment == THEME_BKGRND_IMAGE_ALIGN_TOP) {
      alignment = kCSSBackgroundPositionTop;
    } else if (theme_info.image_vertical_alignment ==
                   THEME_BKGRND_IMAGE_ALIGN_BOTTOM) {
      alignment = kCSSBackgroundPositionBottom;
    } else {
      alignment = kCSSBackgroundPositionCenter;
    }
    info->Set(v8::String::New("imageVerticalAlignment"),
              UTF8ToV8String(alignment));

    // The tiling of the theme background image is one of "no-repeat",
    // "repeat-x", "repeat-y", "repeat".
    // This is the CSS "background-repeat" format.
    // Value is only valid if |image_url| is not empty.
    std::string tiling = kCSSBackgroundRepeatNo;
    switch (theme_info.image_tiling) {
      case THEME_BKGRND_IMAGE_NO_REPEAT:
        tiling = kCSSBackgroundRepeatNo;
        break;
      case THEME_BKGRND_IMAGE_REPEAT_X:
        tiling = kCSSBackgroundRepeatX;
        break;
      case THEME_BKGRND_IMAGE_REPEAT_Y:
        tiling = kCSSBackgroundRepeatY;
        break;
      case THEME_BKGRND_IMAGE_REPEAT:
        tiling = kCSSBackgroundRepeat;
        break;
    }
    info->Set(v8::String::New("imageTiling"), UTF8ToV8String(tiling));

    // The theme background image height is only valid if |imageUrl| is valid.
    info->Set(v8::String::New("imageHeight"),
              v8::Int32::New(theme_info.image_height));

    // The attribution URL is only valid if the theme has attribution logo.
    if (theme_info.has_attribution) {
      info->Set(v8::String::New("attributionUrl"),
                UTF8ToV8String(kThemeAttributionUrl));
    }
  }

  return info;
}

// static
v8::Handle<v8::Value> SearchBoxExtensionWrapper::GetFont(
    const v8::Arguments& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view) return v8::Undefined();

  return UTF16ToV8String(SearchBox::Get(render_view)->omnibox_font());
}

// static
v8::Handle<v8::Value> SearchBoxExtensionWrapper::GetFontSize(
    const v8::Arguments& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view) return v8::Undefined();

  return v8::Uint32::New(SearchBox::Get(render_view)->omnibox_font_size());
}

// static
v8::Handle<v8::Value> SearchBoxExtensionWrapper::GetSuggestionIframeURLPrefix(
    const v8::Arguments& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view) return v8::Undefined();

  return UTF8ToV8String(base::StringPrintf(
      "%s://%s/",
      chrome::kChromeSearchScheme,
      chrome::kChromeSearchSuggestionHost));
}

// static
v8::Handle<v8::Value> SearchBoxExtensionWrapper::NavigateSearchBox(
    const v8::Arguments& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view || !args.Length()) return v8::Undefined();

  GURL destination_url;
  content::PageTransition transition = content::PAGE_TRANSITION_TYPED;
  if (args[0]->IsNumber()) {
    InstantAutocompleteResult result;
    if (SearchBox::Get(render_view)->GetAutocompleteResultWithID(
            args[0]->IntegerValue(), &result)) {
      destination_url = GURL(result.destination_url);
      transition = result.transition;
    }
  } else {
    // Resolve the URL.
    const string16& possibly_relative_url = V8ValueToUTF16(args[0]);
    WebKit::WebView* webview = render_view->GetWebView();
    if (!possibly_relative_url.empty() && webview) {
      GURL current_url(webview->mainFrame()->document().url());
      destination_url = current_url.Resolve(possibly_relative_url);
    }
  }

  DVLOG(1) << render_view << " NavigateSearchBox: " << destination_url;

  // Navigate the main frame.
  if (destination_url.is_valid()) {
    WindowOpenDisposition disposition = CURRENT_TAB;
    if (args[1]->Uint32Value() == 2)
      disposition = NEW_BACKGROUND_TAB;
    SearchBox::Get(render_view)->NavigateToURL(
        destination_url, transition, disposition);
  }

  return v8::Undefined();
}

// static
v8::Handle<v8::Value> SearchBoxExtensionWrapper::NavigateNewTabPage(
    const v8::Arguments& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view || !args.Length()) return v8::Undefined();

  GURL destination_url;
  content::PageTransition transition = content::PAGE_TRANSITION_AUTO_BOOKMARK;
  if (args[0]->IsNumber()) {
    InstantMostVisitedItem item;
    if (SearchBox::Get(render_view)->GetMostVisitedItemWithID(
            args[0]->IntegerValue(), &item)) {
      destination_url = item.url;
    }
  } else {
    destination_url = GURL(V8ValueToUTF16(args[0]));
  }

  DVLOG(1) << render_view << " NavigateNewTabPage: " << destination_url;

  // Navigate the main frame.
  if (destination_url.is_valid()) {
    WindowOpenDisposition disposition = CURRENT_TAB;
    if (args[1]->Uint32Value() == 2)
      disposition = NEW_BACKGROUND_TAB;
    SearchBox::Get(render_view)->NavigateToURL(
        destination_url, transition, disposition);
  }
  return v8::Undefined();
}

// static
v8::Handle<v8::Value> SearchBoxExtensionWrapper::NavigateContentWindow(
    const v8::Arguments& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view || !args.Length()) return v8::Undefined();

  DVLOG(1) << render_view << " NavigateContentWindow; query="
           << SearchBox::Get(render_view)->query();

  // If the query is blank and verbatim is false, this must be the NTP. If the
  // user were clicking on an autocomplete suggestion, either the query would
  // be non-blank, or it would be blank due to SetQueryFromAutocompleteResult()
  // but verbatim would be true.
  if (SearchBox::Get(render_view)->query().empty() &&
      !SearchBox::Get(render_view)->verbatim())
    return NavigateNewTabPage(args);
  return NavigateSearchBox(args);
}

// static
v8::Handle<v8::Value> SearchBoxExtensionWrapper::SetSuggestions(
    const v8::Arguments& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view || !args.Length()) return v8::Undefined();
  SearchBox* search_box = SearchBox::Get(render_view);

  DVLOG(1) << render_view << " SetSuggestions";
  v8::Handle<v8::Object> suggestion_json = args[0]->ToObject();

  InstantCompleteBehavior behavior = INSTANT_COMPLETE_NOW;
  InstantSuggestionType type = INSTANT_SUGGESTION_SEARCH;
  v8::Handle<v8::Value> complete_value =
      suggestion_json->Get(v8::String::New("complete_behavior"));
  if (complete_value->Equals(v8::String::New("now"))) {
    behavior = INSTANT_COMPLETE_NOW;
  } else if (complete_value->Equals(v8::String::New("never"))) {
    behavior = INSTANT_COMPLETE_NEVER;
  } else if (complete_value->Equals(v8::String::New("replace"))) {
    behavior = INSTANT_COMPLETE_REPLACE;
  }

  std::vector<InstantSuggestion> suggestions;

  v8::Handle<v8::Value> suggestions_field =
      suggestion_json->Get(v8::String::New("suggestions"));
  if (suggestions_field->IsArray()) {
    v8::Handle<v8::Array> suggestions_array = suggestions_field.As<v8::Array>();
    for (size_t i = 0; i < suggestions_array->Length(); i++) {
      string16 text = V8ValueToUTF16(
          suggestions_array->Get(i)->ToObject()->Get(v8::String::New("value")));
      suggestions.push_back(
          InstantSuggestion(text, behavior, type, search_box->query()));
    }
  }

  search_box->SetSuggestions(suggestions);

  return v8::Undefined();
}

// static
v8::Handle<v8::Value> SearchBoxExtensionWrapper::SetSuggestion(
    const v8::Arguments& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view || args.Length() < 2) return v8::Undefined();

  DVLOG(1) << render_view << " SetSuggestion";
  string16 text = V8ValueToUTF16(args[0]);
  InstantCompleteBehavior behavior = INSTANT_COMPLETE_NOW;
  InstantSuggestionType type = INSTANT_SUGGESTION_URL;

  if (args[1]->Uint32Value() == 2) {
    behavior = INSTANT_COMPLETE_NEVER;
    type = INSTANT_SUGGESTION_SEARCH;
  }

  SearchBox* search_box = SearchBox::Get(render_view);
  std::vector<InstantSuggestion> suggestions;
  suggestions.push_back(
      InstantSuggestion(text, behavior, type, search_box->query()));
  search_box->SetSuggestions(suggestions);

  return v8::Undefined();
}

// static
v8::Handle<v8::Value>
    SearchBoxExtensionWrapper::SetSuggestionFromAutocompleteResult(
        const v8::Arguments& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view || !args.Length()) return v8::Undefined();

  DVLOG(1) << render_view << " SetSuggestionFromAutocompleteResult";
  InstantAutocompleteResult result;
  if (!SearchBox::Get(render_view)->GetAutocompleteResultWithID(
          args[0]->IntegerValue(), &result)) {
    return v8::Undefined();
  }

  // We only support selecting autocomplete results that are URLs.
  string16 text = result.destination_url;
  InstantCompleteBehavior behavior = INSTANT_COMPLETE_NOW;
  InstantSuggestionType type = INSTANT_SUGGESTION_URL;

  SearchBox* search_box = SearchBox::Get(render_view);
  std::vector<InstantSuggestion> suggestions;
  suggestions.push_back(
      InstantSuggestion(text, behavior, type, search_box->query()));
  search_box->SetSuggestions(suggestions);

  return v8::Undefined();
}

// static
v8::Handle<v8::Value> SearchBoxExtensionWrapper::SetQuery(
    const v8::Arguments& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view || args.Length() < 2) return v8::Undefined();

  DVLOG(1) << render_view << " SetQuery";
  string16 text = V8ValueToUTF16(args[0]);
  InstantCompleteBehavior behavior = INSTANT_COMPLETE_REPLACE;
  InstantSuggestionType type = INSTANT_SUGGESTION_SEARCH;

  if (args[1]->Uint32Value() == 1)
    type = INSTANT_SUGGESTION_URL;

  SearchBox* search_box = SearchBox::Get(render_view);
  std::vector<InstantSuggestion> suggestions;
  suggestions.push_back(
      InstantSuggestion(text, behavior, type, search_box->query()));
  search_box->SetSuggestions(suggestions);

  return v8::Undefined();
}

v8::Handle<v8::Value>
    SearchBoxExtensionWrapper::SetQueryFromAutocompleteResult(
        const v8::Arguments& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view || !args.Length()) return v8::Undefined();

  DVLOG(1) << render_view << " SetQueryFromAutocompleteResult";
  InstantAutocompleteResult result;
  if (!SearchBox::Get(render_view)->GetAutocompleteResultWithID(
          args[0]->IntegerValue(), &result)) {
    return v8::Undefined();
  }

  SearchBox* search_box = SearchBox::Get(render_view);
  std::vector<InstantSuggestion> suggestions;
  if (result.search_query.empty()) {
    // TODO(jered): Distinguish between history URLs and search provider
    // navsuggest URLs so that we can do proper accounting on history URLs.
    suggestions.push_back(
        InstantSuggestion(result.destination_url,
                          INSTANT_COMPLETE_REPLACE,
                          INSTANT_SUGGESTION_URL,
                          search_box->query()));
  } else {
    suggestions.push_back(
        InstantSuggestion(result.search_query,
                          INSTANT_COMPLETE_REPLACE,
                          INSTANT_SUGGESTION_SEARCH,
                          string16()));
  }

  search_box->SetSuggestions(suggestions);
  // Clear the SearchBox's query text explicitly since this is a restricted
  // value.
  search_box->ClearQuery();

  return v8::Undefined();
}

// static
v8::Handle<v8::Value> SearchBoxExtensionWrapper::ShowOverlay(
    const v8::Arguments& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view || args.Length() < 1) return v8::Undefined();

  DVLOG(1) << render_view << " ShowOverlay";

  int height = 100;
  InstantSizeUnits units = INSTANT_SIZE_PERCENT;
  if (args[0]->IsInt32()) {
    height = args[0]->Int32Value();
    units = INSTANT_SIZE_PIXELS;
  }
  SearchBox::Get(render_view)->ShowInstantOverlay(height, units);

  return v8::Undefined();
}

// static
v8::Handle<v8::Value> SearchBoxExtensionWrapper::GetMostVisitedItems(
    const v8::Arguments& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view)
    return v8::Undefined();
  DVLOG(1) << render_view << " GetMostVisitedItems";

  const SearchBox* search_box = SearchBox::Get(render_view);

  std::vector<InstantMostVisitedItemIDPair> instant_mv_items;
  search_box->GetMostVisitedItems(&instant_mv_items);
  v8::Handle<v8::Array> v8_mv_items = v8::Array::New(instant_mv_items.size());
  for (size_t i = 0; i < instant_mv_items.size(); ++i) {
    // We set the "dir" attribute of the title, so that in RTL locales, a LTR
    // title is rendered left-to-right and truncated from the right. For
    // example, the title of http://msdn.microsoft.com/en-us/default.aspx is
    // "MSDN: Microsoft developer network". In RTL locales, in the New Tab
    // page, if the "dir" of this title is not specified, it takes Chrome UI's
    // directionality. So the title will be truncated as "soft developer
    // network". Setting the "dir" attribute as "ltr" renders the truncated
    // title as "MSDN: Microsoft D...". As another example, the title of
    // http://yahoo.com is "Yahoo!". In RTL locales, in the New Tab page, the
    // title will be rendered as "!Yahoo" if its "dir" attribute is not set to
    // "ltr".
    const InstantMostVisitedItem& mv_item = instant_mv_items[i].second;
    std::string direction;
    if (base::i18n::StringContainsStrongRTLChars(mv_item.title))
      direction = kRTLHtmlTextDirection;
    else
      direction = kLTRHtmlTextDirection;

    string16 title = mv_item.title;
    if (title.empty())
      title = UTF8ToUTF16(mv_item.url.spec());

    InstantRestrictedID restricted_id = instant_mv_items[i].first;
    v8::Handle<v8::Object> item = v8::Object::New();
    item->Set(v8::String::New("rid"), v8::Int32::New(restricted_id));
    item->Set(v8::String::New("thumbnailUrl"),
              GenerateThumbnailURL(restricted_id));
    item->Set(v8::String::New("faviconUrl"),
              GenerateFaviconURL(restricted_id));
    item->Set(v8::String::New("title"), UTF16ToV8String(title));
    item->Set(v8::String::New("domain"), UTF8ToV8String(mv_item.url.host()));
    item->Set(v8::String::New("direction"), UTF8ToV8String(direction));

    v8_mv_items->Set(i, item);
  }
  return v8_mv_items;
}

// static
v8::Handle<v8::Value> SearchBoxExtensionWrapper::DeleteMostVisitedItem(
    const v8::Arguments& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view || !args.Length()) return v8::Undefined();

  DVLOG(1) << render_view << " DeleteMostVisitedItem";
  SearchBox::Get(render_view)->DeleteMostVisitedItem(args[0]->IntegerValue());
  return v8::Undefined();
}

// static
v8::Handle<v8::Value> SearchBoxExtensionWrapper::UndoMostVisitedDeletion(
    const v8::Arguments& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view || !args.Length()) return v8::Undefined();

  DVLOG(1) << render_view << " UndoMostVisitedDeletion";
  SearchBox::Get(render_view)->UndoMostVisitedDeletion(args[0]->IntegerValue());
  return v8::Undefined();
}

// static
v8::Handle<v8::Value> SearchBoxExtensionWrapper::UndoAllMostVisitedDeletions(
    const v8::Arguments& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view) return v8::Undefined();

  DVLOG(1) << render_view << " UndoAllMostVisitedDeletions";
  SearchBox::Get(render_view)->UndoAllMostVisitedDeletions();
  return v8::Undefined();
}

// static
v8::Handle<v8::Value> SearchBoxExtensionWrapper::FocusOmnibox(
    const v8::Arguments& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view) return v8::Undefined();

  DVLOG(1) << render_view << " FocusOmnibox";
  SearchBox::Get(render_view)->FocusOmnibox();

  return v8::Undefined();
}

// static
v8::Handle<v8::Value> SearchBoxExtensionWrapper::StartCapturingKeyStrokes(
    const v8::Arguments& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view) return v8::Undefined();

  DVLOG(1) << render_view << " StartCapturingKeyStrokes";
  SearchBox::Get(render_view)->StartCapturingKeyStrokes();
  return v8::Undefined();
}

// static
v8::Handle<v8::Value> SearchBoxExtensionWrapper::StopCapturingKeyStrokes(
    const v8::Arguments& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view) return v8::Undefined();

  DVLOG(1) << render_view << " StopCapturingKeyStrokes";
  SearchBox::Get(render_view)->StopCapturingKeyStrokes();
  return v8::Undefined();
}

// static
v8::Extension* SearchBoxExtension::Get() {
  return new SearchBoxExtensionWrapper(ResourceBundle::GetSharedInstance().
      GetRawDataResource(IDR_SEARCHBOX_API));
}

// static
v8::Handle<v8::Value> SearchBoxExtensionWrapper::SetSuggestionStyle(
    const v8::Arguments& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view || args.Length() < 2 || !args[0]->IsNumber() ||
      !args[1]->IsNumber()) {
    return v8::Undefined();
  }

  DVLOG(1) << render_view << " SetSuggestionStyle";

  InstantAutocompleteResultStyle style;
  style.url_color = args[0]->IntegerValue();
  style.title_color = args[1]->IntegerValue();
  SearchBox::Get(render_view)->SetInstantAutocompleteResultStyle(style);

  return v8::Undefined();
}

// static
bool SearchBoxExtension::PageSupportsInstant(WebKit::WebFrame* frame) {
  if (!frame) return false;
  v8::HandleScope handle_scope;
  v8::Handle<v8::Value> v = frame->executeScriptAndReturnValue(
      WebKit::WebScriptSource(kSupportsInstantScript));
  return !v.IsEmpty() && v->BooleanValue();
}

v8::Handle<v8::Value> SearchBoxExtensionWrapper::ShowBars(
    const v8::Arguments& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view) return v8::Undefined();

  DVLOG(1) << render_view << " ShowBars";
  SearchBox::Get(render_view)->ShowBars();
  return v8::Undefined();
}

// static
v8::Handle<v8::Value> SearchBoxExtensionWrapper::HideBars(
    const v8::Arguments& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view) return v8::Undefined();

  DVLOG(1) << render_view << " HideBars";
  SearchBox::Get(render_view)->HideBars();
  return v8::Undefined();
}

// static
v8::Handle<v8::Value> SearchBoxExtensionWrapper::ShouldUseIframes(
    const v8::Arguments& args) {
  DVLOG(1) << "ShouldUseIframes";
  return v8::Boolean::New(SearchBox::ShouldUseIframes());
}

// static
void SearchBoxExtension::DispatchChange(WebKit::WebFrame* frame) {
  Dispatch(frame, kDispatchChangeEventScript);
}

// static
void SearchBoxExtension::DispatchSubmit(WebKit::WebFrame* frame) {
  Dispatch(frame, kDispatchSubmitEventScript);
}

// static
void SearchBoxExtension::DispatchCancel(WebKit::WebFrame* frame) {
  Dispatch(frame, kDispatchCancelEventScript);
}

// static
void SearchBoxExtension::DispatchResize(WebKit::WebFrame* frame) {
  Dispatch(frame, kDispatchResizeEventScript);
}

// static
void SearchBoxExtension::DispatchOnWindowReady(WebKit::WebFrame* frame) {
  Dispatch(frame, kDispatchOnWindowReady);
}

// static
void SearchBoxExtension::DispatchAutocompleteResults(WebKit::WebFrame* frame) {
  Dispatch(frame, kDispatchAutocompleteResultsEventScript);
}

// static
void SearchBoxExtension::DispatchUpOrDownKeyPress(WebKit::WebFrame* frame,
                                                  int count) {
  Dispatch(frame, WebKit::WebString::fromUTF8(
      base::StringPrintf(kDispatchUpOrDownKeyPressEventScript, abs(count),
                         count < 0 ? ui::VKEY_UP : ui::VKEY_DOWN)));
}

// static
void SearchBoxExtension::DispatchEscKeyPress(WebKit::WebFrame* frame) {
  Dispatch(frame, WebKit::WebString::fromUTF8(
      base::StringPrintf(kDispatchEscKeyPressEventScript, ui::VKEY_ESCAPE)));
}

// static
void SearchBoxExtension::DispatchKeyCaptureChange(WebKit::WebFrame* frame) {
  Dispatch(frame, kDispatchKeyCaptureChangeScript);
}

// static
void SearchBoxExtension::DispatchMarginChange(WebKit::WebFrame* frame) {
  Dispatch(frame, kDispatchMarginChangeEventScript);
}

// static
void SearchBoxExtension::DispatchThemeChange(WebKit::WebFrame* frame) {
  Dispatch(frame, kDispatchThemeChangeEventScript);
}

// static
void SearchBoxExtension::DispatchMostVisitedChanged(
    WebKit::WebFrame* frame) {
  Dispatch(frame, kDispatchMostVisitedChangedScript);
}

void SearchBoxExtension::DispatchBarsHidden(WebKit::WebFrame* frame) {
  Dispatch(frame, kDispatchBarsHiddenEventScript);
}

}  // namespace extensions_v8
