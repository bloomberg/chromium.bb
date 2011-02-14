// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/searchbox_extension.h"

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/string_split.h"
#include "base/stringprintf.h"
#include "chrome/common/render_messages_params.h"
#include "chrome/renderer/render_view.h"
#include "chrome/renderer/searchbox.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebScriptSource.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebString.h"
#include "v8/include/v8.h"

using WebKit::WebFrame;
using WebKit::WebScriptSource;
using WebKit::WebString;
using WebKit::WebView;

namespace extensions_v8 {

static const char kSearchBoxExtensionName[] = "v8/SearchBox";

static const char kSearchBoxExtensionScript[] =
    "var chrome;"
    "if (!chrome)"
    "  chrome = {};"
    "if (!chrome.searchBox) {"
    "  chrome.searchBox = new function() {"
    "    native function GetValue();"
    "    native function GetVerbatim();"
    "    native function GetSelectionStart();"
    "    native function GetSelectionEnd();"
    "    native function GetX();"
    "    native function GetY();"
    "    native function GetWidth();"
    "    native function GetHeight();"
    "    native function SetSuggestions();"
    "    this.__defineGetter__('value', GetValue);"
    "    this.__defineGetter__('verbatim', GetVerbatim);"
    "    this.__defineGetter__('selectionStart', GetSelectionStart);"
    "    this.__defineGetter__('selectionEnd', GetSelectionEnd);"
    "    this.__defineGetter__('x', GetX);"
    "    this.__defineGetter__('y', GetY);"
    "    this.__defineGetter__('width', GetWidth);"
    "    this.__defineGetter__('height', GetHeight);"
    "    this.setSuggestions = function(text) {"
    "      SetSuggestions(text);"
    "    };"
    "    this.onchange = null;"
    "    this.onsubmit = null;"
    "    this.oncancel = null;"
    "    this.onresize = null;"
    "  };"
    "}";

static const char kChangeEventName[] = "chrome.searchBox.onchange";

static const char kSubmitEventName[] = "chrome.searchBox.onsubmit";

static const char kCancelEventName[] = "chrome.searchBox.oncancel";

static const char kResizeEventName[] = "chrome.searchBox.onresize";

// Deprecated API support.
// TODO(tonyg): Remove these when they are no longer used.
// ----------------------------------------------------------------------------
// Script sent as the user is typing and the provider supports instant.
// Params:
// . the text the user typed.
// '46' forces the server to give us verbatim results.
static const char kUserInputScript[] =
    "if (window.chrome.userInput)"
    "  window.chrome.userInput("
    "    window.chrome.searchBox.value,"
    "    window.chrome.searchBox.verbatim ? 46 : 0,"
    "    window.chrome.searchBox.selectionStart);";

// Script sent when the page is committed and the provider supports instant.
// Params:
// . the text the user typed.
// . boolean indicating if the user pressed enter to accept the text.
static const char kUserDoneScript[] =
    "if (window.chrome.userWantsQuery)"
    "  window.chrome.userWantsQuery("
    "    window.chrome.searchBox.value,"
    "    window.chrome.searchBox.verbatim);";

// Script sent when the bounds of the omnibox changes and the provider supports
// instant. The params are the bounds relative to the origin of the preview
// (x, y, width, height).
static const char kSetOmniboxBoundsScript[] =
    "if (window.chrome.setDropdownDimensions)"
    "  window.chrome.setDropdownDimensions("
    "    window.chrome.searchBox.x,"
    "    window.chrome.searchBox.y,"
    "    window.chrome.searchBox.width,"
    "    window.chrome.searchBox.height);";

// We first send this script down to determine if the page supports instant.
static const char kSupportsInstantScript[] =
    "if (window.chrome.sv) true; else false;";

// The google.y.first array is a list of functions which are to be executed
// after the external JavaScript used by Google web search loads. The deprecated
// API requires setDropdownDimensions and userInput to be invoked after
// the external JavaScript loads. So if they are not already registered, we add
// them to the array of functions the page will execute after load. This tight
// coupling discourages proliferation of the deprecated API.
static const char kInitScript[] =
    "(function() {"
    "var initScript = function(){%s%s};"
    "if (window.chrome.setDropdownDimensions)"
    "  initScript();"
    "else if (window.google && window.google.y)"
    "  window.google.y.first.push(initScript);"
    "})();";
// ----------------------------------------------------------------------------

class SearchBoxExtensionWrapper : public v8::Extension {
 public:
  SearchBoxExtensionWrapper();

  // Allows v8's javascript code to call the native functions defined
  // in this class for window.chrome.
  virtual v8::Handle<v8::FunctionTemplate> GetNativeFunction(
      v8::Handle<v8::String> name);

