// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/searchbox/searchbox_extension.h"

#include "base/i18n/rtl.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/autocomplete_match_type.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/instant_types.h"
#include "chrome/common/url_constants.h"
#include "chrome/renderer/searchbox/searchbox.h"
#include "content/public/renderer/render_view.h"
#include "googleurl/src/gurl.h"
#include "grit/renderer_resources.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebScriptSource.h"
#include "third_party/WebKit/public/web/WebView.h"
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

const char kThemeAttributionFormat[] =
    "chrome-search://theme/IDR_THEME_NTP_ATTRIBUTION?%s";

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

v8::Handle<v8::String> GenerateThumbnailURL(
    int render_view_id,
    InstantRestrictedID most_visited_item_id) {
  return UTF8ToV8String(base::StringPrintf("chrome-search://thumb/%d/%d",
                                           render_view_id,
                                           most_visited_item_id));
}

v8::Handle<v8::String> GenerateFaviconURL(
    int render_view_id,
    InstantRestrictedID most_visited_item_id) {
  return UTF8ToV8String(base::StringPrintf("chrome-search://favicon/%d/%d",
                                           render_view_id,
                                           most_visited_item_id));
}

// If |url| starts with |prefix|, removes |prefix|.
void StripPrefix(string16* url, const string16& prefix) {
  if (StartsWith(*url, prefix, true))
    url->erase(0, prefix.length());
}

// Removes leading "http://" or "http://www." from |url| unless |user_input|
// starts with those prefixes.
void StripURLPrefixes(string16* url, const string16& user_input) {
  string16 trimmed_user_input;
  TrimWhitespace(user_input, TRIM_TRAILING, &trimmed_user_input);
  if (StartsWith(*url, trimmed_user_input, true))
    return;

  StripPrefix(url, ASCIIToUTF16(chrome::kHttpScheme) + ASCIIToUTF16("://"));
  if (StartsWith(*url, trimmed_user_input, true))
    return;

  StripPrefix(url, ASCIIToUTF16("www."));
}

// Formats a URL for display to the user. Strips out prefixes like whitespace,
// "http://" and "www." unless the user input (|query|) matches the prefix.
// Also removes trailing whitespaces and "/" unless the user input matches the
// trailing "/".
void FormatURLForDisplay(string16* url, const string16& query) {
  StripURLPrefixes(url, query);

  string16 trimmed_user_input;
  TrimWhitespace(query, TRIM_LEADING, &trimmed_user_input);
  if (EndsWith(*url, trimmed_user_input, true))
    return;

  // Strip a lone trailing slash.
  if (EndsWith(*url, ASCIIToUTF16("/"), true))
    url->erase(url->length() - 1, 1);
}

// Populates a Javascript NativeSuggestions object from |result|.
// NOTE: Includes properties like "contents" which should be erased before the
// suggestion is returned to the Instant page.
v8::Handle<v8::Object> GenerateNativeSuggestion(
    const string16& query,
    InstantRestrictedID restricted_id,
    const InstantAutocompleteResult& result) {
  v8::Handle<v8::Object> obj = v8::Object::New();
  obj->Set(v8::String::New("provider"), UTF16ToV8String(result.provider));
  obj->Set(v8::String::New("type"),
           UTF8ToV8String(AutocompleteMatchType::ToString(result.type)));
  obj->Set(v8::String::New("description"), UTF16ToV8String(result.description));
  obj->Set(v8::String::New("destination_url"),
           UTF16ToV8String(result.destination_url));
  if (result.search_query.empty()) {
    string16 url = result.destination_url;
    FormatURLForDisplay(&url, query);
    obj->Set(v8::String::New("contents"), UTF16ToV8String(url));
  } else {
    obj->Set(v8::String::New("contents"), UTF16ToV8String(result.search_query));
    obj->Set(v8::String::New("is_search"), v8::Boolean::New(true));
  }
  obj->Set(v8::String::New("rid"), v8::Uint32::New(restricted_id));

  v8::Handle<v8::Object> ranking_data = v8::Object::New();
  ranking_data->Set(v8::String::New("relevance"),
                    v8::Int32::New(result.relevance));
  obj->Set(v8::String::New("rankingData"), ranking_data);
  return obj;
}

