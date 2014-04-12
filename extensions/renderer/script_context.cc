// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/script_context.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_split.h"
#include "base/values.h"
#include "content/public/renderer/render_view.h"
#include "content/public/renderer/v8_value_converter.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_api.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/features/base_feature_provider.h"
#include "third_party/WebKit/public/web/WebDataSource.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebScopedMicrotaskSuppression.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "v8/include/v8.h"

using content::V8ValueConverter;

namespace extensions {

ScriptContext::ScriptContext(v8::Handle<v8::Context> v8_context,
                             blink::WebFrame* web_frame,
                             const Extension* extension,
                             Feature::Context context_type)
    : v8_context_(v8_context),
      web_frame_(web_frame),
      extension_(extension),
      context_type_(context_type),
      safe_builtins_(this),
      isolate_(v8_context->GetIsolate()) {
  VLOG(1) << "Created context:\n"
          << "  extension id: " << GetExtensionID() << "\n"
          << "  frame:        " << web_frame_ << "\n"
          << "  context type: " << GetContextTypeDescription();
}

ScriptContext::~ScriptContext() {
  VLOG(1) << "Destroyed context for extension\n"
          << "  extension id: " << GetExtensionID();
  Invalidate();
}

void ScriptContext::Invalidate() {
  if (!is_valid())
    return;
  if (module_system_)
    module_system_->Invalidate();
  web_frame_ = NULL;
  v8_context_.reset();
}

std::string ScriptContext::GetExtensionID() const {
  return extension_.get() ? extension_->id() : std::string();
}

content::RenderView* ScriptContext::GetRenderView() const {
  if (web_frame_ && web_frame_->view())
    return content::RenderView::FromWebView(web_frame_->view());
  else
    return NULL;
}

v8::Local<v8::Value> ScriptContext::CallFunction(
    v8::Handle<v8::Function> function,
    int argc,
    v8::Handle<v8::Value> argv[]) const {
  v8::EscapableHandleScope handle_scope(isolate());
  v8::Context::Scope scope(v8_context());

  blink::WebScopedMicrotaskSuppression suppression;
  if (!is_valid()) {
    return handle_scope.Escape(
        v8::Local<v8::Primitive>(v8::Undefined(isolate())));
  }

  v8::Handle<v8::Object> global = v8_context()->Global();
  if (!web_frame_)
    return handle_scope.Escape(function->Call(global, argc, argv));
  return handle_scope.Escape(
      v8::Local<v8::Value>(web_frame_->callFunctionEvenIfScriptDisabled(
          function, global, argc, argv)));
}

Feature::Availability ScriptContext::GetAvailability(
    const std::string& api_name) {
  // Hack: Hosted apps should have the availability of messaging APIs based on
  // the URL of the page (which might have access depending on some extension
  // with externally_connectable), not whether the app has access to messaging
  // (which it won't).
  const Extension* extension = extension_.get();
  if (extension && extension->is_hosted_app() &&
      (api_name == "runtime.connect" || api_name == "runtime.sendMessage")) {
    extension = NULL;
  }
  return ExtensionAPI::GetSharedInstance()->IsAvailable(
      api_name, extension, context_type_, GetURL());
}

void ScriptContext::DispatchEvent(const char* event_name,
                                  v8::Handle<v8::Array> args) const {
  v8::HandleScope handle_scope(isolate());
  v8::Context::Scope context_scope(v8_context());

  v8::Handle<v8::Value> argv[] = {
      v8::String::NewFromUtf8(isolate(), event_name), args};
  module_system_->CallModuleMethod(
      kEventBindings, "dispatchEvent", arraysize(argv), argv);
}

void ScriptContext::DispatchOnUnloadEvent() {
  module_system_->CallModuleMethod("unload_event", "dispatch");
}

std::string ScriptContext::GetContextTypeDescription() {
  switch (context_type_) {
    case Feature::UNSPECIFIED_CONTEXT:
      return "UNSPECIFIED";
    case Feature::BLESSED_EXTENSION_CONTEXT:
      return "BLESSED_EXTENSION";
    case Feature::UNBLESSED_EXTENSION_CONTEXT:
      return "UNBLESSED_EXTENSION";
    case Feature::CONTENT_SCRIPT_CONTEXT:
      return "CONTENT_SCRIPT";
    case Feature::WEB_PAGE_CONTEXT:
      return "WEB_PAGE";
    case Feature::BLESSED_WEB_PAGE_CONTEXT:
      return "BLESSED_WEB_PAGE";
  }
  NOTREACHED();
  return std::string();
}

GURL ScriptContext::GetURL() const {
  return web_frame() ? GetDataSourceURLForFrame(web_frame()) : GURL();
}

bool ScriptContext::IsAnyFeatureAvailableToContext(const Feature& api) {
  return ExtensionAPI::GetSharedInstance()->IsAnyFeatureAvailableToContext(
      api, extension(), context_type(), GetDataSourceURLForFrame(web_frame()));
}

// static
GURL ScriptContext::GetDataSourceURLForFrame(const blink::WebFrame* frame) {
  // Normally we would use frame->document().url() to determine the document's
  // URL, but to decide whether to inject a content script, we use the URL from
  // the data source. This "quirk" helps prevents content scripts from
  // inadvertently adding DOM elements to the compose iframe in Gmail because
  // the compose iframe's dataSource URL is about:blank, but the document URL
  // changes to match the parent document after Gmail document.writes into
  // it to create the editor.
  // http://code.google.com/p/chromium/issues/detail?id=86742
  blink::WebDataSource* data_source = frame->provisionalDataSource()
                                          ? frame->provisionalDataSource()
                                          : frame->dataSource();
  CHECK(data_source);
  return GURL(data_source->request().url());
}

ScriptContext* ScriptContext::GetContext() { return this; }

void ScriptContext::OnResponseReceived(const std::string& name,
                                       int request_id,
                                       bool success,
                                       const base::ListValue& response,
                                       const std::string& error) {
  v8::HandleScope handle_scope(isolate());

  scoped_ptr<V8ValueConverter> converter(V8ValueConverter::create());
  v8::Handle<v8::Value> argv[] = {
      v8::Integer::New(isolate(), request_id),
      v8::String::NewFromUtf8(isolate(), name.c_str()),
      v8::Boolean::New(isolate(), success),
      converter->ToV8Value(&response, v8_context_.NewHandle(isolate())),
      v8::String::NewFromUtf8(isolate(), error.c_str())};

  v8::Handle<v8::Value> retval = module_system()->CallModuleMethod(
      "sendRequest", "handleResponse", arraysize(argv), argv);

  // In debug, the js will validate the callback parameters and return a
  // string if a validation error has occured.
  DCHECK(retval.IsEmpty() || retval->IsUndefined())
      << *v8::String::Utf8Value(retval);
}

}  // namespace extensions
