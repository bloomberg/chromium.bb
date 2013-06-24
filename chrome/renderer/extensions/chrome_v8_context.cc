// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/chrome_v8_context.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_split.h"
#include "base/values.h"
#include "chrome/common/extensions/api/extension_api.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_set.h"
#include "chrome/common/extensions/features/base_feature_provider.h"
#include "chrome/renderer/extensions/chrome_v8_extension.h"
#include "chrome/renderer/extensions/module_system.h"
#include "chrome/renderer/extensions/user_script_slave.h"
#include "content/public/renderer/render_view.h"
#include "content/public/renderer/v8_value_converter.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebScopedMicrotaskSuppression.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "v8/include/v8.h"

using content::V8ValueConverter;

namespace extensions {

ChromeV8Context::ChromeV8Context(v8::Handle<v8::Context> v8_context,
                                 WebKit::WebFrame* web_frame,
                                 const Extension* extension,
                                 Feature::Context context_type)
    : v8_context_(v8_context),
      web_frame_(web_frame),
      extension_(extension),
      context_type_(context_type),
      safe_builtins_(this) {
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
  if (!is_valid())
    return;
  if (module_system_)
    module_system_->Invalidate();
  web_frame_ = NULL;
  v8_context_.reset();
}

std::string ChromeV8Context::GetExtensionID() const {
  return extension_.get() ? extension_->id() : std::string();
}

content::RenderView* ChromeV8Context::GetRenderView() const {
  if (web_frame_ && web_frame_->view())
    return content::RenderView::FromWebView(web_frame_->view());
  else
    return NULL;
}

GURL ChromeV8Context::GetURL() const {
  return web_frame_ ?
      UserScriptSlave::GetDataSourceURLForFrame(web_frame_) : GURL();
}

v8::Local<v8::Value> ChromeV8Context::CallFunction(
    v8::Handle<v8::Function> function,
    int argc,
    v8::Handle<v8::Value> argv[]) const {
  v8::HandleScope handle_scope;
  v8::Context::Scope scope(v8_context());

  WebKit::WebScopedMicrotaskSuppression suppression;
  if (!is_valid())
    return handle_scope.Close(v8::Undefined());

  v8::Handle<v8::Object> global = v8_context()->Global();
  if (!web_frame_)
    return handle_scope.Close(function->Call(global, argc, argv));
  return handle_scope.Close(
      web_frame_->callFunctionEvenIfScriptDisabled(function,
                                                   global,
                                                   argc,
                                                   argv));
}

bool ChromeV8Context::IsAnyFeatureAvailableToContext(
    const std::string& api_name) {
  return ExtensionAPI::GetSharedInstance()->IsAnyFeatureAvailableToContext(
      api_name,
      context_type_,
      UserScriptSlave::GetDataSourceURLForFrame(web_frame_));
}

Feature::Availability ChromeV8Context::GetAvailability(
    const std::string& api_name) {
  return ExtensionAPI::GetSharedInstance()->IsAvailable(api_name,
                                                        extension_.get(),
                                                        context_type_,
                                                        GetURL());
}

void ChromeV8Context::DispatchOnUnloadEvent() {
  module_system_->CallModuleMethod("unload_event", "dispatch");
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
  return std::string();
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

  v8::Handle<v8::Value> retval = module_system_->CallModuleMethod(
      "sendRequest", "handleResponse", arraysize(argv), argv);

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