// Populates a Javascript MostVisitedItem object from |mv_item|.
// NOTE: Includes "url", "title" and "domain" which are private data, so should
// not be returned to the Instant page. These should be erased before returning
// the object. See GetMostVisitedItemsWrapper() in searchbox_api.js.
v8::Handle<v8::Object> GenerateMostVisitedItem(
    int render_view_id,
    InstantRestrictedID restricted_id,
    const InstantMostVisitedItem &mv_item) {
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
  std::string direction;
  if (base::i18n::StringContainsStrongRTLChars(mv_item.title))
    direction = kRTLHtmlTextDirection;
  else
    direction = kLTRHtmlTextDirection;

  string16 title = mv_item.title;
  if (title.empty())
    title = UTF8ToUTF16(mv_item.url.spec());

  v8::Handle<v8::Object> obj = v8::Object::New();
  obj->Set(v8::String::New("rid"), v8::Int32::New(restricted_id));
  obj->Set(v8::String::New("thumbnailUrl"),
           GenerateThumbnailURL(render_view_id, restricted_id));
  obj->Set(v8::String::New("faviconUrl"),
           GenerateFaviconURL(render_view_id, restricted_id));
  obj->Set(v8::String::New("title"), UTF16ToV8String(title));
  obj->Set(v8::String::New("domain"), UTF8ToV8String(mv_item.url.host()));
  obj->Set(v8::String::New("direction"), UTF8ToV8String(direction));
  obj->Set(v8::String::New("url"), UTF8ToV8String(mv_item.url.spec()));
  return obj;
}

// Returns the render view for the current JS context if it matches |origin|,
// otherwise returns NULL. Used to restrict methods that access suggestions and
// most visited data to pages with origin chrome-search://most-visited and
// chrome-search://suggestions.
content::RenderView* GetRenderViewWithCheckedOrigin(const GURL& origin) {
  WebKit::WebFrame* webframe = WebKit::WebFrame::frameForCurrentContext();
  if (!webframe)
    return NULL;
  WebKit::WebView* webview = webframe->view();
  if (!webview)
    return NULL;  // Can happen during closing.
  content::RenderView* render_view = content::RenderView::FromWebView(webview);
  if (!render_view)
    return NULL;

  GURL url(webframe->document().url());
  if (url.GetOrigin() != origin.GetOrigin())
    return NULL;

  return render_view;
}

// Returns the current URL.
GURL GetCurrentURL(content::RenderView* render_view) {
  WebKit::WebView* webview = render_view->GetWebView();
  return webview ? GURL(webview->mainFrame()->document().url()) : GURL();
}

}  // namespace

namespace internal {  // for testing.

// Returns whether or not the user's input string, |query|,  might contain any
// sensitive information, based purely on its value and not where it came from.
// (It may be sensitive for other reasons, like be a URL from the user's
// browsing history.)
bool IsSensitiveInput(const string16& query) {
  const GURL query_as_url(query);
  if (query_as_url.is_valid()) {
    // The input can be interpreted as a URL.  Check to see if it is potentially
    // sensitive.  (Code shamelessly copied from search_provider.cc's
    // IsQuerySuitableForSuggest function.)

    // First we check the scheme: if this looks like a URL with a scheme that is
    // file, we shouldn't send it.  Sending such things is a waste of time and a
    // disclosure of potentially private, local data.  If the scheme is OK, we
    // still need to check other cases below.
    if (LowerCaseEqualsASCII(query_as_url.scheme(), chrome::kFileScheme))
      return true;

    // Don't send URLs with usernames, queries or refs.  Some of these are
    // private, and the Suggest server is unlikely to have any useful results
    // for any of them.  Also don't send URLs with ports, as we may initially
    // think that a username + password is a host + port (and we don't want to
    // send usernames/passwords), and even if the port really is a port, the
    // server is once again unlikely to have and useful results.
    if (!query_as_url.username().empty() ||
        !query_as_url.port().empty() ||
        !query_as_url.query().empty() || !query_as_url.ref().empty())
      return true;

    // Don't send anything for https except the hostname.  Hostnames are OK
    // because they are visible when the TCP connection is established, but the
    // specific path may reveal private information.
    if (LowerCaseEqualsASCII(query_as_url.scheme(), chrome::kHttpsScheme) &&
        !query_as_url.path().empty() && query_as_url.path() != "/")
      return true;
  }
  return false;
}

// Resolves a possibly relative URL using the current URL.
GURL ResolveURL(const GURL& current_url,
                const string16& possibly_relative_url) {
  if (current_url.is_valid() && !possibly_relative_url.empty())
    return current_url.Resolve(possibly_relative_url);
  return GURL(possibly_relative_url);
}

}  // namespace internal

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
    "    window.chrome.embeddedSearch &&"
    "    window.chrome.embeddedSearch.searchBox &&"
    "    window.chrome.embeddedSearch.searchBox.onbarshidden &&"
    "    typeof window.chrome.embeddedSearch.searchBox.onbarshidden =="
    "         'function') {"
    "  window.chrome.embeddedSearch.searchBox.onbarshidden();"
    "  true;"
    "}";

