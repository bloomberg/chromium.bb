// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/searchbox/searchbox_extension.h"

#include "base/i18n/rtl.h"
#include "base/json/string_escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/instant_types.h"
#include "chrome/common/ntp_logging_events.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/renderer_resources.h"
#include "chrome/renderer/searchbox/searchbox.h"
#include "components/crx_file/id_util.h"
#include "content/public/renderer/render_view.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebScriptSource.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/window_open_disposition.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "url/gurl.h"
#include "url/url_constants.h"
#include "v8/include/v8.h"

namespace {

const char kCSSBackgroundImageFormat[] = "-webkit-image-set("
    "url(chrome-search://theme/IDR_THEME_NTP_BACKGROUND?%s) 1x, "
    "url(chrome-search://theme/IDR_THEME_NTP_BACKGROUND@2x?%s) 2x)";

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

const char kThemeAttributionFormat[] = "-webkit-image-set("
    "url(chrome-search://theme/IDR_THEME_NTP_ATTRIBUTION?%s) 1x, "
    "url(chrome-search://theme/IDR_THEME_NTP_ATTRIBUTION@2x?%s) 2x)";

const char kLTRHtmlTextDirection[] = "ltr";
const char kRTLHtmlTextDirection[] = "rtl";

// Converts a V8 value to a string16.
base::string16 V8ValueToUTF16(v8::Handle<v8::Value> v) {
  v8::String::Value s(v);
  return base::string16(reinterpret_cast<const base::char16*>(*s), s.length());
}

// Converts string16 to V8 String.
v8::Handle<v8::String> UTF16ToV8String(v8::Isolate* isolate,
                                       const base::string16& s) {
  return v8::String::NewFromTwoByte(isolate,
                                    reinterpret_cast<const uint16_t*>(s.data()),
                                    v8::String::kNormalString,
                                    s.size());
}

// Converts std::string to V8 String.
v8::Handle<v8::String> UTF8ToV8String(v8::Isolate* isolate,
                                      const std::string& s) {
  return v8::String::NewFromUtf8(
      isolate, s.data(), v8::String::kNormalString, s.size());
}

void Dispatch(blink::WebFrame* frame, const blink::WebString& script) {
  if (!frame) return;
  frame->executeScript(blink::WebScriptSource(script));
}

v8::Handle<v8::String> GenerateThumbnailURL(
    v8::Isolate* isolate,
    int render_view_id,
    InstantRestrictedID most_visited_item_id) {
  return UTF8ToV8String(
      isolate,
      base::StringPrintf(
          "chrome-search://thumb/%d/%d", render_view_id, most_visited_item_id));
}

// Populates a Javascript MostVisitedItem object from |mv_item|.
// NOTE: Includes "url", "title" and "domain" which are private data, so should
// not be returned to the Instant page. These should be erased before returning
// the object. See GetMostVisitedItemsWrapper() in searchbox_api.js.
v8::Handle<v8::Object> GenerateMostVisitedItem(
    v8::Isolate* isolate,
    int render_view_id,
    InstantRestrictedID restricted_id,
    const InstantMostVisitedItem& mv_item) {
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

  base::string16 title = mv_item.title;
  if (title.empty())
    title = base::UTF8ToUTF16(mv_item.url.spec());

  v8::Handle<v8::Object> obj = v8::Object::New(isolate);
  obj->Set(v8::String::NewFromUtf8(isolate, "renderViewId"),
           v8::Int32::New(isolate, render_view_id));
  obj->Set(v8::String::NewFromUtf8(isolate, "rid"),
           v8::Int32::New(isolate, restricted_id));
  obj->Set(v8::String::NewFromUtf8(isolate, "thumbnailUrl"),
           GenerateThumbnailURL(isolate, render_view_id, restricted_id));
  obj->Set(v8::String::NewFromUtf8(isolate, "title"),
           UTF16ToV8String(isolate, title));
  obj->Set(v8::String::NewFromUtf8(isolate, "domain"),
           UTF8ToV8String(isolate, mv_item.url.host()));
  obj->Set(v8::String::NewFromUtf8(isolate, "direction"),
           UTF8ToV8String(isolate, direction));
  obj->Set(v8::String::NewFromUtf8(isolate, "url"),
           UTF8ToV8String(isolate, mv_item.url.spec()));
  return obj;
}

// Returns the render view for the current JS context if it matches |origin|,
// otherwise returns NULL. Used to restrict methods that access suggestions and
// most visited data to pages with origin chrome-search://most-visited and
// chrome-search://suggestions.
content::RenderView* GetRenderViewWithCheckedOrigin(const GURL& origin) {
  blink::WebLocalFrame* webframe =
      blink::WebLocalFrame::frameForCurrentContext();
  if (!webframe)
    return NULL;
  blink::WebView* webview = webframe->view();
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
  blink::WebView* webview = render_view->GetWebView();
  return webview ? GURL(webview->mainFrame()->document().url()) : GURL();
}

}  // namespace

