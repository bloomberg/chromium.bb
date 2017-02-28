// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api_binding_hooks.h"

#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/supports_user_data.h"
#include "extensions/renderer/api_signature.h"
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
  explicit JSHookInterface(const std::string& api_name)
      : api_name_(api_name) {}

  static gin::WrapperInfo kWrapperInfo;

  // gin::Wrappable:
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override {
    return Wrappable<JSHookInterface>::GetObjectTemplateBuilder(isolate)
        .SetMethod("setHandleRequest", &JSHookInterface::SetHandleRequest)
        .SetMethod("setUpdateArgumentsPreValidate",
                   &JSHookInterface::SetUpdateArgumentsPreValidate)
        .SetMethod("setUpdateArgumentsPostValidate",
                   &JSHookInterface::SetUpdateArgumentsPostValidate)
        .SetMethod("setCustomCallback", &JSHookInterface::SetCustomCallback);
  }

  void ClearHooks() {
    handle_request_hooks_.clear();
    pre_validation_hooks_.clear();
    post_validation_hooks_.clear();
  }

  v8::Local<v8::Function> GetHandleRequestHook(const std::string& method_name,
                                               v8::Isolate* isolate) const {
    return GetHookFromMap(handle_request_hooks_, method_name, isolate);
  }

  v8::Local<v8::Function> GetPreValidationHook(const std::string& method_name,
                                               v8::Isolate* isolate) const {
    return GetHookFromMap(pre_validation_hooks_, method_name, isolate);
  }

  v8::Local<v8::Function> GetPostValidationHook(const std::string& method_name,
                                                v8::Isolate* isolate) const {
    return GetHookFromMap(post_validation_hooks_, method_name, isolate);
  }

  v8::Local<v8::Function> GetCustomCallback(const std::string& method_name,
                                            v8::Isolate* isolate) const {
    return GetHookFromMap(custom_callback_hooks_, method_name, isolate);
  }

 private:
  using JSHooks = std::map<std::string, v8::Global<v8::Function>>;

  v8::Local<v8::Function> GetHookFromMap(const JSHooks& map,
                                         const std::string& method_name,
                                         v8::Isolate* isolate) const {
    auto iter = map.find(method_name);
    if (iter == map.end())
      return v8::Local<v8::Function>();
    return iter->second.Get(isolate);
  }

  void AddHookToMap(JSHooks* map,
                    v8::Isolate* isolate,
                    const std::string& method_name,
                    v8::Local<v8::Function> hook) {
    std::string qualified_method_name =
        base::StringPrintf("%s.%s", api_name_.c_str(), method_name.c_str());
    v8::Global<v8::Function>& entry = (*map)[qualified_method_name];
    if (!entry.IsEmpty()) {
      NOTREACHED() << "Hooks can only be set once.";
      return;
    }
    entry.Reset(isolate, hook);
  }

  // Adds a hook to handle the implementation of the API method.
  void SetHandleRequest(v8::Isolate* isolate,
                        const std::string& method_name,
                        v8::Local<v8::Function> hook) {
    AddHookToMap(&handle_request_hooks_, isolate, method_name, hook);
  }

  // Adds a hook to update the arguments passed to the API method before we do
  // any kind of validation.
  void SetUpdateArgumentsPreValidate(v8::Isolate* isolate,
                                     const std::string& method_name,
                                     v8::Local<v8::Function> hook) {
    AddHookToMap(&pre_validation_hooks_, isolate, method_name, hook);
  }

  void SetUpdateArgumentsPostValidate(v8::Isolate* isolate,
                                      const std::string& method_name,
                                      v8::Local<v8::Function> hook) {
    AddHookToMap(&post_validation_hooks_, isolate, method_name, hook);
  }

  void SetCustomCallback(v8::Isolate* isolate,
                         const std::string& method_name,
                         v8::Local<v8::Function> hook) {
    AddHookToMap(&custom_callback_hooks_, isolate, method_name, hook);
  }

  std::string api_name_;

  JSHooks handle_request_hooks_;
  JSHooks pre_validation_hooks_;
  JSHooks post_validation_hooks_;
  JSHooks custom_callback_hooks_;

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
      hooks->ClearHooks();
    }
  }

  v8::Isolate* isolate;

  std::map<std::string, v8::Global<v8::Object>> hook_interfaces;
};