static const char kDispatchFocusChangedScript[] =
    "if (window.chrome &&"
    "    window.chrome.embeddedSearch &&"
    "    window.chrome.embeddedSearch.searchBox &&"
    "    window.chrome.embeddedSearch.searchBox.onfocuschange &&"
    "    typeof window.chrome.embeddedSearch.searchBox.onfocuschange =="
    "         'function') {"
    "  window.chrome.embeddedSearch.searchBox.onfocuschange();"
    "  true;"
    "}";

static const char kDispatchInputStartScript[] =
    "if (window.chrome &&"
    "    window.chrome.embeddedSearch &&"
    "    window.chrome.embeddedSearch.newTabPage &&"
    "    window.chrome.embeddedSearch.newTabPage.oninputstart &&"
    "    typeof window.chrome.embeddedSearch.newTabPage.oninputstart =="
    "         'function') {"
    "  window.chrome.embeddedSearch.newTabPage.oninputstart();"
    "  true;"
    "}";

static const char kDispatchInputCancelScript[] =
    "if (window.chrome &&"
    "    window.chrome.embeddedSearch &&"
    "    window.chrome.embeddedSearch.newTabPage &&"
    "    window.chrome.embeddedSearch.newTabPage.oninputcancel &&"
    "    typeof window.chrome.embeddedSearch.newTabPage.oninputcancel =="
    "         'function') {"
    "  window.chrome.embeddedSearch.newTabPage.oninputcancel();"
    "  true;"
    "}";

