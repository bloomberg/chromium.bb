// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/searchbox/searchbox_extension.h"

#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/renderer/searchbox/searchbox.h"
#include "content/public/renderer/render_view.h"
#include "grit/renderer_resources.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebScriptSource.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebURLRequest.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/base/resource/resource_bundle.h"
#include "v8/include/v8.h"

namespace {

// Converts a V8 value to a string16.
string16 V8ValueToUTF16(v8::Handle<v8::Value> v) {
  v8::String::Value s(v);
  return string16(reinterpret_cast<const char16*>(*s), s.length());
}

// Converts string16 to V8 String.
v8::Handle<v8::String> UTF16ToV8String(const string16& s) {
  return v8::String::New(reinterpret_cast<const uint16_t*>(s.data()), s.size());
}

void Dispatch(WebKit::WebFrame* frame, const WebKit::WebString& script) {
  if (!frame) return;
  frame->executeScript(WebKit::WebScriptSource(script));
}

}  // namespace

namespace extensions_v8 {

static const char kSearchBoxExtensionName[] = "v8/SearchBox";

static const char kDispatchChangeEventScript[] =
    "if (window.chrome &&"
    "    window.chrome.searchBox &&"
    "    window.chrome.searchBox.onchange &&"
    "    typeof window.chrome.searchBox.onchange == 'function') {"
    "  window.chrome.searchBox.onchange();"
    "  true;"
    "}";

static const char kDispatchSubmitEventScript[] =
    "if (window.chrome &&"
    "    window.chrome.searchBox &&"
    "    window.chrome.searchBox.onsubmit &&"
    "    typeof window.chrome.searchBox.onsubmit == 'function') {"
    "  window.chrome.searchBox.onsubmit();"
    "  true;"
    "}";

static const char kDispatchCancelEventScript[] =
    "if (window.chrome &&"
    "    window.chrome.searchBox &&"
    "    window.chrome.searchBox.oncancel &&"
    "    typeof window.chrome.searchBox.oncancel == 'function') {"
    "  window.chrome.searchBox.oncancel();"
    "  true;"
    "}";

static const char kDispatchResizeEventScript[] =
    "if (window.chrome &&"
    "    window.chrome.searchBox &&"
    "    window.chrome.searchBox.onresize &&"
    "    typeof window.chrome.searchBox.onresize == 'function') {"
    "  window.chrome.searchBox.onresize();"
    "  true;"
    "}";

// We first send this script down to determine if the page supports instant.
static const char kSupportsInstantScript[] =
    "if (window.chrome &&"
    "    window.chrome.searchBox &&"
    "    window.chrome.searchBox.onsubmit &&"
    "    typeof window.chrome.searchBox.onsubmit == 'function') {"
    "  true;"
    "} else {"
    "  false;"
    "}";

// Extended API.
static const char kDispatchAutocompleteResultsEventScript[] =
    "if (window.chrome &&"
    "    window.chrome.searchBox &&"
    "    window.chrome.searchBox.onnativesuggestions &&"
    "    typeof window.chrome.searchBox.onnativesuggestions == 'function') {"
    "  window.chrome.searchBox.onnativesuggestions();"
    "  true;"
    "}";

// Takes two printf-style replaceable values: count, key_code.
static const char kDispatchUpOrDownKeyPressEventScript[] =
    "if (window.chrome &&"
    "    window.chrome.searchBox &&"
    "    window.chrome.searchBox.onkeypress &&"
    "    typeof window.chrome.searchBox.onkeypress == 'function') {"
    "  for (var i = 0; i < %d; ++i)"
    "    window.chrome.searchBox.onkeypress({keyCode: %d});"
    "  true;"
    "}";

static const char kDispatchContextChangeEventScript[] =
    "if (window.chrome &&"
    "    window.chrome.searchBox &&"
    "    window.chrome.searchBox.oncontextchange &&"
    "    typeof window.chrome.searchBox.oncontextchange == 'function') {"
    "  window.chrome.searchBox.oncontextchange();"
    "  true;"
    "}";

// ----------------------------------------------------------------------------

class SearchBoxExtensionWrapper : public v8::Extension {
 public:
  explicit SearchBoxExtensionWrapper(const base::StringPiece& code);

  // Allows v8's javascript code to call the native functions defined
  // in this class for window.chrome.
  virtual v8::Handle<v8::FunctionTemplate> GetNativeFunction(
      v8::Handle<v8::String> name);