  // Helper function to find the RenderView. May return NULL.
  static RenderView* GetRenderView();

  // Gets the value of the user's search query.
  static v8::Handle<v8::Value> GetValue(const v8::Arguments& args);

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

  // Sets ordered suggestions. Valid for current |value|.
  static v8::Handle<v8::Value> SetSuggestions(const v8::Arguments& args);

 private:
  DISALLOW_COPY_AND_ASSIGN(SearchBoxExtensionWrapper);
};

SearchBoxExtensionWrapper::SearchBoxExtensionWrapper()
    : v8::Extension(kSearchBoxExtensionName, kSearchBoxExtensionScript) {}

v8::Handle<v8::FunctionTemplate> SearchBoxExtensionWrapper::GetNativeFunction(
    v8::Handle<v8::String> name) {
  if (name->Equals(v8::String::New("GetValue"))) {
    return v8::FunctionTemplate::New(GetValue);
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
  } else if (name->Equals(v8::String::New("SetSuggestions"))) {
    return v8::FunctionTemplate::New(SetSuggestions);
  }
  return v8::Handle<v8::FunctionTemplate>();
}

// static
RenderView* SearchBoxExtensionWrapper::GetRenderView() {
  WebFrame* webframe = WebFrame::frameForEnteredContext();
  DCHECK(webframe) << "There should be an active frame since we just got "
      "a native function called.";
  if (!webframe) return NULL;

  WebView* webview = webframe->view();
  if (!webview) return NULL;  // can happen during closing

  return RenderView::FromWebView(webview);
}

// static
v8::Handle<v8::Value> SearchBoxExtensionWrapper::GetValue(
    const v8::Arguments& args) {
  RenderView* render_view = GetRenderView();
  if (!render_view) return v8::Undefined();
  return v8::String::New(
      reinterpret_cast<const uint16_t*>(
          render_view->searchbox()->value().c_str()),
      render_view->searchbox()->value().length());
}

// static
v8::Handle<v8::Value> SearchBoxExtensionWrapper::GetVerbatim(
    const v8::Arguments& args) {
  RenderView* render_view = GetRenderView();
  if (!render_view) return v8::Undefined();
  return v8::Boolean::New(render_view->searchbox()->verbatim());
}

// static
v8::Handle<v8::Value> SearchBoxExtensionWrapper::GetSelectionStart(
    const v8::Arguments& args) {
  RenderView* render_view = GetRenderView();
  if (!render_view) return v8::Undefined();
  return v8::Int32::New(render_view->searchbox()->selection_start());
}

// static
v8::Handle<v8::Value> SearchBoxExtensionWrapper::GetSelectionEnd(
    const v8::Arguments& args) {
  RenderView* render_view = GetRenderView();
  if (!render_view) return v8::Undefined();
  return v8::Int32::New(render_view->searchbox()->selection_end());
}

// static
v8::Handle<v8::Value> SearchBoxExtensionWrapper::GetX(
    const v8::Arguments& args) {
  RenderView* render_view = GetRenderView();
  if (!render_view) return v8::Undefined();
  return v8::Int32::New(render_view->searchbox()->rect().x());
}

// static
v8::Handle<v8::Value> SearchBoxExtensionWrapper::GetY(
    const v8::Arguments& args) {
  RenderView* render_view = GetRenderView();
  if (!render_view) return v8::Undefined();
  return v8::Int32::New(render_view->searchbox()->rect().y());
}

// static
v8::Handle<v8::Value> SearchBoxExtensionWrapper::GetWidth(
    const v8::Arguments& args) {
  RenderView* render_view = GetRenderView();
  if (!render_view) return v8::Undefined();
  return v8::Int32::New(render_view->searchbox()->rect().width());
}

// static
v8::Handle<v8::Value> SearchBoxExtensionWrapper::GetHeight(
    const v8::Arguments& args) {
  RenderView* render_view = GetRenderView();
  if (!render_view) return v8::Undefined();
  return v8::Int32::New(render_view->searchbox()->rect().height());
}

// Accepts a single argument in form:
// {
//   suggestions: [
//     {
//       value: "..."
//     }
//   ]
// }
// static
v8::Handle<v8::Value> SearchBoxExtensionWrapper::SetSuggestions(
    const v8::Arguments& args) {
  std::vector<std::string> suggestions;

  if (args.Length() && args[0]->IsArray()) {
    // For backwards compatibility, also accept an array of strings.
    // TODO(tonyg): Remove this when it is confirmed to be unused.
    v8::Array* suggestions_array = static_cast<v8::Array*>(*args[0]);
    uint32_t length = suggestions_array->Length();
    for (uint32_t i = 0; i < length; i++) {
      std::string suggestion = *v8::String::Utf8Value(
          suggestions_array->Get(v8::Integer::New(i))->ToString());
      if (!suggestion.length()) continue;
      suggestions.push_back(suggestion);
    }
  } else if (args.Length() && args[0]->IsObject()) {
    // Standard version, object argument.
    v8::Object* suggestion_json = static_cast<v8::Object*>(*args[0]);
    v8::Local<v8::Value> suggestions_field =
        suggestion_json->Get(v8::String::New("suggestions"));

    if (suggestions_field->IsArray()) {
      v8::Local<v8::Array> suggestions_array =
          suggestions_field.As<v8::Array>();

      uint32_t length = suggestions_array->Length();
      for (uint32_t i = 0; i < length; i++) {
        v8::Local<v8::Value> suggestion_value =
            suggestions_array->Get(v8::Integer::New(i));
        if (!suggestion_value->IsObject()) continue;

        v8::Local<v8::Object> suggestion_object =
            suggestion_value.As<v8::Object>();
        v8::Local<v8::Value> suggestion_object_value =
            suggestion_object->Get(v8::String::New("value"));
        if (!suggestion_object_value->IsString()) continue;

        std::string suggestion = *v8::String::Utf8Value(
            suggestion_object_value->ToString());
        if (!suggestion.length()) continue;
        suggestions.push_back(suggestion);
      }
    }
  }

  if (RenderView* render_view = GetRenderView())
    render_view->searchbox()->SetSuggestions(suggestions);
  return v8::Undefined();
}

// static
bool Dispatch(WebFrame* frame, const std::string& event_name) {
  DCHECK(frame) << "Dispatch requires frame";
  if (!frame) return false;

  v8::HandleScope handle_scope;
  v8::Local<v8::Context> context = frame->mainWorldScriptContext();
  v8::Context::Scope context_scope(context);

  v8::Local<v8::Value> value =
      context->Global()->Get(v8::String::New("window"));
  std::vector<std::string> components;
  base::SplitStringDontTrim(event_name, '.', &components);
  for (size_t i = 0; i < components.size(); ++i) {
    if (!value.IsEmpty() && value->IsObject())
      value = value->ToObject()->Get(v8::String::New(components[i].c_str()));
  }
  if (value.IsEmpty() || !value->IsFunction())
    return false;

  v8::Local<v8::Function> function = v8::Local<v8::Function>::Cast(value);
  if (function.IsEmpty())
    return false;

  function->Call(v8::Object::New(), 0, NULL);
  return true;
}

// static
void SearchBoxExtension::DispatchChange(WebFrame* frame) {
  if (Dispatch(frame, kChangeEventName))
    return;
  frame->executeScript(WebScriptSource(kUserInputScript));
}

// static
void SearchBoxExtension::DispatchSubmit(WebFrame* frame) {
  if (Dispatch(frame, kSubmitEventName))
    return;
  frame->executeScript(WebScriptSource(kUserDoneScript));
}

// static
void SearchBoxExtension::DispatchCancel(WebFrame* frame) {
  if (Dispatch(frame, kCancelEventName))
    return;
  frame->executeScript(WebScriptSource(kUserDoneScript));
}

// static
void SearchBoxExtension::DispatchResize(WebFrame* frame) {
  if (Dispatch(frame, kResizeEventName))
    return;
  frame->executeScript(WebScriptSource(kSetOmniboxBoundsScript));
}

// static
bool SearchBoxExtension::PageSupportsInstant(WebFrame* frame) {
  DCHECK(frame) << "PageSupportsInstant requires frame";
  if (!frame) return false;

  bool supports_deprecated_api = frame->executeScriptAndReturnValue(
      WebScriptSource(kSupportsInstantScript))->BooleanValue();
  // TODO(tonyg): Add way of detecting instant support to SearchBox API.
  bool supports_searchbox_api = supports_deprecated_api;

  // The deprecated API needs to notify the page of events it may have missed.
  // This isn't necessary in the SearchBox API, since the page can query the
  // API at any time.
  static std::string init_script(
      StringPrintf(kInitScript, kSetOmniboxBoundsScript, kUserInputScript));
  if (supports_deprecated_api) {
    frame->executeScript(WebScriptSource(WebString::fromUTF8(init_script)));
  }

  return supports_searchbox_api || supports_deprecated_api;
}

// static
v8::Extension* SearchBoxExtension::Get() {
  return new SearchBoxExtensionWrapper();
}

}  // namespace extensions_v8