static const char kDispatchToggleVoiceSearchScript[] =
    "if (window.chrome &&"
    "    window.chrome.embeddedSearch &&"
    "    window.chrome.embeddedSearch.searchBox &&"
    "    window.chrome.embeddedSearch.searchBox.ontogglevoicesearch &&"
    "    typeof window.chrome.embeddedSearch.searchBox.ontogglevoicesearch =="
    "         'function') {"
    "  window.chrome.embeddedSearch.searchBox.ontogglevoicesearch();"
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
  static void DeleteMostVisitedItem(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  // Gets the value of the user's search query.
  static void GetQuery(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Gets whether the |value| should be considered final -- as opposed to a
  // partial match. This may be set if the user clicks a suggestion, presses
  // forward delete, or in other cases where Chrome overrides.
  static void GetVerbatim(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Gets the start of the selection in the search box.
  static void GetSelectionStart(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  // Gets the end of the selection in the search box.
  static void GetSelectionEnd(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Gets the x coordinate (relative to |window|) of the left edge of the
  // region of the search box that overlaps the window.
  static void GetX(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Gets the y coordinate (relative to |window|) of the right edge of the
  // region of the search box that overlaps the window.
  static void GetY(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Gets the width of the region of the search box that overlaps the window,
  // i.e., the width of the omnibox.
  static void GetWidth(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Gets the height of the region of the search box that overlaps the window.
  static void GetHeight(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Gets Most Visited Items.
  static void GetMostVisitedItems(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  // Gets the start-edge margin to use with extended Instant.
  static void GetStartMargin(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Returns true if the Searchbox itself is oriented right-to-left.
  static void GetRightToLeft(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Gets the autocomplete results from search box.
  static void GetAutocompleteResults(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  // Gets whether to display Instant results.
  static void GetDisplayInstantResults(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  // Gets the background info of the theme currently adopted by browser.
  // Call only when overlay is showing NTP page.
  static void GetThemeBackgroundInfo(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  // Gets whether the browser is capturing key strokes.
  static void IsKeyCaptureEnabled(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  // Gets the font family of the text in the omnibox.
  static void GetFont(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Gets the font size of the text in the omnibox.
  static void GetFontSize(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Gets whether or not the app launcher is enabled.
  static void GetAppLauncherEnabled(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  // Navigates the window to a URL represented by either a URL string or a
  // restricted ID. The two variants handle restricted IDs in their
  // respective namespaces.
  static void NavigateSearchBox(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void NavigateNewTabPage(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  // DEPRECATED: TODO(sreeram): Remove when google.com no longer uses this.
  static void NavigateContentWindow(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  // Sets ordered suggestions. Valid for current |value|.
  static void SetSuggestions(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Sets the text to be autocompleted into the search box.
  static void SetSuggestion(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Like SetSuggestion() but uses a restricted autocomplete result ID to
  // identify the text.
  static void SetSuggestionFromAutocompleteResult(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  // Sets the search box text, completely replacing what the user typed.
  static void SetQuery(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Like |SetQuery| but uses a restricted autocomplete result ID to identify
  // the text.
  static void SetQueryFromAutocompleteResult(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  // Indicates whether the page supports voice search.
  static void SetVoiceSearchSupported(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  // Requests the overlay be shown with the specified contents and height.
  static void ShowOverlay(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Sets the focus to the omnibox.
  static void FocusOmnibox(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Start capturing user key strokes.
  static void StartCapturingKeyStrokes(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  // Stop capturing user key strokes.
  static void StopCapturingKeyStrokes(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  // Undoes the deletion of all Most Visited itens.
  static void UndoAllMostVisitedDeletions(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  // Undoes the deletion of a Most Visited item.
  static void UndoMostVisitedDeletion(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  // Shows any attached bars.
  static void ShowBars(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Hides any attached bars.  When the bars are hidden, the "onbarshidden"
  // event is fired to notify the page.
  static void HideBars(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Gets the raw data for a suggestion including its content.
  // GetRenderViewWithCheckedOrigin() enforces that only code in the origin
  // chrome-search://suggestion can call this function.
  static void GetSuggestionData(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  // Gets the raw data for a most visited item including its raw URL.
  // GetRenderViewWithCheckedOrigin() enforces that only code in the origin
  // chrome-search://most-visited can call this function.
  static void GetMostVisitedItemData(
    const v8::FunctionCallbackInfo<v8::Value>& args);

  // Gets whether the omnibox has focus or not.
  static void IsFocused(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Gets whether user input is in progress.
  static void IsInputInProgress(
      const v8::FunctionCallbackInfo<v8::Value>& args);

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
  if (name->Equals(v8::String::New("GetAppLauncherEnabled")))
    return v8::FunctionTemplate::New(GetAppLauncherEnabled);
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
  if (name->Equals(v8::String::New("SetVoiceSearchSupported")))
    return v8::FunctionTemplate::New(SetVoiceSearchSupported);
  if (name->Equals(v8::String::New("ShowOverlay")))
    return v8::FunctionTemplate::New(ShowOverlay);
  if (name->Equals(v8::String::New("FocusOmnibox")))
    return v8::FunctionTemplate::New(FocusOmnibox);
  if (name->Equals(v8::String::New("StartCapturingKeyStrokes")))
    return v8::FunctionTemplate::New(StartCapturingKeyStrokes);
  if (name->Equals(v8::String::New("StopCapturingKeyStrokes")))
    return v8::FunctionTemplate::New(StopCapturingKeyStrokes);
  if (name->Equals(v8::String::New("UndoAllMostVisitedDeletions")))
    return v8::FunctionTemplate::New(UndoAllMostVisitedDeletions);
  if (name->Equals(v8::String::New("UndoMostVisitedDeletion")))
    return v8::FunctionTemplate::New(UndoMostVisitedDeletion);
  if (name->Equals(v8::String::New("ShowBars")))
    return v8::FunctionTemplate::New(ShowBars);
  if (name->Equals(v8::String::New("HideBars")))
    return v8::FunctionTemplate::New(HideBars);
  if (name->Equals(v8::String::New("GetSuggestionData")))
    return v8::FunctionTemplate::New(GetSuggestionData);
  if (name->Equals(v8::String::New("GetMostVisitedItemData")))
    return v8::FunctionTemplate::New(GetMostVisitedItemData);
  if (name->Equals(v8::String::New("IsFocused")))
    return v8::FunctionTemplate::New(IsFocused);
  if (name->Equals(v8::String::New("IsInputInProgress")))
    return v8::FunctionTemplate::New(IsInputInProgress);
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
void SearchBoxExtensionWrapper::GetQuery(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view) return;
  DVLOG(1) << render_view << " GetQuery request";

  if (SearchBox::Get(render_view)->query_is_restricted()) {
    DVLOG(1) << render_view << " Query text marked as restricted.";
    return;
  }

  const string16& query = SearchBox::Get(render_view)->query();
  if (internal::IsSensitiveInput(query)) {
    DVLOG(1) << render_view << " Query text is sensitive.";
    return;
  }

  DVLOG(1) << render_view << " GetQuery: '" << query << "'";
  args.GetReturnValue().Set(UTF16ToV8String(query));
}

// static
void SearchBoxExtensionWrapper::GetVerbatim(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view) return;

  DVLOG(1) << render_view << " GetVerbatim: "
           << SearchBox::Get(render_view)->verbatim();
  args.GetReturnValue().Set(SearchBox::Get(render_view)->verbatim());
}

// static
void SearchBoxExtensionWrapper::GetSelectionStart(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view) return;

  args.GetReturnValue().Set(static_cast<uint32_t>(
      SearchBox::Get(render_view)->selection_start()));
}

// static
void SearchBoxExtensionWrapper::GetSelectionEnd(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view) return;

  args.GetReturnValue().Set(static_cast<uint32_t>(
      SearchBox::Get(render_view)->selection_end()));
}

// static
void SearchBoxExtensionWrapper::GetX(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view) return;

  args.GetReturnValue().Set(static_cast<int32_t>(
      SearchBox::Get(render_view)->GetPopupBounds().x()));
}

// static
void SearchBoxExtensionWrapper::GetY(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view) return;

  args.GetReturnValue().Set(static_cast<int32_t>(
      SearchBox::Get(render_view)->GetPopupBounds().y()));
}

// static
void SearchBoxExtensionWrapper::GetWidth(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view) return;
  args.GetReturnValue().Set(static_cast<int32_t>(
      SearchBox::Get(render_view)->GetPopupBounds().width()));
}

// static
void SearchBoxExtensionWrapper::GetHeight(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view) return;

  args.GetReturnValue().Set(static_cast<int32_t>(
      SearchBox::Get(render_view)->GetPopupBounds().height()));
}

// static
void SearchBoxExtensionWrapper::GetStartMargin(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view) return;
  args.GetReturnValue().Set(static_cast<int32_t>(
      SearchBox::Get(render_view)->GetStartMargin()));
}

// static
void SearchBoxExtensionWrapper::GetRightToLeft(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  args.GetReturnValue().Set(base::i18n::IsRTL());
}

// static
void SearchBoxExtensionWrapper::GetAutocompleteResults(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view) return;

  std::vector<InstantAutocompleteResultIDPair> results;
  SearchBox::Get(render_view)->GetAutocompleteResults(&results);

  DVLOG(1) << render_view << " GetAutocompleteResults: " << results.size();

  v8::Handle<v8::Array> results_array = v8::Array::New(results.size());
  const string16& query = SearchBox::Get(render_view)->query();
  for (size_t i = 0; i < results.size(); ++i) {
    results_array->Set(i, GenerateNativeSuggestion(query,
                                                   results[i].first,
                                                   results[i].second));
  }
  args.GetReturnValue().Set(results_array);
}

// static
void SearchBoxExtensionWrapper::IsKeyCaptureEnabled(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view) return;

  args.GetReturnValue().Set(SearchBox::Get(render_view)->
                            is_key_capture_enabled());
}

// static
void SearchBoxExtensionWrapper::GetDisplayInstantResults(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view) return;

  bool display_instant_results =
      SearchBox::Get(render_view)->display_instant_results();
  DVLOG(1) << render_view << " "
           << "GetDisplayInstantResults: " << display_instant_results;
  args.GetReturnValue().Set(display_instant_results);
}

// static
void SearchBoxExtensionWrapper::GetThemeBackgroundInfo(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view) return;

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
      info->Set(v8::String::New("attributionUrl"), UTF8ToV8String(
          base::StringPrintf(kThemeAttributionFormat,
                             theme_info.theme_id.c_str())));
    }
  }

  args.GetReturnValue().Set(info);
}

// static
void SearchBoxExtensionWrapper::GetFont(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view) return;

  args.GetReturnValue().Set(
      UTF16ToV8String(SearchBox::Get(render_view)->omnibox_font()));
}

// static
void SearchBoxExtensionWrapper::GetFontSize(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view) return;

  args.GetReturnValue().Set(static_cast<uint32_t>(
      SearchBox::Get(render_view)->omnibox_font_size()));
}

// static
void SearchBoxExtensionWrapper::GetAppLauncherEnabled(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view) return;

  args.GetReturnValue().Set(
      SearchBox::Get(render_view)->app_launcher_enabled());
}

// static
void SearchBoxExtensionWrapper::NavigateSearchBox(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view || !args.Length()) return;

  GURL destination_url;
  content::PageTransition transition = content::PAGE_TRANSITION_TYPED;
  bool is_search_type = false;
  if (args[0]->IsNumber()) {
    InstantAutocompleteResult result;
    if (SearchBox::Get(render_view)->GetAutocompleteResultWithID(
            args[0]->IntegerValue(), &result)) {
      destination_url = GURL(result.destination_url);
      transition = result.transition;
      is_search_type = !result.search_query.empty();
    }
  } else {
    // Resolve the URL.
    const string16& possibly_relative_url = V8ValueToUTF16(args[0]);
    GURL current_url = GetCurrentURL(render_view);
    destination_url = internal::ResolveURL(current_url, possibly_relative_url);
  }

  DVLOG(1) << render_view << " NavigateSearchBox: " << destination_url;

  // Navigate the main frame.
  if (destination_url.is_valid()) {
    WindowOpenDisposition disposition = CURRENT_TAB;
    if (args[1]->Uint32Value() == 2)
      disposition = NEW_BACKGROUND_TAB;
    SearchBox::Get(render_view)->NavigateToURL(
        destination_url, transition, disposition, is_search_type);
  }
}

// static
void SearchBoxExtensionWrapper::NavigateNewTabPage(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view || !args.Length()) return;

  GURL destination_url;
  content::PageTransition transition = content::PAGE_TRANSITION_AUTO_BOOKMARK;
  if (args[0]->IsNumber()) {
    InstantMostVisitedItem item;
    if (SearchBox::Get(render_view)->GetMostVisitedItemWithID(
            args[0]->IntegerValue(), &item)) {
      destination_url = item.url;
    }
  } else {
    // Resolve the URL
    const string16& possibly_relative_url = V8ValueToUTF16(args[0]);
    GURL current_url = GetCurrentURL(render_view);
    destination_url = internal::ResolveURL(current_url, possibly_relative_url);
  }

  DVLOG(1) << render_view << " NavigateNewTabPage: " << destination_url;

  // Navigate the main frame.
  if (destination_url.is_valid()) {
    WindowOpenDisposition disposition = CURRENT_TAB;
    if (args[1]->Uint32Value() == 2)
      disposition = NEW_BACKGROUND_TAB;
    SearchBox::Get(render_view)->NavigateToURL(
        destination_url, transition, disposition, false);
  }
}

// static
void SearchBoxExtensionWrapper::NavigateContentWindow(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view || !args.Length()) return;

  DVLOG(1) << render_view << " NavigateContentWindow; query="
           << SearchBox::Get(render_view)->query();

  // If the query is blank and verbatim is false, this must be the NTP. If the
  // user were clicking on an autocomplete suggestion, either the query would
  // be non-blank, or it would be blank due to SetQueryFromAutocompleteResult()
  // but verbatim would be true.
  if (SearchBox::Get(render_view)->query().empty() &&
      !SearchBox::Get(render_view)->verbatim()) {
    NavigateNewTabPage(args);
    return;
  }
  NavigateSearchBox(args);
}

// static
void SearchBoxExtensionWrapper::SetSuggestions(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view || !args.Length()) return;
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
          InstantSuggestion(text, behavior, type, search_box->query(),
                            kNoMatchIndex));
    }
  }

  search_box->SetSuggestions(suggestions);
}

// static
void SearchBoxExtensionWrapper::SetSuggestion(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view || args.Length() < 2) return;

  string16 text = V8ValueToUTF16(args[0]);
  DVLOG(1) << render_view << " SetSuggestion: " << text;

  InstantCompleteBehavior behavior = INSTANT_COMPLETE_NOW;
  InstantSuggestionType type = INSTANT_SUGGESTION_URL;

  if (args[1]->Uint32Value() == 2) {
    behavior = INSTANT_COMPLETE_NEVER;
    type = INSTANT_SUGGESTION_SEARCH;
  }

  SearchBox* search_box = SearchBox::Get(render_view);
  std::vector<InstantSuggestion> suggestions;
  suggestions.push_back(
      InstantSuggestion(text, behavior, type, search_box->query(),
                        kNoMatchIndex));
  search_box->SetSuggestions(suggestions);
}

// static
void SearchBoxExtensionWrapper::SetSuggestionFromAutocompleteResult(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view || !args.Length()) return;

  DVLOG(1) << render_view << " SetSuggestionFromAutocompleteResult";
  InstantAutocompleteResult result;
  if (!SearchBox::Get(render_view)->GetAutocompleteResultWithID(
          args[0]->IntegerValue(), &result)) {
    return;
  }

  // We only support selecting autocomplete results that are URLs.
  string16 text = result.destination_url;
  InstantCompleteBehavior behavior = INSTANT_COMPLETE_NOW;
  InstantSuggestionType type = INSTANT_SUGGESTION_URL;

  SearchBox* search_box = SearchBox::Get(render_view);
  std::vector<InstantSuggestion> suggestions;
  suggestions.push_back(
      InstantSuggestion(text, behavior, type, search_box->query(),
                        kNoMatchIndex));
  search_box->SetSuggestions(suggestions);
}

// static
void SearchBoxExtensionWrapper::SetQuery(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view || args.Length() < 2) return;

  DVLOG(1) << render_view << " SetQuery";
  string16 text = V8ValueToUTF16(args[0]);
  InstantCompleteBehavior behavior = INSTANT_COMPLETE_REPLACE;
  InstantSuggestionType type = INSTANT_SUGGESTION_SEARCH;

  if (args[1]->Uint32Value() == 1)
    type = INSTANT_SUGGESTION_URL;

  SearchBox* search_box = SearchBox::Get(render_view);
  std::vector<InstantSuggestion> suggestions;
  suggestions.push_back(
      InstantSuggestion(text, behavior, type, search_box->query(),
                        kNoMatchIndex));
  search_box->SetSuggestions(suggestions);
}

void SearchBoxExtensionWrapper::SetQueryFromAutocompleteResult(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view || !args.Length()) return;

  DVLOG(1) << render_view << " SetQueryFromAutocompleteResult";
  InstantAutocompleteResult result;
  if (!SearchBox::Get(render_view)->GetAutocompleteResultWithID(
          args[0]->IntegerValue(), &result)) {
    return;
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
                          search_box->query(),
                          result.autocomplete_match_index));
  } else {
    suggestions.push_back(
        InstantSuggestion(result.search_query,
                          INSTANT_COMPLETE_REPLACE,
                          INSTANT_SUGGESTION_SEARCH,
                          string16(),
                          result.autocomplete_match_index));
  }

  search_box->SetSuggestions(suggestions);
  search_box->MarkQueryAsRestricted();
}

// static
void SearchBoxExtensionWrapper::SetVoiceSearchSupported(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view || args.Length() < 1) return;

  DVLOG(1) << render_view << " SetVoiceSearchSupported";
  SearchBox::Get(render_view)->SetVoiceSearchSupported(args[0]->BooleanValue());
}

// static
void SearchBoxExtensionWrapper::ShowOverlay(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view || args.Length() < 1) return;

  int height = 100;
  InstantSizeUnits units = INSTANT_SIZE_PERCENT;
  if (args[0]->IsInt32()) {
    height = args[0]->Int32Value();
    units = INSTANT_SIZE_PIXELS;
  }
  DVLOG(1) << render_view << " ShowOverlay: " << height << "/" << units;

  SearchBox::Get(render_view)->ShowInstantOverlay(height, units);
}

// static
void SearchBoxExtensionWrapper::GetMostVisitedItems(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view)
    return;
  DVLOG(1) << render_view << " GetMostVisitedItems";

  const SearchBox* search_box = SearchBox::Get(render_view);

  std::vector<InstantMostVisitedItemIDPair> instant_mv_items;
  search_box->GetMostVisitedItems(&instant_mv_items);
  v8::Handle<v8::Array> v8_mv_items = v8::Array::New(instant_mv_items.size());
  for (size_t i = 0; i < instant_mv_items.size(); ++i) {
    v8_mv_items->Set(i, GenerateMostVisitedItem(render_view->GetRoutingID(),
                                                instant_mv_items[i].first,
                                                instant_mv_items[i].second));
  }
  args.GetReturnValue().Set(v8_mv_items);
}

// static
void SearchBoxExtensionWrapper::DeleteMostVisitedItem(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view || !args.Length()) return;

  DVLOG(1) << render_view << " DeleteMostVisitedItem";
  SearchBox::Get(render_view)->DeleteMostVisitedItem(args[0]->IntegerValue());
}

// static
void SearchBoxExtensionWrapper::UndoMostVisitedDeletion(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view || !args.Length()) return;

  DVLOG(1) << render_view << " UndoMostVisitedDeletion";
  SearchBox::Get(render_view)->UndoMostVisitedDeletion(args[0]->IntegerValue());
}

// static
void SearchBoxExtensionWrapper::UndoAllMostVisitedDeletions(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view) return;

  DVLOG(1) << render_view << " UndoAllMostVisitedDeletions";
  SearchBox::Get(render_view)->UndoAllMostVisitedDeletions();
}

// static
void SearchBoxExtensionWrapper::FocusOmnibox(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view) return;

  DVLOG(1) << render_view << " FocusOmnibox";
  SearchBox::Get(render_view)->FocusOmnibox();
}