  // Helper function to find the RenderView. May return NULL.
  static content::RenderView* GetRenderView();

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

  // Gets the width of the region of the search box that overlaps the window.
  static v8::Handle<v8::Value> GetWidth(const v8::Arguments& args);

  // Gets the height of the region of the search box that overlaps the window.
  static v8::Handle<v8::Value> GetHeight(const v8::Arguments& args);

  // Gets the autocomplete results from search box.
  static v8::Handle<v8::Value> GetAutocompleteResults(
      const v8::Arguments& args);

  // Gets the current session context.
  static v8::Handle<v8::Value> GetContext(const v8::Arguments& args);

  // Navigates the window to a URL represented by either a URL string or a
  // restricted ID.
  static v8::Handle<v8::Value> NavigateContentWindow(const v8::Arguments& args);

  // Sets ordered suggestions. Valid for current |value|.
  static v8::Handle<v8::Value> SetSuggestions(const v8::Arguments& args);

  // Sets the text to be autocompleted into the search box.
  static v8::Handle<v8::Value> SetQuerySuggestion(const v8::Arguments& args);

  // Like |SetQuerySuggestion| but uses a restricted ID to identify the text.
  static v8::Handle<v8::Value> SetQuerySuggestionFromAutocompleteResult(
      const v8::Arguments& args);

  // Sets the search box text, completely replacing what the user typed.
  static v8::Handle<v8::Value> SetQuery(const v8::Arguments& args);

  // Like |SetQuery| but uses a restricted ID to identify the text.
  static v8::Handle<v8::Value> SetQueryFromAutocompleteResult(
      const v8::Arguments& args);

  // Requests the preview be shown with the specified contents and height.
  static v8::Handle<v8::Value> Show(const v8::Arguments& args);

 private:
  DISALLOW_COPY_AND_ASSIGN(SearchBoxExtensionWrapper);
};

SearchBoxExtensionWrapper::SearchBoxExtensionWrapper(
    const base::StringPiece& code)
    : v8::Extension(kSearchBoxExtensionName, code.data(), 0, 0, code.size()) {
}

v8::Handle<v8::FunctionTemplate> SearchBoxExtensionWrapper::GetNativeFunction(
    v8::Handle<v8::String> name) {
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
  if (name->Equals(v8::String::New("GetAutocompleteResults")))
    return v8::FunctionTemplate::New(GetAutocompleteResults);
  if (name->Equals(v8::String::New("GetContext")))
    return v8::FunctionTemplate::New(GetContext);
  if (name->Equals(v8::String::New("NavigateContentWindow")))
    return v8::FunctionTemplate::New(NavigateContentWindow);
  if (name->Equals(v8::String::New("SetSuggestions")))
    return v8::FunctionTemplate::New(SetSuggestions);
  if (name->Equals(v8::String::New("SetQuerySuggestion")))
    return v8::FunctionTemplate::New(SetQuerySuggestion);
  if (name->Equals(v8::String::New("SetQuerySuggestionFromAutocompleteResult")))
    return v8::FunctionTemplate::New(SetQuerySuggestionFromAutocompleteResult);
  if (name->Equals(v8::String::New("SetQuery")))
    return v8::FunctionTemplate::New(SetQuery);
  if (name->Equals(v8::String::New("SetQueryFromAutocompleteResult")))
    return v8::FunctionTemplate::New(SetQueryFromAutocompleteResult);
  if (name->Equals(v8::String::New("Show")))
    return v8::FunctionTemplate::New(Show);
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

  return UTF16ToV8String(SearchBox::Get(render_view)->query());
}

// static
v8::Handle<v8::Value> SearchBoxExtensionWrapper::GetVerbatim(
    const v8::Arguments& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view) return v8::Undefined();

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

  return v8::Int32::New(SearchBox::Get(render_view)->GetRect().x());
}

// static
v8::Handle<v8::Value> SearchBoxExtensionWrapper::GetY(
    const v8::Arguments& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view) return v8::Undefined();

  return v8::Int32::New(SearchBox::Get(render_view)->GetRect().y());
}

// static
v8::Handle<v8::Value> SearchBoxExtensionWrapper::GetWidth(
    const v8::Arguments& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view) return v8::Undefined();

  return v8::Int32::New(SearchBox::Get(render_view)->GetRect().width());
}

