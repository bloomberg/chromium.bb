// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api_bindings_system.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "extensions/renderer/api_binding_hooks.h"

namespace extensions {

APIBindingsSystem::Request::Request() {}
APIBindingsSystem::Request::~Request() {}

APIBindingsSystem::APIBindingsSystem(
    const binding::RunJSFunction& call_js,
    const binding::RunJSFunctionSync& call_js_sync,
    const GetAPISchemaMethod& get_api_schema,
    const SendRequestMethod& send_request)
    : request_handler_(call_js),
      event_handler_(call_js),
      call_js_(call_js),
      call_js_sync_(call_js_sync),
      get_api_schema_(get_api_schema),
      send_request_(send_request) {}

APIBindingsSystem::~APIBindingsSystem() {}

v8::Local<v8::Object> APIBindingsSystem::CreateAPIInstance(
    const std::string& api_name,
    v8::Local<v8::Context> context,
    v8::Isolate* isolate,
    const APIBinding::AvailabilityCallback& is_available,
    v8::Local<v8::Object>* hooks_interface_out) {
  std::unique_ptr<APIBinding>& binding = api_bindings_[api_name];
  if (!binding)
    binding = CreateNewAPIBinding(api_name);
  if (hooks_interface_out)
    *hooks_interface_out = binding->GetJSHookInterface(context);
  return binding->CreateInstance(
      context, isolate, &event_handler_, is_available);
}

std::unique_ptr<APIBinding> APIBindingsSystem::CreateNewAPIBinding(
    const std::string& api_name) {
  const base::DictionaryValue& api_schema = get_api_schema_.Run(api_name);

  const base::ListValue* function_definitions = nullptr;
  api_schema.GetList("functions", &function_definitions);
  const base::ListValue* type_definitions = nullptr;
  api_schema.GetList("types", &type_definitions);
  const base::ListValue* event_definitions = nullptr;
  api_schema.GetList("events", &event_definitions);

  // Find the hooks for the API. If none exist, an empty set will be created so
  // we can use JS custom bindings.
  // TODO(devlin): Once all legacy custom bindings are converted, we don't have
  // to unconditionally pass in binding hooks.
  std::unique_ptr<APIBindingHooks> hooks;
  auto iter = binding_hooks_.find(api_name);
  if (iter != binding_hooks_.end()) {
    hooks = std::move(iter->second);
    binding_hooks_.erase(iter);
  } else {
    hooks = base::MakeUnique<APIBindingHooks>(call_js_sync_);
  }

  return base::MakeUnique<APIBinding>(
      api_name, function_definitions, type_definitions, event_definitions,
      base::Bind(&APIBindingsSystem::OnAPICall, base::Unretained(this)),
      std::move(hooks), &type_reference_map_);
}

void APIBindingsSystem::CompleteRequest(int request_id,
                                        const base::ListValue& response) {
  request_handler_.CompleteRequest(request_id, response);
}

void APIBindingsSystem::FireEventInContext(const std::string& event_name,
                                           v8::Local<v8::Context> context,
                                           const base::ListValue& response) {
  event_handler_.FireEventInContext(event_name, context, response);
}

APIBindingHooks* APIBindingsSystem::GetHooksForAPI(
    const std::string& api_name) {
  DCHECK(api_bindings_.empty())
      << "Hook registration must happen before creating any binding instances.";
  std::unique_ptr<APIBindingHooks>& hooks = binding_hooks_[api_name];
  if (!hooks)
    hooks = base::MakeUnique<APIBindingHooks>(call_js_sync_);
  return hooks.get();
}

void APIBindingsSystem::OnAPICall(const std::string& name,
                                  std::unique_ptr<base::ListValue> arguments,
                                  v8::Isolate* isolate,
                                  v8::Local<v8::Context> context,
                                  v8::Local<v8::Function> callback) {
  auto request = base::MakeUnique<Request>();
  if (!callback.IsEmpty()) {
    request->request_id =
        request_handler_.AddPendingRequest(isolate, callback, context);
    request->has_callback = true;
  }
  // TODO(devlin): Query and curry user gestures around.
  request->has_user_gesture = false;
  request->arguments = std::move(arguments);
  request->method_name = name;

  send_request_.Run(std::move(request), context);
}

}  // namespace extensions
