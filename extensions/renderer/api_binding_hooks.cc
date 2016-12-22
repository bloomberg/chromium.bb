// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api_binding_hooks.h"

#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/supports_user_data.h"
#include "gin/arguments.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "gin/per_context_data.h"
#include "gin/wrappable.h"

namespace extensions {

namespace {

// An interface to allow for registration of custom hooks from JavaScript.
// Contains registered hooks for a single API.
class JSHookInterface final : public gin::Wrappable<JSHookInterface> {
 public:
  using JSHooks = std::map<std::string, v8::Global<v8::Function>>;

  explicit JSHookInterface(const std::string& api_name)
      : api_name_(api_name) {}

  static gin::WrapperInfo kWrapperInfo;

  // gin::Wrappable:
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override {
    return Wrappable<JSHookInterface>::GetObjectTemplateBuilder(isolate)
        .SetMethod("setHandleRequest", &JSHookInterface::SetHandleRequest);
  }

  JSHooks* js_hooks() { return &js_hooks_; }

 private:
  // Adds a custom hook.
  void SetHandleRequest(v8::Isolate* isolate,
                        const std::string& method_name,
                        v8::Local<v8::Function> handler) {
    std::string qualified_method_name =
        base::StringPrintf("%s.%s", api_name_.c_str(), method_name.c_str());
    v8::Global<v8::Function>& entry = js_hooks_[qualified_method_name];
    if (!entry.IsEmpty()) {
      NOTREACHED() << "Hooks can only be set once.";
      return;
    }
    entry.Reset(isolate, handler);
  }

  std::string api_name_;

  JSHooks js_hooks_;

  DISALLOW_COPY_AND_ASSIGN(JSHookInterface);
};

const char kExtensionAPIHooksPerContextKey[] = "extension_api_hooks";

struct APIHooksPerContextData : public base::SupportsUserData::Data {
  APIHooksPerContextData(v8::Isolate* isolate) : isolate(isolate) {}
  ~APIHooksPerContextData() override {
    v8::HandleScope scope(isolate);
    for (const auto& pair : hook_interfaces) {
      // We explicitly clear the hook data map here to remove all references to
      // v8 objects in order to avoid cycles.
      JSHookInterface* hooks = nullptr;
      gin::Converter<JSHookInterface*>::FromV8(
          isolate, pair.second.Get(isolate), &hooks);
      CHECK(hooks);
      hooks->js_hooks()->clear();
    }
  }

  v8::Isolate* isolate;

  std::map<std::string, v8::Global<v8::Object>> hook_interfaces;
};

gin::WrapperInfo JSHookInterface::kWrapperInfo =
    {gin::kEmbedderNativeGin};

}  // namespace

APIBindingHooks::APIBindingHooks(const binding::RunJSFunction& run_js)
    : run_js_(run_js) {}
APIBindingHooks::~APIBindingHooks() {}

void APIBindingHooks::RegisterHandleRequest(const std::string& method_name,
                                            const HandleRequestHook& hook) {
  DCHECK(!hooks_used_) << "Hooks must be registered before the first use!";
  request_hooks_[method_name] = hook;
}

void APIBindingHooks::RegisterJsSource(v8::Global<v8::String> source,
                                       v8::Global<v8::String> resource_name) {
  js_hooks_source_ = std::move(source);
  js_resource_name_ = std::move(resource_name);
}

bool APIBindingHooks::HandleRequest(const std::string& api_name,
                                    const std::string& method_name,
                                    v8::Local<v8::Context> context,
                                    const APISignature* signature,
                                    gin::Arguments* arguments) {
  // Easy case: a native custom hook.
  auto request_hooks_iter = request_hooks_.find(method_name);
  if (request_hooks_iter != request_hooks_.end()) {
    request_hooks_iter->second.Run(signature, arguments);
    return true;
  }

  // Harder case: looking up a custom hook registered on the context (since
  // these are JS, each context has a separate instance).
  gin::PerContextData* per_context_data = gin::PerContextData::From(context);
  DCHECK(per_context_data);
  APIHooksPerContextData* data = static_cast<APIHooksPerContextData*>(
      per_context_data->GetUserData(kExtensionAPIHooksPerContextKey));
  if (!data)
    return false;

  auto hook_interface_iter = data->hook_interfaces.find(api_name);
  if (hook_interface_iter == data->hook_interfaces.end())
    return false;

  JSHookInterface* hook_interface = nullptr;
  gin::Converter<JSHookInterface*>::FromV8(
      context->GetIsolate(),
      hook_interface_iter->second.Get(context->GetIsolate()), &hook_interface);
  CHECK(hook_interface);

  auto js_hook_iter = hook_interface->js_hooks()->find(method_name);
  if (js_hook_iter == hook_interface->js_hooks()->end())
    return false;

  // Found a JS handler.
  std::vector<v8::Local<v8::Value>> v8_args;
  // TODO(devlin): Right now, this doesn't support exceptions or return values,
  // which we will need to at some point.
  if (arguments->Length() == 0 || arguments->GetRemaining(&v8_args)) {
    v8::Local<v8::Function> handler =
        js_hook_iter->second.Get(context->GetIsolate());
    run_js_.Run(handler, context, v8_args.size(), v8_args.data());
  }

  return true;
}

void APIBindingHooks::InitializeInContext(
    v8::Local<v8::Context> context,
    const std::string& api_name) {
  if (js_hooks_source_.IsEmpty())
    return;

  v8::Local<v8::String> source = js_hooks_source_.Get(context->GetIsolate());
  v8::Local<v8::String> resource_name =
      js_resource_name_.Get(context->GetIsolate());
  v8::Local<v8::Script> script;
  v8::ScriptOrigin origin(resource_name);
  if (!v8::Script::Compile(context, source, &origin).ToLocal(&script))
    return;
  v8::Local<v8::Value> func_as_value = script->Run();
  v8::Local<v8::Function> function;
  if (!gin::ConvertFromV8(context->GetIsolate(), func_as_value, &function))
    return;
  v8::Local<v8::Value> api_hooks = GetJSHookInterface(api_name, context);
  v8::Local<v8::Value> args[] = {api_hooks};
  run_js_.Run(function, context, arraysize(args), args);
}

v8::Local<v8::Object> APIBindingHooks::GetJSHookInterface(
    const std::string& api_name,
    v8::Local<v8::Context> context) {
  gin::PerContextData* per_context_data = gin::PerContextData::From(context);
  DCHECK(per_context_data);
  APIHooksPerContextData* data = static_cast<APIHooksPerContextData*>(
      per_context_data->GetUserData(kExtensionAPIHooksPerContextKey));
  if (!data) {
    auto api_data =
        base::MakeUnique<APIHooksPerContextData>(context->GetIsolate());
    data = api_data.get();
    per_context_data->SetUserData(kExtensionAPIHooksPerContextKey,
                                  api_data.release());
  }

  auto iter = data->hook_interfaces.find(api_name);
  if (iter != data->hook_interfaces.end())
    return iter->second.Get(context->GetIsolate());

  gin::Handle<JSHookInterface> hooks =
      gin::CreateHandle(context->GetIsolate(), new JSHookInterface(api_name));
  CHECK(!hooks.IsEmpty());
  v8::Local<v8::Object> hooks_object = hooks.ToV8().As<v8::Object>();
  data->hook_interfaces[api_name].Reset(context->GetIsolate(), hooks_object);

  return hooks_object;
}

}  // namespace extensions