// static
void SearchBoxExtensionWrapper::StartCapturingKeyStrokes(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view) return;

  DVLOG(1) << render_view << " StartCapturingKeyStrokes";
  SearchBox::Get(render_view)->StartCapturingKeyStrokes();
}

// static
void SearchBoxExtensionWrapper::StopCapturingKeyStrokes(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view) return;

  DVLOG(1) << render_view << " StopCapturingKeyStrokes";
  SearchBox::Get(render_view)->StopCapturingKeyStrokes();
}

// static
v8::Extension* SearchBoxExtension::Get() {
  return new SearchBoxExtensionWrapper(ResourceBundle::GetSharedInstance().
      GetRawDataResource(IDR_SEARCHBOX_API));
}

// static
bool SearchBoxExtension::PageSupportsInstant(WebKit::WebFrame* frame) {
  if (!frame) return false;
  v8::HandleScope handle_scope;
  v8::Handle<v8::Value> v = frame->executeScriptAndReturnValue(
      WebKit::WebScriptSource(kSupportsInstantScript));
  return !v.IsEmpty() && v->BooleanValue();
}

void SearchBoxExtensionWrapper::ShowBars(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view) return;

  DVLOG(1) << render_view << " ShowBars";
  SearchBox::Get(render_view)->ShowBars();
}

