// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/chrome_v8_context.h"

#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_split.h"
#include "base/values.h"
#include "chrome/common/extensions/api/extension_api.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_set.h"
#include "chrome/renderer/extensions/chrome_v8_extension.h"
#include "chrome/renderer/extensions/module_system.h"
#include "chrome/renderer/extensions/user_script_slave.h"
#include "content/public/renderer/render_view.h"
#include "content/public/renderer/v8_value_converter.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "v8/include/v8.h"

using content::V8ValueConverter;

namespace extensions {

namespace {

const char kChromeHidden[] = "chromeHidden";
const char kValidateCallbacks[] = "validateCallbacks";
const char kValidateAPI[] = "validateAPI";

}  // namespace

ChromeV8Context::ChromeV8Context(v8::Handle<v8::Context> v8_context,
                                 WebKit::WebFrame* web_frame,
                                 const Extension* extension,
                                 Feature::Context context_type)
    : v8_context_(v8_context),
      web_frame_(web_frame),
      extension_(extension),
      context_type_(context_type) {
  VLOG(1) << "Created context:\n"
          << "  extension id: " << GetExtensionID() << "\n"
          << "  frame:        " << web_frame_ << "\n"
          << "  context type: " << GetContextTypeDescription();
}

ChromeV8Context::~ChromeV8Context() {
  VLOG(1) << "Destroyed context for extension\n"
          << "  extension id: " << GetExtensionID();
  Invalidate();
}

void ChromeV8Context::Invalidate() {
  if (v8_context_.get().IsEmpty())
    return;
  if (module_system_)
    module_system_->Invalidate();
  web_frame_ = NULL;
  v8_context_.reset();
}

std::string ChromeV8Context::GetExtensionID() {
  return extension_ ? extension_->id() : "";
}

// static
v8::Handle<v8::Value> ChromeV8Context::GetOrCreateChromeHidden(
    v8::Handle<v8::Context> context) {
  v8::Local<v8::Object> global = context->Global();
  v8::Local<v8::Value> hidden = global->GetHiddenValue(
      v8::String::New(kChromeHidden));

  if (hidden.IsEmpty() || hidden->IsUndefined()) {
    hidden = v8::Object::New();
    global->SetHiddenValue(v8::String::New(kChromeHidden), hidden);

    if (DCHECK_IS_ON()) {
      // Tell bindings.js to validate callbacks and events against their schema
      // definitions.
      v8::Local<v8::Object>::Cast(hidden)->Set(
          v8::String::New(kValidateCallbacks), v8::True());
      // Tell bindings.js to validate API for ambiguity.
      v8::Local<v8::Object>::Cast(hidden)->Set(
          v8::String::New(kValidateAPI), v8::True());
    }
  }

  DCHECK(hidden->IsObject());
  return v8::Local<v8::Object>::Cast(hidden);
}

v8::Handle<v8::Value> ChromeV8Context::GetChromeHidden() const {
  v8::Local<v8::Object> global = v8_context_->Global();
  return global->GetHiddenValue(v8::String::New(kChromeHidden));
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
  // ChromeV8ContextSet calls Invalidate() and then schedules a task to delete
  // this object. This check prevents a race from attempting to execute script
  // on a NULL web_frame_.
  if (!web_frame_)
    return false;

  v8::Context::Scope context_scope(v8_context_.get());

  // Look up the function name, which may be a sub-property like
  // "Port.dispatchOnMessage" in the hidden global variable.
  v8::Local<v8::Value> value = v8::Local<v8::Value>::New(GetChromeHidden());
  if (value.IsEmpty())
    return false;

  std::vector<std::string> components;
  base::SplitStringDontTrim(function_name, '.', &components);
  for (size_t i = 0; i < components.size(); ++i) {
    if (!value.IsEmpty() && value->IsObject()) {
      value = v8::Local<v8::Object>::Cast(value)->Get(
          v8::String::New(components[i].c_str()));
    }
  }

  if (value.IsEmpty() || !value->IsFunction()) {
    VLOG(1) << "Could not execute chrome hidden method: " << function_name;
    return false;
  }

  TRACE_EVENT1("v8", "v8.callChromeHiddenMethod",
               "function_name", function_name);

  v8::Local<v8::Function> function = v8::Local<v8::Function>::Cast(value);
  v8::Handle<v8::Value> result_temp =
      web_frame_->callFunctionEvenIfScriptDisabled(function,
                                                   v8::Object::New(),
                                                   argc,
                                                   argv);
  if (result)
    *result = result_temp;

  return true;
}

Feature::Availability ChromeV8Context::GetAvailability(
    const std::string& api_name) {
  return ExtensionAPI::GetSharedInstance()->IsAvailable(
      api_name,
      extension_,
      context_type_,
      UserScriptSlave::GetDataSourceURLForFrame(web_frame_));
}

void ChromeV8Context::DispatchOnLoadEvent(bool is_incognito_process,
                                          int manifest_version) {
  v8::HandleScope handle_scope;
  v8::Handle<v8::Value> argv[] = {
    v8::String::New(GetExtensionID().c_str()),
    v8::String::New(GetContextTypeDescription().c_str()),
    v8::Boolean::New(is_incognito_process),
    v8::Integer::New(manifest_version),
  };
  CallChromeHiddenMethod("dispatchOnLoad", arraysize(argv), argv, NULL);
}

void ChromeV8Context::DispatchOnUnloadEvent() {
  v8::HandleScope handle_scope;
  CallChromeHiddenMethod("dispatchOnUnload", 0, NULL, NULL);
}

std::string ChromeV8Context::GetContextTypeDescription() {
  switch (context_type_) {
    case Feature::UNSPECIFIED_CONTEXT:         return "UNSPECIFIED";
    case Feature::BLESSED_EXTENSION_CONTEXT:   return "BLESSED_EXTENSION";
    case Feature::UNBLESSED_EXTENSION_CONTEXT: return "UNBLESSED_EXTENSION";
    case Feature::CONTENT_SCRIPT_CONTEXT:      return "CONTENT_SCRIPT";
    case Feature::WEB_PAGE_CONTEXT:            return "WEB_PAGE";
  }
  NOTREACHED();
  return "";
}

ChromeV8Context* ChromeV8Context::GetContext() {
  return this;
}

void ChromeV8Context::OnResponseReceived(const std::string& name,
                                         int request_id,
                                         bool success,
                                         const base::ListValue& response,
                                         const std::string& error) {
  v8::HandleScope handle_scope;

  scoped_ptr<V8ValueConverter> converter(V8ValueConverter::create());
  v8::Handle<v8::Value> argv[] = {
    v8::Integer::New(request_id),
    v8::String::New(name.c_str()),
    v8::Boolean::New(success),
    converter->ToV8Value(&response, v8_context_.get()),
    v8::String::New(error.c_str())
  };

  v8::Handle<v8::Value> retval;
  CHECK(CallChromeHiddenMethod("handleResponse", arraysize(argv), argv,
                               &retval));
  // In debug, the js will validate the callback parameters and return a
  // string if a validation error has occured.
  if (DCHECK_IS_ON()) {
    if (!retval.IsEmpty() && !retval->IsUndefined()) {
      std::string error = *v8::String::AsciiValue(retval);
      DCHECK(false) << error;
    }
  }
}

}  // namespace extensions
