// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/searchbox/searchbox_extension.h"

#include <ctype.h>

#include "base/string_number_conversions.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/renderer/searchbox/searchbox.h"
#include "content/public/renderer/render_view.h"
#include "grit/renderer_resources.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebScriptSource.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/base/resource/resource_bundle.h"
#include "v8/include/v8.h"

namespace {

// Splits the string in |number| into two pieces, a leading number token (saved
// in |number|) and the rest (saved in |suffix|). Either piece may become empty,
// depending on whether the input had no digits or only digits. Neither argument
// may be NULL.
void SplitLeadingNumberToken(std::string* number, std::string* suffix) {
  size_t i = 0;
  while (i < number->size() && isdigit((*number)[i]))
    ++i;
  suffix->assign(*number, i, number->size() - i);
  number->resize(i);
}

// Converts a V8 value to a string16.
string16 V8ValueToUTF16(v8::Handle<v8::Value> v) {
  v8::String::Value s(v);
  return string16(reinterpret_cast<const char16*>(*s), s.length());
}

// Converts string16 to V8 String.
v8::Handle<v8::String> UTF16ToV8String(const string16& s) {
  return v8::String::New(reinterpret_cast<const uint16_t*>(s.data()), s.size());
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

  // Resize the preview to the given height.
  static v8::Handle<v8::Value> SetPreviewHeight(const v8::Arguments& args);

 private:
  DISALLOW_COPY_AND_ASSIGN(SearchBoxExtensionWrapper);
};

SearchBoxExtensionWrapper::SearchBoxExtensionWrapper(
    const base::StringPiece& code)
    : v8::Extension(kSearchBoxExtensionName, code.data(), 0, 0, code.size()) {
}

v8::Handle<v8::FunctionTemplate> SearchBoxExtensionWrapper::GetNativeFunction(
    v8::Handle<v8::String> name) {
  if (name->Equals(v8::String::New("GetQuery"))) {
    return v8::FunctionTemplate::New(GetQuery);
  } else if (name->Equals(v8::String::New("GetVerbatim"))) {
    return v8::FunctionTemplate::New(GetVerbatim);
  } else if (name->Equals(v8::String::New("GetSelectionStart"))) {
    return v8::FunctionTemplate::New(GetSelectionStart);
  } else if (name->Equals(v8::String::New("GetSelectionEnd"))) {
    return v8::FunctionTemplate::New(GetSelectionEnd);
  } else if (name->Equals(v8::String::New("GetX"))) {
    return v8::FunctionTemplate::New(GetX);
  } else if (name->Equals(v8::String::New("GetY"))) {
    return v8::FunctionTemplate::New(GetY);
  } else if (name->Equals(v8::String::New("GetWidth"))) {
    return v8::FunctionTemplate::New(GetWidth);
  } else if (name->Equals(v8::String::New("GetHeight"))) {
    return v8::FunctionTemplate::New(GetHeight);
  } else if (name->Equals(v8::String::New("GetAutocompleteResults"))) {
    return v8::FunctionTemplate::New(GetAutocompleteResults);
  } else if (name->Equals(v8::String::New("SetSuggestions"))) {
    return v8::FunctionTemplate::New(SetSuggestions);
  } else if (name->Equals(v8::String::New("SetQuerySuggestion"))) {
    return v8::FunctionTemplate::New(SetQuerySuggestion);
  } else if (name->Equals(v8::String::New(
      "SetQuerySuggestionFromAutocompleteResult"))) {
    return v8::FunctionTemplate::New(SetQuerySuggestionFromAutocompleteResult);
  } else if (name->Equals(v8::String::New("SetQuery"))) {
    return v8::FunctionTemplate::New(SetQuery);
  } else if (name->Equals(v8::String::New("SetQueryFromAutocompleteResult"))) {
    return v8::FunctionTemplate::New(SetQueryFromAutocompleteResult);
  } else if (name->Equals(v8::String::New("SetPreviewHeight"))) {
    return v8::FunctionTemplate::New(SetPreviewHeight);
  }
  return v8::Handle<v8::FunctionTemplate>();
}

// static
content::RenderView* SearchBoxExtensionWrapper::GetRenderView() {
  WebKit::WebFrame* webframe = WebKit::WebFrame::frameForEnteredContext();
  DCHECK(webframe) << "There should be an active frame since we just got "
                      "a native function called.";
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
      SearchBox::Get(render_view)->autocomplete_results();
  const int results_base = SearchBox::Get(render_view)->results_base();
  v8::Handle<v8::Array> results_array = v8::Array::New(results.size());
  for (size_t i = 0; i < results.size(); ++i) {
    v8::Handle<v8::Object> result = v8::Object::New();
    result->Set(v8::String::New("provider"),
                UTF16ToV8String(results[i].provider));
    result->Set(v8::String::New("contents"),
                UTF16ToV8String(results[i].contents));
    result->Set(v8::String::New("destination_url"),
                v8::String::New(results[i].destination_url.spec().c_str()));
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
v8::Handle<v8::Value> SearchBoxExtensionWrapper::SetSuggestions(
    const v8::Arguments& args) {
  std::vector<InstantSuggestion> suggestions;

  if (args.Length() && args[0]->IsObject()) {
    v8::Handle<v8::Object> suggestion_json = args[0]->ToObject();

    InstantCompleteBehavior behavior = INSTANT_COMPLETE_NOW;
    InstantSuggestionType type = INSTANT_SUGGESTION_SEARCH;
    v8::Handle<v8::Value> complete_value =
          suggestion_json->Get(v8::String::New("complete_behavior"));
    if (complete_value->IsString()) {
      if (complete_value->Equals(v8::String::New("now"))) {
        behavior = INSTANT_COMPLETE_NOW;
      } else if (complete_value->Equals(v8::String::New("never"))) {
        behavior = INSTANT_COMPLETE_NEVER;
      } else if (complete_value->Equals(v8::String::New("delayed"))) {
        behavior = INSTANT_COMPLETE_DELAYED;
      } else if (complete_value->Equals(v8::String::New("replace"))) {
        behavior = INSTANT_COMPLETE_REPLACE;
      } else {
        VLOG(1) << "Unsupported complete behavior '"
                << *v8::String::Utf8Value(complete_value) << "'";
      }
    }
    v8::Handle<v8::Value> suggestions_field =
        suggestion_json->Get(v8::String::New("suggestions"));
    if (suggestions_field->IsArray()) {
      v8::Handle<v8::Array> suggestions_array =
          suggestions_field.As<v8::Array>();
      size_t length = suggestions_array->Length();
      for (size_t i = 0; i < length; i++) {
        v8::Handle<v8::Value> suggestion_value = suggestions_array->Get(i);
        if (!suggestion_value->IsObject()) continue;

        v8::Handle<v8::Object> suggestion_object = suggestion_value->ToObject();
        v8::Handle<v8::Value> suggestion_object_value =
            suggestion_object->Get(v8::String::New("value"));
        if (!suggestion_object_value->IsString()) continue;
        string16 text = V8ValueToUTF16(suggestion_object_value);

        suggestions.push_back(InstantSuggestion(text, behavior, type));
      }
    }
  }

  if (content::RenderView* render_view = GetRenderView())
    SearchBox::Get(render_view)->SetSuggestions(suggestions);
  return v8::Undefined();
}

// static
v8::Handle<v8::Value> SearchBoxExtensionWrapper::SetQuerySuggestion(
    const v8::Arguments& args) {
  if (1 <= args.Length() && args.Length() <= 2 && args[0]->IsString()) {
    string16 text = V8ValueToUTF16(args[0]);
    InstantCompleteBehavior behavior = INSTANT_COMPLETE_NOW;
    InstantSuggestionType type = INSTANT_SUGGESTION_URL;

    if (args.Length() >= 2 && args[1]->Uint32Value() == 2) {
      behavior = INSTANT_COMPLETE_NEVER;
      // TODO(sreeram): The page should really set the type explicitly.
      type = INSTANT_SUGGESTION_SEARCH;
    }

    if (content::RenderView* render_view = GetRenderView()) {
      std::vector<InstantSuggestion> suggestions;
      suggestions.push_back(InstantSuggestion(text, behavior, type));
      SearchBox::Get(render_view)->SetSuggestions(suggestions);
    }
  }
  return v8::Undefined();
}

// static
v8::Handle<v8::Value>
    SearchBoxExtensionWrapper::SetQuerySuggestionFromAutocompleteResult(
        const v8::Arguments& args) {
  content::RenderView* render_view = GetRenderView();
  if (1 <= args.Length() && args.Length() <= 2 && args[0]->IsNumber() &&
      render_view) {
    const int results_id = args[0]->Uint32Value();
    const int results_base = SearchBox::Get(render_view)->results_base();
    // Note that stale results_ids, less than the current results_base, will
    // wrap.
    const size_t index = results_id - results_base;
    const std::vector<InstantAutocompleteResult>& suggestions =
        SearchBox::Get(render_view)->autocomplete_results();
    if (index < suggestions.size()) {
      string16 text = UTF8ToUTF16(suggestions[index].destination_url.spec());
      InstantCompleteBehavior behavior = INSTANT_COMPLETE_NOW;
      InstantSuggestionType type = INSTANT_SUGGESTION_URL;

      if (args.Length() >= 2 && args[1]->Uint32Value() == 2)
        behavior = INSTANT_COMPLETE_NEVER;

      if (suggestions[index].is_search) {
        text = suggestions[index].contents;
        type = INSTANT_SUGGESTION_SEARCH;
      }

      std::vector<InstantSuggestion> suggestions;
      suggestions.push_back(InstantSuggestion(text, behavior, type));
      SearchBox::Get(render_view)->SetSuggestions(suggestions);
    } else {
      VLOG(1) << "Invalid results_id " << results_id << "; "
              << "results_base is " << results_base << ".";
    }
  }
  return v8::Undefined();
}

// static
v8::Handle<v8::Value> SearchBoxExtensionWrapper::SetQuery(
    const v8::Arguments& args) {
  if (1 <= args.Length() && args.Length() <= 2 && args[0]->IsString()) {
    string16 text = V8ValueToUTF16(args[0]);
    InstantCompleteBehavior behavior = INSTANT_COMPLETE_REPLACE;
    InstantSuggestionType type = INSTANT_SUGGESTION_URL;

    if (args.Length() >= 2 && args[1]->Uint32Value() == 0)
      type = INSTANT_SUGGESTION_SEARCH;

    if (content::RenderView* render_view = GetRenderView()) {
      std::vector<InstantSuggestion> suggestions;
      suggestions.push_back(InstantSuggestion(text, behavior, type));
      SearchBox::Get(render_view)->SetSuggestions(suggestions);
    }
  }
  return v8::Undefined();
}

v8::Handle<v8::Value>
    SearchBoxExtensionWrapper::SetQueryFromAutocompleteResult(
        const v8::Arguments& args) {
  content::RenderView* render_view = GetRenderView();
  if (1 <= args.Length() && args.Length() <= 2 && args[0]->IsNumber() &&
      render_view) {
    const int results_id = args[0]->Uint32Value();
    const int results_base = SearchBox::Get(render_view)->results_base();
    // Note that stale results_ids, less than the current results_base, will
    // wrap.
    const size_t index = results_id - results_base;
    const std::vector<InstantAutocompleteResult>& suggestions =
        SearchBox::Get(render_view)->autocomplete_results();
    if (index < suggestions.size()) {
      string16 text = UTF8ToUTF16(suggestions[index].destination_url.spec());
      InstantCompleteBehavior behavior = INSTANT_COMPLETE_REPLACE;
      InstantSuggestionType type = INSTANT_SUGGESTION_URL;

      if ((args.Length() >= 2 && args[1]->Uint32Value() == 0) ||
          (args.Length() < 2 && suggestions[index].is_search)) {
        text = suggestions[index].contents;
        type = INSTANT_SUGGESTION_SEARCH;
      }

      std::vector<InstantSuggestion> suggestions;
      suggestions.push_back(InstantSuggestion(text, behavior, type));
      SearchBox::Get(render_view)->SetSuggestions(suggestions);
    } else {
      VLOG(1) << "Invalid results_id " << results_id << "; "
              << "results_base is " << results_base << ".";
    }
  }
  return v8::Undefined();
}

// static
v8::Handle<v8::Value> SearchBoxExtensionWrapper::SetPreviewHeight(
    const v8::Arguments& args) {
  if (args.Length() == 1) {
    int height = 0;
    InstantSizeUnits units = INSTANT_SIZE_PIXELS;
    if (args[0]->IsInt32()) {
      height = args[0]->Int32Value();
    } else if (args[0]->IsString()) {
      std::string height_str = *v8::String::Utf8Value(args[0]);
      std::string units_str;
      SplitLeadingNumberToken(&height_str, &units_str);
      if (!base::StringToInt(height_str, &height))
        return v8::Undefined();
      if (units_str == "%") {
        units = INSTANT_SIZE_PERCENT;
      } else if (!units_str.empty() && units_str != "px") {
        return v8::Undefined();
      }
    } else {
      return v8::Undefined();
    }
    content::RenderView* render_view = GetRenderView();
    if (render_view && height >= 0)
      SearchBox::Get(render_view)->SetInstantPreviewHeight(height, units);
  }
  return v8::Undefined();
}

// static
void Dispatch(WebKit::WebFrame* frame, const WebKit::WebString& script) {
  DCHECK(frame) << "Dispatch requires frame";
  if (!frame) return;
  frame->executeScript(WebKit::WebScriptSource(script));
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
bool SearchBoxExtension::PageSupportsInstant(WebKit::WebFrame* frame) {
  DCHECK(frame) << "PageSupportsInstant requires frame";
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
v8::Extension* SearchBoxExtension::Get() {
  const base::StringPiece code =
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_SEARCHBOX_API, ui::SCALE_FACTOR_NONE);
  return new SearchBoxExtensionWrapper(code);
}

}  // namespace extensions_v8