// static
v8::Handle<v8::Value> SearchBoxExtensionWrapper::GetHeight(
    const v8::Arguments& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view) return v8::Undefined();

  return v8::Int32::New(SearchBox::Get(render_view)->GetRect().height());
}

// static
v8::Handle<v8::Value> SearchBoxExtensionWrapper::GetAutocompleteResults(
    const v8::Arguments& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view) return v8::Undefined();

  const std::vector<InstantAutocompleteResult>& results =
      SearchBox::Get(render_view)->GetAutocompleteResults();
  const size_t results_base = SearchBox::Get(render_view)->results_base();

  v8::Handle<v8::Array> results_array = v8::Array::New(results.size());
  for (size_t i = 0; i < results.size(); ++i) {
    v8::Handle<v8::Object> result = v8::Object::New();
    result->Set(v8::String::New("provider"),
                UTF16ToV8String(results[i].provider));
    result->Set(v8::String::New("type"), UTF16ToV8String(results[i].type));
    result->Set(v8::String::New("contents"),
                UTF16ToV8String(results[i].description));
    result->Set(v8::String::New("destination_url"),
                UTF16ToV8String(results[i].destination_url));
    result->Set(v8::String::New("rid"), v8::Uint32::New(results_base + i));

    v8::Handle<v8::Object> ranking_data = v8::Object::New();
    ranking_data->Set(v8::String::New("relevance"),
                      v8::Int32::New(results[i].relevance));
    result->Set(v8::String::New("rankingData"), ranking_data);

    results_array->Set(i, result);
  }
  return results_array;
}

// static
v8::Handle<v8::Value> SearchBoxExtensionWrapper::GetContext(
    const v8::Arguments& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view) return v8::Undefined();

  v8::Handle<v8::Object> context = v8::Object::New();
  context->Set(
      v8::String::New("isNewTabPage"),
      v8::Boolean::New(SearchBox::Get(render_view)->active_tab_is_ntp()));
  return context;
}

// static
v8::Handle<v8::Value> SearchBoxExtensionWrapper::NavigateContentWindow(
    const v8::Arguments& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view || !args.Length()) return v8::Undefined();

  GURL destination_url;
  if (args[0]->IsNumber()) {
    const InstantAutocompleteResult* result = SearchBox::Get(render_view)->
        GetAutocompleteResultWithId(args[0]->Uint32Value());
    if (result)
      destination_url = GURL(result->destination_url);
  } else {
    destination_url = GURL(V8ValueToUTF16(args[0]));
  }

  // Navigate the main frame.
  if (destination_url.is_valid()) {
    WebKit::WebURLRequest request(destination_url);
    render_view->GetWebView()->mainFrame()->loadRequest(request);
  }

  return v8::Undefined();
}

// static
v8::Handle<v8::Value> SearchBoxExtensionWrapper::SetSuggestions(
    const v8::Arguments& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view || !args.Length()) return v8::Undefined();

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
      suggestions.push_back(InstantSuggestion(text, behavior, type));
    }
  }

  SearchBox::Get(render_view)->SetSuggestions(suggestions);

  return v8::Undefined();
}

// static
v8::Handle<v8::Value> SearchBoxExtensionWrapper::SetQuerySuggestion(
    const v8::Arguments& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view || args.Length() < 2) return v8::Undefined();

  string16 text = V8ValueToUTF16(args[0]);
  InstantCompleteBehavior behavior = INSTANT_COMPLETE_NOW;
  InstantSuggestionType type = INSTANT_SUGGESTION_URL;

  if (args[1]->Uint32Value() == 2) {
    behavior = INSTANT_COMPLETE_NEVER;
    type = INSTANT_SUGGESTION_SEARCH;
  }

  std::vector<InstantSuggestion> suggestions;
  suggestions.push_back(InstantSuggestion(text, behavior, type));
  SearchBox::Get(render_view)->SetSuggestions(suggestions);

  return v8::Undefined();
}