// static
void SearchBoxExtensionWrapper::HideBars(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view) return;

  DVLOG(1) << render_view << " HideBars";
  SearchBox::Get(render_view)->HideBars();
}

// static
void SearchBoxExtensionWrapper::GetSuggestionData(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  content::RenderView* render_view = GetRenderViewWithCheckedOrigin(
      GURL(chrome::kChromeSearchSuggestionUrl));
  if (!render_view) return;

  // Need an rid argument.
  if (args.Length() < 1 || !args[0]->IsNumber())
    return;

  DVLOG(1) << render_view << " GetSuggestionData";
  InstantRestrictedID restricted_id = args[0]->IntegerValue();
  InstantAutocompleteResult result;
  if (!SearchBox::Get(render_view)->GetAutocompleteResultWithID(
          restricted_id, &result)) {
    return;
  }
  const string16& query = SearchBox::Get(render_view)->query();
  args.GetReturnValue().Set(
      GenerateNativeSuggestion(query, restricted_id, result));
}

// static
void SearchBoxExtensionWrapper::GetMostVisitedItemData(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  content::RenderView* render_view = GetRenderViewWithCheckedOrigin(
      GURL(chrome::kChromeSearchMostVisitedUrl));
  if (!render_view) return;

  // Need an rid argument.
  if (args.Length() < 1 || !args[0]->IsNumber())
    return;

  DVLOG(1) << render_view << " GetMostVisitedItem";
  InstantRestrictedID restricted_id = args[0]->IntegerValue();
  InstantMostVisitedItem mv_item;
  if (!SearchBox::Get(render_view)->GetMostVisitedItemWithID(
          restricted_id, &mv_item)) {
    return;
  }
  args.GetReturnValue().Set(
      GenerateMostVisitedItem(render_view->GetRoutingID(), restricted_id,
                              mv_item));
}

