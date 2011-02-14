// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/search_extension.h"

#include <string>
#include <vector>

#include "base/command_line.h"
#include "chrome/common/render_messages_params.h"
#include "chrome/renderer/render_view.h"
#include "chrome/renderer/searchbox.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "v8/include/v8.h"

using WebKit::WebFrame;
using WebKit::WebView;

namespace extensions_v8 {

const char* const kSearchExtensionName = "v8/InstantSearch";

class SearchExtensionWrapper : public v8::Extension {
 public:
  SearchExtensionWrapper();

  // Allows v8's javascript code to call the native functions defined
  // in this class for window.chrome.
  virtual v8::Handle<v8::FunctionTemplate> GetNativeFunction(
      v8::Handle<v8::String> name);

  // Helper function to find the RenderView. May return NULL.
  static RenderView* GetRenderView();

  // Implementation of window.chrome.setSuggestResult.
  static v8::Handle<v8::Value> SetSuggestResult(const v8::Arguments& args);

 private:
  DISALLOW_COPY_AND_ASSIGN(SearchExtensionWrapper);
};

SearchExtensionWrapper::SearchExtensionWrapper()
    : v8::Extension(kSearchExtensionName,
      "var chrome;"
      "if (!chrome)"
      "  chrome = {};"
      "chrome.setSuggestResult = function(text) {"
      "  native function NativeSetSuggestResult();"
      "  NativeSetSuggestResult(text);"
      "};") {
}

v8::Handle<v8::FunctionTemplate> SearchExtensionWrapper::GetNativeFunction(
    v8::Handle<v8::String> name) {
  if (name->Equals(v8::String::New("NativeSetSuggestResult"))) {
    return v8::FunctionTemplate::New(SetSuggestResult);
  }

  return v8::Handle<v8::FunctionTemplate>();
}

// static
RenderView* SearchExtensionWrapper::GetRenderView() {
  WebFrame* webframe = WebFrame::frameForEnteredContext();
  DCHECK(webframe) << "There should be an active frame since we just got "
      "a native function called.";
  if (!webframe) return NULL;

  WebView* webview = webframe->view();
  if (!webview) return NULL;  // can happen during closing

  return RenderView::FromWebView(webview);
}

// static
v8::Handle<v8::Value> SearchExtensionWrapper::SetSuggestResult(
    const v8::Arguments& args) {
  if (!args.Length() || !args[0]->IsString()) return v8::Undefined();

  RenderView* render_view = GetRenderView();
  if (!render_view) return v8::Undefined();

  std::vector<std::string> suggestions;
  suggestions.push_back(std::string(*v8::String::Utf8Value(args[0])));
  render_view->searchbox()->SetSuggestions(suggestions);
  return v8::Undefined();
}

v8::Extension* SearchExtension::Get() {
  return new SearchExtensionWrapper();
}

}  // namespace extensions_v8