gin::WrapperInfo JSHookInterface::kWrapperInfo =
    {gin::kEmbedderNativeGin};

// Gets the v8::Object of the JSHookInterface, optionally creating it if it
// doesn't exist.
v8::Local<v8::Object> GetJSHookInterfaceObject(
    const std::string& api_name,
    v8::Local<v8::Context> context,
    bool should_create) {
  gin::PerContextData* per_context_data = gin::PerContextData::From(context);
  DCHECK(per_context_data);
  APIHooksPerContextData* data = static_cast<APIHooksPerContextData*>(
      per_context_data->GetUserData(kExtensionAPIHooksPerContextKey));
  if (!data) {
    if (!should_create)
      return v8::Local<v8::Object>();

    auto api_data =
        base::MakeUnique<APIHooksPerContextData>(context->GetIsolate());
    data = api_data.get();
    per_context_data->SetUserData(kExtensionAPIHooksPerContextKey,
                                  api_data.release());
  }

  auto iter = data->hook_interfaces.find(api_name);
  if (iter != data->hook_interfaces.end())
    return iter->second.Get(context->GetIsolate());

  if (!should_create)
    return v8::Local<v8::Object>();

  gin::Handle<JSHookInterface> hooks =
      gin::CreateHandle(context->GetIsolate(), new JSHookInterface(api_name));
  CHECK(!hooks.IsEmpty());
  v8::Local<v8::Object> hooks_object = hooks.ToV8().As<v8::Object>();
  data->hook_interfaces[api_name].Reset(context->GetIsolate(), hooks_object);

  return hooks_object;
}

}  // namespace

APIBindingHooks::RequestResult::RequestResult(ResultCode code) : code(code) {}
APIBindingHooks::RequestResult::RequestResult(
    ResultCode code,
    v8::Local<v8::Function> custom_callback)
    : code(code), custom_callback(custom_callback) {}
APIBindingHooks::RequestResult::~RequestResult() {}
APIBindingHooks::RequestResult::RequestResult(const RequestResult& other) =
    default;

APIBindingHooks::APIBindingHooks(const std::string& api_name,
                                 const binding::RunJSFunctionSync& run_js)
    : api_name_(api_name), run_js_(run_js) {}
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

