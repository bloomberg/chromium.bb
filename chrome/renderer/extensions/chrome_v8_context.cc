// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/chrome_v8_context.h"

#include "base/logging.h"
#include "base/string_split.h"
#include "base/values.h"
#include "chrome/common/extensions/extension_set.h"
#include "chrome/renderer/extensions/chrome_v8_extension.h"
#include "content/public/renderer/render_view.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "v8/include/v8.h"

ChromeV8Context::ChromeV8Context(v8::Handle<v8::Context> v8_context,
                                 WebKit::WebFrame* web_frame,
                                 const std::string& extension_id)
    : v8_context_(v8::Persistent<v8::Context>::New(v8_context)),
      web_frame_(web_frame),
      extension_id_(extension_id) {
  VLOG(1) << "Created context for extension\n"
          << "  id:    " << extension_id << "\n"
          << "  frame: " << web_frame_;
}

ChromeV8Context::~ChromeV8Context() {
  VLOG(1) << "Destroyed context for extension\n"
          << "  id:    " << extension_id_;
  v8_context_.Dispose();
}

content::RenderView* ChromeV8Context::GetRenderView() const {
  if (web_frame_ && web_frame_->view())
    return content::RenderView::FromWebView(web_frame_->view());
  else
    return NULL;
}

bool ChromeV8Context::CallChromeHiddenMethod(
    const std::string& function_name,
    int argc,
    v8::Handle<v8::Value>* argv,
    v8::Handle<v8::Value>* result) const {
  v8::Context::Scope context_scope(v8_context_);
  v8::TryCatch try_catch;

  // Look up the function name, which may be a sub-property like
  // "Port.dispatchOnMessage" in the hidden global variable.
  v8::Local<v8::Value> value = v8::Local<v8::Value>::New(
      ChromeV8Extension::GetChromeHidden(v8_context_));
  std::vector<std::string> components;
  base::SplitStringDontTrim(function_name, '.', &components);
  for (size_t i = 0; i < components.size(); ++i) {
    if (!value.IsEmpty() && value->IsObject()) {
      value = v8::Local<v8::Object>::Cast(value)->Get(
          v8::String::New(components[i].c_str()));
      if (try_catch.HasCaught()) {
        NOTREACHED() << *v8::String::AsciiValue(try_catch.Exception());
        return false;
      }
    }
  }

  if (value.IsEmpty() || !value->IsFunction()) {
    VLOG(1) << "Could not execute chrome hidden method: " << function_name;
    return false;
  }

  v8::Handle<v8::Value> result_temp =
      v8::Local<v8::Function>::Cast(value)->Call(v8::Object::New(), argc, argv);
  if (try_catch.HasCaught()) {
    NOTREACHED() << *v8::String::AsciiValue(try_catch.Exception());
    return false;
  }
  if (result)
    *result = result_temp;
  return true;
}

void ChromeV8Context::DispatchOnLoadEvent(bool is_extension_process,
                                          bool is_incognito_process) const {
  v8::HandleScope handle_scope;
  v8::Handle<v8::Value> argv[3];
  argv[0] = v8::String::New(extension_id_.c_str());
  argv[1] = v8::Boolean::New(is_extension_process);
  argv[2] = v8::Boolean::New(is_incognito_process);
  CallChromeHiddenMethod("dispatchOnLoad", arraysize(argv), argv, NULL);
}

void ChromeV8Context::DispatchOnUnloadEvent() const {
  v8::HandleScope handle_scope;
  CallChromeHiddenMethod("dispatchOnUnload", 0, NULL, NULL);
}