namespace internal {  // for testing.

// Returns an array with the RGBA color components.
v8::Handle<v8::Value> RGBAColorToArray(v8::Isolate* isolate,
                                       const RGBAColor& color) {
  v8::Handle<v8::Array> color_array = v8::Array::New(isolate, 4);
  color_array->Set(0, v8::Int32::New(isolate, color.r));
  color_array->Set(1, v8::Int32::New(isolate, color.g));
  color_array->Set(2, v8::Int32::New(isolate, color.b));
  color_array->Set(3, v8::Int32::New(isolate, color.a));
  return color_array;
}

// Resolves a possibly relative URL using the current URL.
GURL ResolveURL(const GURL& current_url,
                const base::string16& possibly_relative_url) {
  if (current_url.is_valid() && !possibly_relative_url.empty())
    return current_url.Resolve(possibly_relative_url);
  return GURL(possibly_relative_url);
}

}  // namespace internal

namespace extensions_v8 {

static const char kSearchBoxExtensionName[] = "v8/EmbeddedSearch";

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

static const char kDispatchChromeIdentityCheckResult[] =
    "if (window.chrome &&"
    "    window.chrome.embeddedSearch &&"
    "    window.chrome.embeddedSearch.newTabPage &&"
    "    window.chrome.embeddedSearch.newTabPage.onsignedincheckdone &&"
    "    typeof window.chrome.embeddedSearch.newTabPage"
    "        .onsignedincheckdone === 'function') {"
    "  window.chrome.embeddedSearch.newTabPage.onsignedincheckdone(%s, %s);"
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

static const char kDispatchSuggestionChangeEventScript[] =
    "if (window.chrome &&"
    "    window.chrome.embeddedSearch &&"
    "    window.chrome.embeddedSearch.searchBox &&"
    "    window.chrome.embeddedSearch.searchBox.onsuggestionchange &&"
    "    typeof window.chrome.embeddedSearch.searchBox.onsuggestionchange =="
    "        'function') {"
    "  window.chrome.embeddedSearch.searchBox.onsuggestionchange();"
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
  virtual v8::Handle<v8::FunctionTemplate> GetNativeFunctionTemplate(
      v8::Isolate*,
      v8::Handle<v8::String> name) OVERRIDE;

  // Helper function to find the RenderView. May return NULL.
  static content::RenderView* GetRenderView();

