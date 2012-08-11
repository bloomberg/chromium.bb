// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/searchbox/searchbox_extension.h"

#include "chrome/renderer/searchbox/searchbox.h"
#include "content/public/renderer/render_view.h"
#include "grit/renderer_resources.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebScriptSource.h"
#include "ui/base/resource/resource_bundle.h"
#include "v8/include/v8.h"

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

SearchBoxExtensionWrapper::SearchBoxExtensionWrapper(
    const base::StringPiece& code)
    : v8::Extension(kSearchBoxExtensionName, code.data(), 0, 0, code.size()) {
}

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
v8::Handle<v8::Value> SearchBoxExtensionWrapper::GetValue(
    const v8::Arguments& args) {
  content::RenderView* render_view = GetRenderView();
  if (!render_view) return v8::Undefined();
  return v8::String::New(
      reinterpret_cast<const uint16_t*>(
          SearchBox::Get(render_view)->value().data()),
      SearchBox::Get(render_view)->value().length());
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
v8::Handle<v8::Value> SearchBoxExtensionWrapper::SetSuggestions(
    const v8::Arguments& args) {
  std::vector<string16> suggestions;
  InstantCompleteBehavior behavior = INSTANT_COMPLETE_NOW;

  if (args.Length() && args[0]->IsObject()) {
    v8::Local<v8::Object> suggestion_json = args[0]->ToObject();

    v8::Local<v8::Value> complete_value =
        suggestion_json->Get(v8::String::New("complete_behavior"));
    if (complete_value->IsString()) {
      if (complete_value->Equals(v8::String::New("now"))) {
        behavior = INSTANT_COMPLETE_NOW;
      } else if (complete_value->Equals(v8::String::New("never"))) {
        behavior = INSTANT_COMPLETE_NEVER;
      } else if (complete_value->Equals(v8::String::New("delayed"))) {
        behavior = INSTANT_COMPLETE_DELAYED;
      } else {
        VLOG(1) << "Unsupported complete behavior '"
                << *v8::String::Utf8Value(complete_value) << "'";
      }
    }

    v8::Local<v8::Value> suggestions_field =
        suggestion_json->Get(v8::String::New("suggestions"));

    if (suggestions_field->IsArray()) {
      v8::Local<v8::Array> suggestions_array =
          suggestions_field.As<v8::Array>();

      size_t length = suggestions_array->Length();
      for (size_t i = 0; i < length; i++) {
        v8::Local<v8::Value> suggestion_value = suggestions_array->Get(i);
        if (!suggestion_value->IsObject()) continue;

        v8::Local<v8::Object> suggestion_object = suggestion_value->ToObject();
        v8::Local<v8::Value> suggestion_object_value =
            suggestion_object->Get(v8::String::New("value"));
        if (!suggestion_object_value->IsString()) continue;

        string16 suggestion(reinterpret_cast<char16*>(*v8::String::Value(
            suggestion_object_value->ToString())));
        suggestions.push_back(suggestion);
      }
    }
  }

  if (content::RenderView* render_view = GetRenderView())
    SearchBox::Get(render_view)->SetSuggestions(suggestions, behavior);
  return v8::Undefined();
}

// static
void Dispatch(WebKit::WebFrame* frame, WebKit::WebString script) {
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