// static
v8::Handle<v8::Value>
    SearchBoxExtensionWrapper::SetQuerySuggestionFromAutocompleteResult(
        const v8::Arguments& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view || !args.Length()) return v8::Undefined();

  const InstantAutocompleteResult* result = SearchBox::Get(render_view)->
      GetAutocompleteResultWithId(args[0]->Uint32Value());
  if (!result) return v8::Undefined();

  // We only support selecting autocomplete results that are URLs.
  string16 text = result->destination_url;
  InstantCompleteBehavior behavior = INSTANT_COMPLETE_NOW;
  InstantSuggestionType type = INSTANT_SUGGESTION_URL;

  std::vector<InstantSuggestion> suggestions;
  suggestions.push_back(InstantSuggestion(text, behavior, type));
  SearchBox::Get(render_view)->SetSuggestions(suggestions);

  return v8::Undefined();
}

// static
v8::Handle<v8::Value> SearchBoxExtensionWrapper::SetQuery(
    const v8::Arguments& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view || args.Length() < 2) return v8::Undefined();

  string16 text = V8ValueToUTF16(args[0]);
  InstantCompleteBehavior behavior = INSTANT_COMPLETE_REPLACE;
  InstantSuggestionType type = INSTANT_SUGGESTION_SEARCH;

  if (args[1]->Uint32Value() == 1)
    type = INSTANT_SUGGESTION_URL;

  std::vector<InstantSuggestion> suggestions;
  suggestions.push_back(InstantSuggestion(text, behavior, type));
  SearchBox::Get(render_view)->SetSuggestions(suggestions);

  return v8::Undefined();
}

v8::Handle<v8::Value>
    SearchBoxExtensionWrapper::SetQueryFromAutocompleteResult(
        const v8::Arguments& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view || !args.Length()) return v8::Undefined();

  const InstantAutocompleteResult* result = SearchBox::Get(render_view)->
      GetAutocompleteResultWithId(args[0]->Uint32Value());
  if (!result) return v8::Undefined();

  // We only support selecting autocomplete results that are URLs.
  string16 text = result->destination_url;
  InstantCompleteBehavior behavior = INSTANT_COMPLETE_REPLACE;
  // TODO(jered): Distinguish between history URLs and search provider
  // navsuggest URLs so that we can do proper accounting on history URLs.
  InstantSuggestionType type = INSTANT_SUGGESTION_URL;

  std::vector<InstantSuggestion> suggestions;
  suggestions.push_back(InstantSuggestion(text, behavior, type));
  SearchBox::Get(render_view)->SetSuggestions(suggestions);

  return v8::Undefined();
}

// static
v8::Handle<v8::Value> SearchBoxExtensionWrapper::Show(
    const v8::Arguments& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view || args.Length() < 2) return v8::Undefined();

  InstantShownReason reason = INSTANT_SHOWN_NOT_SPECIFIED;
  switch (args[0]->Uint32Value()) {
    case 0: reason = INSTANT_SHOWN_NOT_SPECIFIED; break;
    case 1: reason = INSTANT_SHOWN_CUSTOM_NTP_CONTENT; break;
    case 2: reason = INSTANT_SHOWN_QUERY_SUGGESTIONS; break;
    case 3: reason = INSTANT_SHOWN_ZERO_SUGGESTIONS; break;
  }

  int height = 100;
  InstantSizeUnits units = INSTANT_SIZE_PERCENT;
  if (args[1]->IsInt32()) {
    height = args[1]->Int32Value();
    units = INSTANT_SIZE_PIXELS;
  }

  SearchBox::Get(render_view)->ShowInstantPreview(reason, height, units);

  return v8::Undefined();
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
bool SearchBoxExtension::PageSupportsInstant(WebKit::WebFrame* frame) {
  if (!frame) return false;

  v8::Handle<v8::Value> v = frame->executeScriptAndReturnValue(
      WebKit::WebScriptSource(kSupportsInstantScript));
  bool supports_instant = !v.IsEmpty() && v->BooleanValue();

  // Send a resize message to tell the page that Chrome is actively using the
  // searchbox API with it. The page uses the message to transition from
  // "homepage" mode to "search" mode.
  if (supports_instant)
    DispatchResize(frame);

  return supports_instant;
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
void SearchBoxExtension::DispatchContextChange(WebKit::WebFrame* frame) {
  Dispatch(frame, kDispatchContextChangeEventScript);
}

// static
v8::Extension* SearchBoxExtension::Get() {
  return new SearchBoxExtensionWrapper(ResourceBundle::GetSharedInstance().
      GetRawDataResource(IDR_SEARCHBOX_API));
}

}  // namespace extensions_v8