APIBindingHooks::RequestResult APIBindingHooks::RunHooks(
    const std::string& method_name,
    v8::Local<v8::Context> context,
    const APISignature* signature,
    std::vector<v8::Local<v8::Value>>* arguments,
    const APITypeReferenceMap& type_refs) {
  // Easy case: a native custom hook.
  auto request_hooks_iter = request_hooks_.find(method_name);
  if (request_hooks_iter != request_hooks_.end()) {
    RequestResult result =
        request_hooks_iter->second.Run(
            signature, context, arguments, type_refs);
    // Right now, it doesn't make sense to register a request handler that
    // doesn't handle the request.
    DCHECK_NE(RequestResult::NOT_HANDLED, result.code);
    return result;
  }

  // Harder case: looking up a custom hook registered on the context (since
  // these are JS, each context has a separate instance).
  v8::Local<v8::Object> hook_interface_object =
      GetJSHookInterfaceObject(api_name_, context, false);
  if (hook_interface_object.IsEmpty())
    return RequestResult(RequestResult::NOT_HANDLED);

  v8::Isolate* isolate = context->GetIsolate();

  JSHookInterface* hook_interface = nullptr;
  gin::Converter<JSHookInterface*>::FromV8(
      isolate,
      hook_interface_object, &hook_interface);
  CHECK(hook_interface);

  v8::Local<v8::Function> pre_validate_hook =
      hook_interface->GetPreValidationHook(method_name, isolate);
  v8::TryCatch try_catch(isolate);
  if (!pre_validate_hook.IsEmpty()) {
    // TODO(devlin): What to do with the result of this function call? Can it
    // only fail in the case we've already thrown?
    UpdateArguments(pre_validate_hook, context, arguments);
    if (try_catch.HasCaught()) {
      try_catch.ReThrow();
      return RequestResult(RequestResult::THROWN);
    }
  }

  v8::Local<v8::Function> post_validate_hook =
      hook_interface->GetPostValidationHook(method_name, isolate);
  v8::Local<v8::Function> handle_request =
      hook_interface->GetHandleRequestHook(method_name, isolate);
  v8::Local<v8::Function> custom_callback =
      hook_interface->GetCustomCallback(method_name, isolate);

  // If both the post validation hook and the handle request hook are empty,
  // we're done...
  if (post_validate_hook.IsEmpty() && handle_request.IsEmpty())
    return RequestResult(RequestResult::NOT_HANDLED, custom_callback);

  {
    // ... otherwise, we have to validate the arguments.
    std::vector<v8::Local<v8::Value>> parsed_v8_args;
    std::string error;
    bool success = signature->ParseArgumentsToV8(context, *arguments, type_refs,
                                                 &parsed_v8_args, &error);
    if (try_catch.HasCaught()) {
      try_catch.ReThrow();
      return RequestResult(RequestResult::THROWN);
    }
    if (!success)
      return RequestResult(RequestResult::INVALID_INVOCATION);
    arguments->swap(parsed_v8_args);
  }

  if (!post_validate_hook.IsEmpty()) {
    UpdateArguments(post_validate_hook, context, arguments);
    if (try_catch.HasCaught()) {
      try_catch.ReThrow();
      return RequestResult(RequestResult::THROWN);
    }
  }

  if (handle_request.IsEmpty())
    return RequestResult(RequestResult::NOT_HANDLED, custom_callback);

  v8::Global<v8::Value> global_result =
      run_js_.Run(handle_request, context, arguments->size(),
                  arguments->data());
  if (try_catch.HasCaught()) {
    try_catch.ReThrow();
    return RequestResult(RequestResult::THROWN);
  }
  RequestResult result(RequestResult::HANDLED, custom_callback);
  if (!global_result.IsEmpty())
    result.return_value = global_result.Get(isolate);
  return result;
}

void APIBindingHooks::InitializeInContext(v8::Local<v8::Context> context) {
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
  v8::Local<v8::Value> api_hooks = GetJSHookInterface(context);
  v8::Local<v8::Value> args[] = {api_hooks};
  run_js_.Run(function, context, arraysize(args), args);
}

v8::Local<v8::Object> APIBindingHooks::GetJSHookInterface(
    v8::Local<v8::Context> context) {
  return GetJSHookInterfaceObject(api_name_, context, true);
}

v8::Local<v8::Function> APIBindingHooks::GetCustomJSCallback(
    const std::string& name,
    v8::Local<v8::Context> context) {
  v8::Local<v8::Object> hooks =
      GetJSHookInterfaceObject(api_name_, context, false);
  if (hooks.IsEmpty())
    return v8::Local<v8::Function>();
  JSHookInterface* hook_interface = nullptr;
  gin::Converter<JSHookInterface*>::FromV8(context->GetIsolate(), hooks,
                                           &hook_interface);
  CHECK(hook_interface);

  return hook_interface->GetCustomCallback(name, context->GetIsolate());
}

bool APIBindingHooks::UpdateArguments(
    v8::Local<v8::Function> function,
    v8::Local<v8::Context> context,
    std::vector<v8::Local<v8::Value>>* arguments) {
  v8::Global<v8::Value> global_result;
  {
    v8::TryCatch try_catch(context->GetIsolate());
    global_result = run_js_.Run(function, context,
                                arguments->size(), arguments->data());
    if (try_catch.HasCaught()) {
      try_catch.ReThrow();
      return false;
    }
  }
  DCHECK(!global_result.IsEmpty());
  v8::Local<v8::Value> result = global_result.Get(context->GetIsolate());
  std::vector<v8::Local<v8::Value>> new_args;
  if (result.IsEmpty() ||
      !gin::Converter<std::vector<v8::Local<v8::Value>>>::FromV8(
          context->GetIsolate(), result, &new_args)) {
    return false;
  }
  arguments->swap(new_args);
  return true;
}

}  // namespace extensions