// static
void SearchBoxExtensionWrapper::IsFocused(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view) return;

  bool is_focused = SearchBox::Get(render_view)->is_focused();
  DVLOG(1) << render_view << " IsFocused: " << is_focused;
  args.GetReturnValue().Set(is_focused);
}

// static
void SearchBoxExtensionWrapper::IsInputInProgress(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view) return;

  bool is_input_in_progress =
      SearchBox::Get(render_view)->is_input_in_progress();
  DVLOG(1) << render_view << " IsInputInProgress: " << is_input_in_progress;
  args.GetReturnValue().Set(is_input_in_progress);
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

// static
void SearchBoxExtension::DispatchBarsHidden(WebKit::WebFrame* frame) {
  Dispatch(frame, kDispatchBarsHiddenEventScript);
}

// static
void SearchBoxExtension::DispatchFocusChange(WebKit::WebFrame* frame) {
  Dispatch(frame, kDispatchFocusChangedScript);
}

// static
void SearchBoxExtension::DispatchInputStart(WebKit::WebFrame* frame) {
  Dispatch(frame, kDispatchInputStartScript);
}

// static
void SearchBoxExtension::DispatchInputCancel(WebKit::WebFrame* frame) {
  Dispatch(frame, kDispatchInputCancelScript);
}

// static
void SearchBoxExtension::DispatchToggleVoiceSearch(
    WebKit::WebFrame* frame) {
  Dispatch(frame, kDispatchToggleVoiceSearchScript);
}

}  // namespace extensions_v8