  // Sends a Chrome identity check to the browser.
  static void CheckIsUserSignedInToChromeAs(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  // Deletes a Most Visited item.
  static void DeleteMostVisitedItem(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  // Focuses the omnibox.
  static void Focus(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Gets whether or not the app launcher is enabled.
  static void GetAppLauncherEnabled(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  // Gets the desired navigation behavior from a click event.
  static void GetDispositionFromClick(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  // Gets Most Visited Items.
  static void GetMostVisitedItems(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  // Gets the raw data for a most visited item including its raw URL.
  // GetRenderViewWithCheckedOrigin() enforces that only code in the origin
  // chrome-search://most-visited can call this function.
  static void GetMostVisitedItemData(
    const v8::FunctionCallbackInfo<v8::Value>& args);

  // Gets the submitted value of the user's search query.
  static void GetQuery(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Returns true if the Searchbox itself is oriented right-to-left.
  static void GetRightToLeft(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Gets the start-edge margin to use with extended Instant.
  static void GetStartMargin(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Gets the current top suggestion to prefetch search results.
  static void GetSuggestionToPrefetch(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  // Gets the background info of the theme currently adopted by browser.
  // Call only when overlay is showing NTP page.
  static void GetThemeBackgroundInfo(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  // Gets whether the omnibox has focus or not.
  static void IsFocused(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Gets whether user input is in progress.
  static void IsInputInProgress(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  // Gets whether the browser is capturing key strokes.
  static void IsKeyCaptureEnabled(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  // Logs information from the iframes/titles on the NTP.
  static void LogEvent(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Logs an impression on one of the Most Visited tile on the NTP.
  static void LogMostVisitedImpression(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  // Logs a navigation on one of the Most Visited tile on the NTP.
  static void LogMostVisitedNavigation(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  // Navigates the window to a URL represented by either a URL string or a
  // restricted ID.
  static void NavigateContentWindow(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  // Pastes provided value or clipboard's content into the omnibox.
  static void Paste(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Indicates whether the page supports voice search.
  static void SetVoiceSearchSupported(
      const v8::FunctionCallbackInfo<v8::Value>& args);

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

  // Indicates whether the page supports Instant.
  static void GetDisplayInstantResults(
      const v8::FunctionCallbackInfo<v8::Value>& args);

 private:
  DISALLOW_COPY_AND_ASSIGN(SearchBoxExtensionWrapper);
};

// static
v8::Extension* SearchBoxExtension::Get() {
  return new SearchBoxExtensionWrapper(ResourceBundle::GetSharedInstance().
      GetRawDataResource(IDR_SEARCHBOX_API));
}

// static
bool SearchBoxExtension::PageSupportsInstant(blink::WebFrame* frame) {
  if (!frame) return false;
  v8::HandleScope handle_scope(v8::Isolate::GetCurrent());
  v8::Handle<v8::Value> v = frame->executeScriptAndReturnValue(
      blink::WebScriptSource(kSupportsInstantScript));
  return !v.IsEmpty() && v->BooleanValue();
}

// static
void SearchBoxExtension::DispatchChromeIdentityCheckResult(
    blink::WebFrame* frame,
    const base::string16& identity,
    bool identity_match) {
  std::string escaped_identity = base::GetQuotedJSONString(identity);
  blink::WebString script(base::UTF8ToUTF16(base::StringPrintf(
      kDispatchChromeIdentityCheckResult,
      escaped_identity.c_str(),
      identity_match ? "true" : "false")));
  Dispatch(frame, script);
}

// static
void SearchBoxExtension::DispatchFocusChange(blink::WebFrame* frame) {
  Dispatch(frame, kDispatchFocusChangedScript);
}

// static
void SearchBoxExtension::DispatchInputCancel(blink::WebFrame* frame) {
  Dispatch(frame, kDispatchInputCancelScript);
}

// static
void SearchBoxExtension::DispatchInputStart(blink::WebFrame* frame) {
  Dispatch(frame, kDispatchInputStartScript);
}

// static
void SearchBoxExtension::DispatchKeyCaptureChange(blink::WebFrame* frame) {
  Dispatch(frame, kDispatchKeyCaptureChangeScript);
}

// static
void SearchBoxExtension::DispatchMarginChange(blink::WebFrame* frame) {
  Dispatch(frame, kDispatchMarginChangeEventScript);
}

// static
void SearchBoxExtension::DispatchMostVisitedChanged(
    blink::WebFrame* frame) {
  Dispatch(frame, kDispatchMostVisitedChangedScript);
}

// static
void SearchBoxExtension::DispatchSubmit(blink::WebFrame* frame) {
  Dispatch(frame, kDispatchSubmitEventScript);
}

// static
void SearchBoxExtension::DispatchSuggestionChange(blink::WebFrame* frame) {
  Dispatch(frame, kDispatchSuggestionChangeEventScript);
}

// static
void SearchBoxExtension::DispatchThemeChange(blink::WebFrame* frame) {
  Dispatch(frame, kDispatchThemeChangeEventScript);
}

// static
void SearchBoxExtension::DispatchToggleVoiceSearch(
    blink::WebFrame* frame) {
  Dispatch(frame, kDispatchToggleVoiceSearchScript);
}

SearchBoxExtensionWrapper::SearchBoxExtensionWrapper(
    const base::StringPiece& code)
    : v8::Extension(kSearchBoxExtensionName, code.data(), 0, 0, code.size()) {
}

v8::Handle<v8::FunctionTemplate>
SearchBoxExtensionWrapper::GetNativeFunctionTemplate(
    v8::Isolate* isolate,
    v8::Handle<v8::String> name) {
  if (name->Equals(
          v8::String::NewFromUtf8(isolate, "CheckIsUserSignedInToChromeAs")))
    return v8::FunctionTemplate::New(isolate, CheckIsUserSignedInToChromeAs);
  if (name->Equals(v8::String::NewFromUtf8(isolate, "DeleteMostVisitedItem")))
    return v8::FunctionTemplate::New(isolate, DeleteMostVisitedItem);
  if (name->Equals(v8::String::NewFromUtf8(isolate, "Focus")))
    return v8::FunctionTemplate::New(isolate, Focus);
  if (name->Equals(v8::String::NewFromUtf8(isolate, "GetAppLauncherEnabled")))
    return v8::FunctionTemplate::New(isolate, GetAppLauncherEnabled);
  if (name->Equals(v8::String::NewFromUtf8(isolate, "GetDispositionFromClick")))
    return v8::FunctionTemplate::New(isolate, GetDispositionFromClick);
  if (name->Equals(v8::String::NewFromUtf8(isolate, "GetMostVisitedItems")))
    return v8::FunctionTemplate::New(isolate, GetMostVisitedItems);
  if (name->Equals(v8::String::NewFromUtf8(isolate, "GetMostVisitedItemData")))
    return v8::FunctionTemplate::New(isolate, GetMostVisitedItemData);
  if (name->Equals(v8::String::NewFromUtf8(isolate, "GetQuery")))
    return v8::FunctionTemplate::New(isolate, GetQuery);
  if (name->Equals(v8::String::NewFromUtf8(isolate, "GetRightToLeft")))
    return v8::FunctionTemplate::New(isolate, GetRightToLeft);
  if (name->Equals(v8::String::NewFromUtf8(isolate, "GetStartMargin")))
    return v8::FunctionTemplate::New(isolate, GetStartMargin);
  if (name->Equals(v8::String::NewFromUtf8(isolate, "GetSuggestionToPrefetch")))
    return v8::FunctionTemplate::New(isolate, GetSuggestionToPrefetch);
  if (name->Equals(v8::String::NewFromUtf8(isolate, "GetThemeBackgroundInfo")))
    return v8::FunctionTemplate::New(isolate, GetThemeBackgroundInfo);
  if (name->Equals(v8::String::NewFromUtf8(isolate, "IsFocused")))
    return v8::FunctionTemplate::New(isolate, IsFocused);
  if (name->Equals(v8::String::NewFromUtf8(isolate, "IsInputInProgress")))
    return v8::FunctionTemplate::New(isolate, IsInputInProgress);
  if (name->Equals(v8::String::NewFromUtf8(isolate, "IsKeyCaptureEnabled")))
    return v8::FunctionTemplate::New(isolate, IsKeyCaptureEnabled);
  if (name->Equals(v8::String::NewFromUtf8(isolate, "LogEvent")))
    return v8::FunctionTemplate::New(isolate, LogEvent);
  if (name->Equals(
          v8::String::NewFromUtf8(isolate, "LogMostVisitedImpression"))) {
    return v8::FunctionTemplate::New(isolate, LogMostVisitedImpression);
  }
  if (name->Equals(
          v8::String::NewFromUtf8(isolate, "LogMostVisitedNavigation"))) {
    return v8::FunctionTemplate::New(isolate, LogMostVisitedNavigation);
  }
  if (name->Equals(v8::String::NewFromUtf8(isolate, "NavigateContentWindow")))
    return v8::FunctionTemplate::New(isolate, NavigateContentWindow);
  if (name->Equals(v8::String::NewFromUtf8(isolate, "Paste")))
    return v8::FunctionTemplate::New(isolate, Paste);
  if (name->Equals(v8::String::NewFromUtf8(isolate, "SetVoiceSearchSupported")))
    return v8::FunctionTemplate::New(isolate, SetVoiceSearchSupported);
  if (name->Equals(
          v8::String::NewFromUtf8(isolate, "StartCapturingKeyStrokes")))
    return v8::FunctionTemplate::New(isolate, StartCapturingKeyStrokes);
  if (name->Equals(v8::String::NewFromUtf8(isolate, "StopCapturingKeyStrokes")))
    return v8::FunctionTemplate::New(isolate, StopCapturingKeyStrokes);
  if (name->Equals(
          v8::String::NewFromUtf8(isolate, "UndoAllMostVisitedDeletions")))
    return v8::FunctionTemplate::New(isolate, UndoAllMostVisitedDeletions);
  if (name->Equals(v8::String::NewFromUtf8(isolate, "UndoMostVisitedDeletion")))
    return v8::FunctionTemplate::New(isolate, UndoMostVisitedDeletion);
  if (name->Equals(
          v8::String::NewFromUtf8(isolate, "GetDisplayInstantResults")))
    return v8::FunctionTemplate::New(isolate, GetDisplayInstantResults);
  return v8::Handle<v8::FunctionTemplate>();
}

// static
content::RenderView* SearchBoxExtensionWrapper::GetRenderView() {
  blink::WebLocalFrame* webframe =
      blink::WebLocalFrame::frameForCurrentContext();
  if (!webframe) return NULL;

  blink::WebView* webview = webframe->view();
  if (!webview) return NULL;  // can happen during closing

  return content::RenderView::FromWebView(webview);
}

// static
void SearchBoxExtensionWrapper::CheckIsUserSignedInToChromeAs(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view || args.Length() == 0 || args[0]->IsUndefined()) return;

  DVLOG(1) << render_view << " CheckIsUserSignedInToChromeAs";

  SearchBox::Get(render_view)->CheckIsUserSignedInToChromeAs(
      V8ValueToUTF16(args[0]));
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
void SearchBoxExtensionWrapper::Focus(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view) return;

  DVLOG(1) << render_view << " Focus";
  SearchBox::Get(render_view)->Focus();
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
void SearchBoxExtensionWrapper::GetDispositionFromClick(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view || args.Length() != 5) return;

  bool middle_button = args[0]->BooleanValue();
  bool alt_key = args[1]->BooleanValue();
  bool ctrl_key = args[2]->BooleanValue();
  bool meta_key = args[3]->BooleanValue();
  bool shift_key = args[4]->BooleanValue();

  WindowOpenDisposition disposition = ui::DispositionFromClick(middle_button,
                                                               alt_key,
                                                               ctrl_key,
                                                               meta_key,
                                                               shift_key);
  v8::Isolate* isolate = args.GetIsolate();
  args.GetReturnValue().Set(v8::Int32::New(isolate, disposition));
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
  v8::Isolate* isolate = args.GetIsolate();
  v8::Handle<v8::Array> v8_mv_items =
      v8::Array::New(isolate, instant_mv_items.size());
  for (size_t i = 0; i < instant_mv_items.size(); ++i) {
    v8_mv_items->Set(i,
                     GenerateMostVisitedItem(isolate,
                                             render_view->GetRoutingID(),
                                             instant_mv_items[i].first,
                                             instant_mv_items[i].second));
  }
  args.GetReturnValue().Set(v8_mv_items);
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
  v8::Isolate* isolate = args.GetIsolate();
  args.GetReturnValue().Set(GenerateMostVisitedItem(
      isolate, render_view->GetRoutingID(), restricted_id, mv_item));
}

// static
void SearchBoxExtensionWrapper::GetQuery(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view) return;
  const base::string16& query = SearchBox::Get(render_view)->query();
  DVLOG(1) << render_view << " GetQuery: '" << query << "'";
  v8::Isolate* isolate = args.GetIsolate();
  args.GetReturnValue().Set(UTF16ToV8String(isolate, query));
}

// static
void SearchBoxExtensionWrapper::GetRightToLeft(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  args.GetReturnValue().Set(base::i18n::IsRTL());
}

// static
void SearchBoxExtensionWrapper::GetStartMargin(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view) return;
  args.GetReturnValue().Set(static_cast<int32_t>(
      SearchBox::Get(render_view)->start_margin()));
}

// static
void SearchBoxExtensionWrapper::GetSuggestionToPrefetch(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view) return;

  const InstantSuggestion& suggestion =
      SearchBox::Get(render_view)->suggestion();
  v8::Isolate* isolate = args.GetIsolate();
  v8::Handle<v8::Object> data = v8::Object::New(isolate);
  data->Set(v8::String::NewFromUtf8(isolate, "text"),
            UTF16ToV8String(isolate, suggestion.text));
  data->Set(v8::String::NewFromUtf8(isolate, "metadata"),
            UTF8ToV8String(isolate, suggestion.metadata));
  args.GetReturnValue().Set(data);
}

// static
void SearchBoxExtensionWrapper::GetThemeBackgroundInfo(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view) return;

  DVLOG(1) << render_view << " GetThemeBackgroundInfo";
  const ThemeBackgroundInfo& theme_info =
      SearchBox::Get(render_view)->GetThemeBackgroundInfo();
  v8::Isolate* isolate = args.GetIsolate();
  v8::Handle<v8::Object> info = v8::Object::New(isolate);

  info->Set(v8::String::NewFromUtf8(isolate, "usingDefaultTheme"),
            v8::Boolean::New(isolate, theme_info.using_default_theme));

  // The theme background color is in RGBA format "rgba(R,G,B,A)" where R, G and
  // B are between 0 and 255 inclusive, and A is a double between 0 and 1
  // inclusive.
  // This is the CSS "background-color" format.
  // Value is always valid.
  // TODO(jfweitz): Remove this field after GWS is modified to use the new
  // backgroundColorRgba field.
  info->Set(
      v8::String::NewFromUtf8(isolate, "colorRgba"),
      UTF8ToV8String(
          isolate,
          // Convert the alpha using DoubleToString because StringPrintf will
          // use
          // locale specific formatters (e.g., use , instead of . in German).
          base::StringPrintf(
              kCSSBackgroundColorFormat,
              theme_info.background_color.r,
              theme_info.background_color.g,
              theme_info.background_color.b,
              base::DoubleToString(theme_info.background_color.a / 255.0)
                  .c_str())));

  // Theme color for background as an array with the RGBA components in order.
  // Value is always valid.
  info->Set(v8::String::NewFromUtf8(isolate, "backgroundColorRgba"),
            internal::RGBAColorToArray(isolate, theme_info.background_color));

  // Theme color for text as an array with the RGBA components in order.
  // Value is always valid.
  info->Set(v8::String::NewFromUtf8(isolate, "textColorRgba"),
            internal::RGBAColorToArray(isolate, theme_info.text_color));

  // Theme color for links as an array with the RGBA components in order.
  // Value is always valid.
  info->Set(v8::String::NewFromUtf8(isolate, "linkColorRgba"),
            internal::RGBAColorToArray(isolate, theme_info.link_color));

  // Theme color for light text as an array with the RGBA components in order.
  // Value is always valid.
  info->Set(v8::String::NewFromUtf8(isolate, "textColorLightRgba"),
            internal::RGBAColorToArray(isolate, theme_info.text_color_light));

  // Theme color for header as an array with the RGBA components in order.
  // Value is always valid.
  info->Set(v8::String::NewFromUtf8(isolate, "headerColorRgba"),
            internal::RGBAColorToArray(isolate, theme_info.header_color));

  // Theme color for section border as an array with the RGBA components in
  // order. Value is always valid.
  info->Set(
      v8::String::NewFromUtf8(isolate, "sectionBorderColorRgba"),
      internal::RGBAColorToArray(isolate, theme_info.section_border_color));

  // The theme alternate logo value indicates a white logo when TRUE and a
  // colorful one when FALSE.
  info->Set(v8::String::NewFromUtf8(isolate, "alternateLogo"),
            v8::Boolean::New(isolate, theme_info.logo_alternate));

  // The theme background image url is of format kCSSBackgroundImageFormat
  // where both instances of "%s" are replaced with the id that identifies the
  // theme.
  // This is the CSS "background-image" format.
  // Value is only valid if there's a custom theme background image.
  if (crx_file::id_util::IdIsValid(theme_info.theme_id)) {
    info->Set(v8::String::NewFromUtf8(isolate, "imageUrl"),
              UTF8ToV8String(isolate,
                             base::StringPrintf(kCSSBackgroundImageFormat,
                                                theme_info.theme_id.c_str(),
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
    info->Set(v8::String::NewFromUtf8(isolate, "imageHorizontalAlignment"),
              UTF8ToV8String(isolate, alignment));

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
    info->Set(v8::String::NewFromUtf8(isolate, "imageVerticalAlignment"),
              UTF8ToV8String(isolate, alignment));

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
    info->Set(v8::String::NewFromUtf8(isolate, "imageTiling"),
              UTF8ToV8String(isolate, tiling));

    // The theme background image height is only valid if |imageUrl| is valid.
    info->Set(v8::String::NewFromUtf8(isolate, "imageHeight"),
              v8::Int32::New(isolate, theme_info.image_height));

    // The attribution URL is only valid if the theme has attribution logo.
    if (theme_info.has_attribution) {
      info->Set(
          v8::String::NewFromUtf8(isolate, "attributionUrl"),
          UTF8ToV8String(isolate,
                         base::StringPrintf(kThemeAttributionFormat,
                                            theme_info.theme_id.c_str(),
                                            theme_info.theme_id.c_str())));
    }
  }

  args.GetReturnValue().Set(info);
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
void SearchBoxExtensionWrapper::IsKeyCaptureEnabled(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view) return;

  args.GetReturnValue().Set(SearchBox::Get(render_view)->
                            is_key_capture_enabled());
}

// static
void SearchBoxExtensionWrapper::LogEvent(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  content::RenderView* render_view = GetRenderViewWithCheckedOrigin(
      GURL(chrome::kChromeSearchMostVisitedUrl));
  if (!render_view) return;

  if (args.Length() < 1 || !args[0]->IsNumber())
    return;

  DVLOG(1) << render_view << " LogEvent";

  if (args[0]->Uint32Value() < NTP_NUM_EVENT_TYPES) {
    NTPLoggingEventType event =
        static_cast<NTPLoggingEventType>(args[0]->Uint32Value());
    SearchBox::Get(render_view)->LogEvent(event);
  }
}

// static
void SearchBoxExtensionWrapper::LogMostVisitedImpression(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  content::RenderView* render_view = GetRenderViewWithCheckedOrigin(
      GURL(chrome::kChromeSearchMostVisitedUrl));
  if (!render_view) return;

  if (args.Length() < 2 || !args[0]->IsNumber() || args[1]->IsUndefined())
    return;

  DVLOG(1) << render_view << " LogMostVisitedImpression";

  SearchBox::Get(render_view)->LogMostVisitedImpression(
      args[0]->IntegerValue(), V8ValueToUTF16(args[1]));
}

// static
void SearchBoxExtensionWrapper::LogMostVisitedNavigation(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  content::RenderView* render_view = GetRenderViewWithCheckedOrigin(
      GURL(chrome::kChromeSearchMostVisitedUrl));
  if (!render_view) return;

  if (args.Length() < 2 || !args[0]->IsNumber() || args[1]->IsUndefined())
    return;

  DVLOG(1) << render_view << " LogMostVisitedNavigation";

  SearchBox::Get(render_view)->LogMostVisitedNavigation(
      args[0]->IntegerValue(), V8ValueToUTF16(args[1]));
}

// static
void SearchBoxExtensionWrapper::NavigateContentWindow(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view || !args.Length()) return;

  GURL destination_url;
  bool is_most_visited_item_url = false;
  // Check if the url is a rid
  if (args[0]->IsNumber()) {
    InstantMostVisitedItem item;
    if (SearchBox::Get(render_view)->GetMostVisitedItemWithID(
            args[0]->IntegerValue(), &item)) {
      destination_url = item.url;
      is_most_visited_item_url = true;
    }
  } else {
    // Resolve the URL
    const base::string16& possibly_relative_url = V8ValueToUTF16(args[0]);
  GURL current_url = GetCurrentURL(render_view);
    destination_url = internal::ResolveURL(current_url, possibly_relative_url);
  }

  DVLOG(1) << render_view << " NavigateContentWindow: " << destination_url;

  // Navigate the main frame.
  if (destination_url.is_valid() &&
      !destination_url.SchemeIs(url::kJavaScriptScheme)) {
    WindowOpenDisposition disposition = CURRENT_TAB;
    if (args[1]->IsNumber()) {
      disposition = (WindowOpenDisposition) args[1]->Uint32Value();
    }
    SearchBox::Get(render_view)->NavigateToURL(destination_url, disposition,
                                               is_most_visited_item_url);
  }
}

// static
void SearchBoxExtensionWrapper::Paste(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view) return;

  base::string16 text;
  if (!args[0]->IsUndefined())
    text = V8ValueToUTF16(args[0]);

  DVLOG(1) << render_view << " Paste: " << text;
  SearchBox::Get(render_view)->Paste(text);
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
void SearchBoxExtensionWrapper::SetVoiceSearchSupported(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view || args.Length() < 1) return;

  DVLOG(1) << render_view << " SetVoiceSearchSupported";
  SearchBox::Get(render_view)->SetVoiceSearchSupported(args[0]->BooleanValue());
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
void SearchBoxExtensionWrapper::UndoMostVisitedDeletion(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view || !args.Length()) return;

  DVLOG(1) << render_view << " UndoMostVisitedDeletion";
  SearchBox::Get(render_view)->UndoMostVisitedDeletion(args[0]->IntegerValue());
}

// static
void SearchBoxExtensionWrapper::GetDisplayInstantResults(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view) return;

  bool display_instant_results =
      SearchBox::Get(render_view)->display_instant_results();
  DVLOG(1) << render_view << " GetDisplayInstantResults" <<
      display_instant_results;
  args.GetReturnValue().Set(display_instant_results);
}

}  // namespace extensions_v8
